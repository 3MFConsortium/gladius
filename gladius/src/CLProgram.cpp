#include "CLProgram.h"
#include "Profiling.h"
#include "TimeMeasurement.h"
#include "gpgpu.h"
#include <cmrc/cmrc.hpp>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <string>

CMRC_DECLARE(gladius_resources);

namespace gladius
{
    namespace
    {
        // Print detailed program diagnostics for the current device
        std::string makeProgramDiagnostics(const cl::Program & program,
                                           const cl::Device & device,
                                           std::string const & buildOptions,
                                           std::string const & contextHint = {})
        {
            std::ostringstream out;
            try
            {
                auto const devName = device.getInfo<CL_DEVICE_NAME>();
                auto const vendor = device.getInfo<CL_DEVICE_VENDOR>();
                auto const version = device.getInfo<CL_DEVICE_VERSION>();
                out << "[OpenCL Diagnostics]";
                if (!contextHint.empty())
                    out << " (" << contextHint << ")";
                out << "\n  Device      : " << devName << "\n  Vendor      : " << vendor
                    << "\n  Version     : " << version << "\n  Build opts  : "
                    << (buildOptions.empty() ? std::string("<none>") : buildOptions) << "\n";

                // Kernel metadata
                try
                {
                    auto const numKernels = program.getInfo<CL_PROGRAM_NUM_KERNELS>();
                    out << "  Num kernels : " << numKernels << "\n";
                }
                catch (...)
                {
                    // ignore
                }
                try
                {
                    auto const kernelNames = program.getInfo<CL_PROGRAM_KERNEL_NAMES>();
                    if (!kernelNames.empty())
                        out << "  Kernels     : " << kernelNames << "\n";
                }
                catch (...)
                {
                    // ignore
                }

                // Binary type and build status/log
                try
                {
                    cl_program_binary_type binType{};
                    program.getBuildInfo(device, CL_PROGRAM_BINARY_TYPE, &binType);
                    std::string binTypeStr =
                      (binType == CL_PROGRAM_BINARY_TYPE_NONE)              ? "NONE"
                      : (binType == CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT) ? "COMPILED_OBJECT"
                      : (binType == CL_PROGRAM_BINARY_TYPE_LIBRARY)         ? "LIBRARY"
                      : (binType == CL_PROGRAM_BINARY_TYPE_EXECUTABLE)      ? "EXECUTABLE"
                                                                            : "UNKNOWN";
                    out << "  Binary type : " << binTypeStr << "\n";
                }
                catch (...)
                {
                    // ignore
                }

                try
                {
                    auto const status = program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(device);
                    std::string statusStr = (status == CL_BUILD_NONE)          ? "NONE"
                                            : (status == CL_BUILD_ERROR)       ? "ERROR"
                                            : (status == CL_BUILD_SUCCESS)     ? "SUCCESS"
                                            : (status == CL_BUILD_IN_PROGRESS) ? "IN_PROGRESS"
                                                                               : "UNKNOWN";
                    out << "  Build status: " << statusStr << "\n";
                }
                catch (...)
                {
                    // ignore
                }

                try
                {
                    out << "\n  Build log  :\n"
                        << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << "\n";
                }
                catch (...)
                {
                    // ignore
                }
            }
            catch (...)
            {
                // best-effort diagnostics only
            }
            return out.str();
        }

        // Log build status and log output if the build failed
        void logBuildStatusIfFailed(const cl::Program & program,
                                    const cl::Device & device,
                                    events::SharedLogger const & logger)
        {
            try
            {
                auto const status = program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(device);
                if (status != CL_BUILD_SUCCESS)
                {
                    std::string const buildLog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
                    if (logger)
                    {
                        logger->logError("OpenCL: Build failed");
                        if (!buildLog.empty())
                        {
                            logger->logError(std::string("Build log:\n") + buildLog);
                        }
                    }
                    else
                    {
                        std::cerr << "Build failed\n";
                        std::cerr << "\n\n Kernel build info: \n" << buildLog << std::endl;
                    }
                }
            }
            catch (...)
            {
                // best-effort
            }
        }
    }

    void callBackDispatcher(cl_program, void * userData)
    {
        if (userData == nullptr)
        {
            return;
        }

        const auto data = reinterpret_cast<CallBackUserData *>(userData);
        bool buildWasSuccessful = false;
        if ((data->program != nullptr) && (data->computeContext != nullptr))
        {
            auto const status = data->program->getBuildInfo<CL_PROGRAM_BUILD_STATUS>(
              data->computeContext->GetDevice());
            buildWasSuccessful = (status == CL_BUILD_SUCCESS);
        }

        if ((data->callBack != nullptr) && data->callBack->has_value())
        {
            data->callBack->value()();
        }

        if ((data->sender != nullptr) && buildWasSuccessful)
        {
            data->sender->compilationFinishedHandler();
        }
    }

    CLProgram::CLProgram(SharedComputeContext context)
        : m_program(nullptr)
        , m_ComputeContext(context)
    {
    }

    void CLProgram::loadSourceFromFile(const FileNames & filenames)
    {
        ProfileFunction

          std::scoped_lock lock(m_compilationMutex);
        const auto fs = cmrc::gladius_resources::get_filesystem();

        m_sources.reserve(m_sources.size() + filenames.size());
        for (auto & filename : filenames)
        {
            const auto resourceFilename = std::string("src/kernel/" + filename);
            if (!fs.exists(resourceFilename) || !fs.is_file(resourceFilename))
            {
                if (m_logger)
                {
                    m_logger->logError(std::string("Missing file in resources: ") +
                                       resourceFilename);
                }
                else
                {
                    std::cerr << "missing file in resources: " << resourceFilename << "\n";
                }
                throw std::runtime_error("missing file in resources: " + resourceFilename);
            }

            auto file = fs.open(resourceFilename);
            m_sources.emplace_back(std::string(file.begin(), file.end()));
        }
    }

    void CLProgram::addSource(const std::string & source)
    {
        ProfileFunction std::scoped_lock lock(m_compilationMutex);
        m_sources.emplace_back(source);
    }

    void CLProgram::dumpSource(std::filesystem::path filename)
    {
        ProfileFunction std::ofstream file;
        file.open(filename);
        for (auto src : m_sources)
        {
            file << src;
        }
        file.close();
    }

    void CLProgram::setKernelReplacements(SharedKernelReplacements kernelReplacements)
    {
        ProfileFunction m_kernelReplacements = std::move(kernelReplacements);
    }

    bool CLProgram::isCompilationInProgress() const
    {
        return m_isCompilationInProgress;
    }

    void CLProgram::clearSources()
    {
        ProfileFunction std::scoped_lock lock(m_compilationMutex);
        m_sources.clear();
    }

    void CLProgram::setUseFastRelaxedMath(bool useFastRelaxedMath)
    {
        m_useFastRelaxedMath = useFastRelaxedMath;
    }

    void CLProgram::compileAsLib()
    {
        ProfileFunction;

        std::scoped_lock lock(m_compilationMutex);
        const std::vector<cl::Program> progsToLink{{m_ComputeContext->GetContext(), m_sources}};
        const auto arguments = generateDefineSymbol();
        for (auto & prog : progsToLink)
        {
            prog.compile(arguments.c_str());
            logBuildStatusIfFailed(prog, m_ComputeContext->GetDevice(), m_logger);
        }
        m_lib = std::make_unique<cl::Program>(
          linkProgram(progsToLink, "-create-library -enable-link-options"));
        logBuildStatusIfFailed(*m_lib, m_ComputeContext->GetDevice(), m_logger);
    }

    std::string CLProgram::generateDefineSymbol() const
    {
        ProfileFunction std::stringstream args;

        // Allow overriding build flags for debugging via environment variable
        const char * debugFlags = std::getenv("GLADIUS_OPENCL_BUILD_FLAGS");
        if (debugFlags && std::string(debugFlags).size() > 0)
        {
            args << ' ' << debugFlags;
        }
        else
        {
            if (m_useFastRelaxedMath)
            {
                args << " -cl-fast-relaxed-math";
            }
        }

        for (const auto & symbol : m_symbols)
        {
            args << " -D " << symbol;
        }
        args << m_additionalDefine;
        return args.str();
    }

    void CLProgram::applyAllKernelReplacements()
    {
        ProfileFunction if (!m_kernelReplacements)
        {
            return;
        }
        for (auto & source : m_sources)
        {
            applyKernelReplacements(source, *m_kernelReplacements);
        }
    }

    size_t CLProgram::computeHash() const
    {
        ProfileFunction

          size_t hash = 0;

        // Calculate the hash of the source, device name and additional defines
        for (const auto & source : m_sources)
        {
            hash_combine(hash, std::hash<std::string>{}(source));
        }

        hash_combine(
          hash, std::hash<std::string>{}(m_ComputeContext->GetDevice().getInfo<CL_DEVICE_NAME>()));
        hash_combine(hash, std::hash<std::string>{}(generateDefineSymbol()));

        // kernel replacements
        if (m_kernelReplacements)
        {
            for (const auto & replacement : *m_kernelReplacements)
            {
                const auto & [search, replace] = replacement;
                hash_combine(hash, std::hash<std::string>{}(search));
                hash_combine(hash, std::hash<std::string>{}(replace));
            }
        }

        return hash;
    }

    size_t numberOfLines(cl::Program::Sources const & sources)
    {
        size_t lines = 0;
        for (auto const & source : sources)
        {
            lines += std::count(source.begin(), source.end(), '\n');
        }
        return lines;
    }

    void CLProgram::compile(BuildCallBack & callBack)
    {
        ProfileFunction

          std::scoped_lock lock(m_compilationMutex);

        applyAllKernelReplacements();

        if (m_hashLastSuccessfulCompilation != 0 &&
            m_hashLastSuccessfulCompilation == computeHash())
        {
            m_valid = true;
            m_isCompilationInProgress = false;
            callBackDispatcher(nullptr, &m_callBackUserData);
            return;
        }
        m_valid = false;
        m_isCompilationInProgress = true;

        m_program =
          std::make_unique<cl::Program>(cl::Program(m_ComputeContext->GetContext(), m_sources));

        const auto arguments = generateDefineSymbol();

        if (m_logger)
        {
            m_logger->logInfo("OpenCL: Compiling program (" +
                              std::to_string(numberOfLines(m_sources)) + " lines)");
        }

        m_callBackUserData.computeContext = m_ComputeContext.get();
        m_callBackUserData.callBack = &callBack;
        m_callBackUserData.sender = this;
        m_callBackUserData.program = m_program.get();
        try
        {
            // write to file for debugging
            // dumpSource("debug.cl");
            m_program->build({m_ComputeContext->GetDevice()}, arguments.c_str(), nullptr, nullptr);
            m_hashLastSuccessfulCompilation = computeHash();

            // Always print build status (and logs on failure)
            logBuildStatusIfFailed(*m_program, m_ComputeContext->GetDevice(), m_logger);
        }
        catch (const std::exception & e)
        {
            m_ComputeContext->invalidate();
            const auto diag = makeProgramDiagnostics(
              *m_program, m_ComputeContext->GetDevice(), arguments, "compile(build)");
            if (m_logger)
            {
                m_logger->logError(std::string("OpenCL build failed: ") + e.what());
                m_logger->logError(diag);
            }
            else
            {
                std::cerr << e.what() << '\n' << diag << '\n';
            }
        }

        m_isCompilationInProgress = false;

        callBackDispatcher(nullptr, &m_callBackUserData);
    }

    void CLProgram::compileNonBlocking(BuildCallBack & callBack)
    {
        ProfileFunction m_kernelCompilation = std::async([&]() { this->compile(callBack); });
    }

    void CLProgram::buildWithLib(BuildCallBack & callBack)
    {
        ProfileFunction

          std::scoped_lock lock(m_compilationMutex);
        applyAllKernelReplacements();

        auto const currentHash = computeHash();

        if (m_hashLastSuccessfulCompilation == currentHash)
        {
            m_valid = true;
            callBackDispatcher(nullptr, &m_callBackUserData);
            return;
        }
        m_valid = false;

        std::vector<cl::Program> progsToLink{{m_ComputeContext->GetContext(), m_sources}};
        const auto arguments = generateDefineSymbol();
        for (auto & prog : progsToLink)
        {
            try
            {
                prog.compile(arguments.c_str());
                m_hashLastSuccessfulCompilation = currentHash;
                m_valid = true;
            }
            catch (const std::exception & e)
            {
                m_ComputeContext->invalidate();
                if (m_logger)
                {
                    m_logger->logError(std::string("OpenCL compile failed: ") + e.what());
                    const auto diag = makeProgramDiagnostics(
                      prog, m_ComputeContext->GetDevice(), arguments, "compile(lib)");
                    m_logger->logError(diag);
                }
                else
                {
                    std::cerr << e.what() << '\n';
                }
            }

            logBuildStatusIfFailed(prog, m_ComputeContext->GetDevice(), m_logger);
        }
        progsToLink.push_back(*m_lib);

        m_program = std::make_unique<cl::Program>(linkProgram(progsToLink));
        logBuildStatusIfFailed(*m_program, m_ComputeContext->GetDevice(), m_logger);

        m_callBackUserData.computeContext = m_ComputeContext.get();
        m_callBackUserData.callBack = &callBack;
        m_callBackUserData.sender = this;
        m_callBackUserData.program = m_program.get();
        callBackDispatcher(nullptr, &m_callBackUserData);
    }

    void CLProgram::buildWithLibNonBlocking(BuildCallBack & callBack)
    {
        m_kernelCompilation = std::async([&]() { this->compile(callBack); });
    }

    void CLProgram::finishCompilation()
    {
        if (m_kernelCompilation.valid())
        {
            m_kernelCompilation.get();
        }
    }

    void CLProgram::loadAndCompileSource(const FileNames & filenames,
                                         const std::string & dynamicSource,
                                         BuildCallBack & callBack)
    {
        ProfileFunction;
        m_valid = false;
        loadSourceFromFile(filenames);
        addSource(dynamicSource);
        compileNonBlocking(callBack);
    }

    void CLProgram::buildFromSourceAndLinkWithLibNonBlocking(const FileNames & filenames,
                                                             const std::string & dynamicSource,
                                                             BuildCallBack & callBack)
    {
        ProfileFunction;

        m_valid = false;
        loadSourceFromFile(filenames);
        addSource(dynamicSource);
        compileNonBlocking(callBack);
    }

    void CLProgram::buildFromSourceAndLinkWithLib(const FileNames & filenames,
                                                  const std::string & dynamicSource,
                                                  BuildCallBack & callBack)
    {
        ProfileFunction;

        m_valid = false;
        loadSourceFromFile(filenames);
        addSource(dynamicSource);
        compile(callBack);
    }

    void CLProgram::loadAndCompileLib(const FileNames & filenames)
    {
        ProfileFunction;

        m_valid = false;
        CL_ERROR(m_ComputeContext->GetQueue().finish());
        loadSourceFromFile(filenames);
        compileAsLib();
    }

    bool CLProgram::isValid() const
    {
        return m_valid;
    }

    void CLProgram::compilationFinishedHandler()
    {
        m_valid = true;
        m_kernels.clear();
    }

    void CLProgram::addSymbol(const std::string & symbol)
    {
        m_symbols.insert(symbol);
    }

    void CLProgram::removeSymbol(const std::string & symbol)
    {
        m_symbols.erase(symbol);
    }

    void CLProgram::setAdditionalDefine(std::string define)
    {
        m_additionalDefine = std::move(define);
    }

    void applyKernelReplacements(std::string & source,
                                 const KernelReplacements & kernelReplacements)
    {
        ProfileFunction;

        for (const auto & replacement : kernelReplacements)
        {
            const auto & [search, replace] = replacement;
            size_t pos = 0;
            while ((pos = source.find(search, pos)) != std::string::npos)
            {
                source.replace(pos, search.length(), replace);
                pos += replace.length();
            }
        }
    }
}
