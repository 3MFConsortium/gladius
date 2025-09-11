#include "CLProgram.h"
#include "Profiling.h"
#include "TimeMeasurement.h"
#include "gpgpu.h"
#include <boost/functional/hash.hpp>
#include <cmrc/cmrc.hpp>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <cstdlib>
#endif

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
#ifdef _WIN32
        char * debugFlags = nullptr;
        size_t len = 0;
        _dupenv_s(&debugFlags, &len, "GLADIUS_OPENCL_BUILD_FLAGS");
        if (debugFlags && std::string(debugFlags).size() > 0)
        {
            args << ' ' << debugFlags;
        }
        if (debugFlags)
        {
            free(debugFlags);
        }
#else
        const char * debugFlags = std::getenv("GLADIUS_OPENCL_BUILD_FLAGS");
        if (debugFlags && std::string(debugFlags).size() > 0)
        {
            args << ' ' << debugFlags;
        }
#endif
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
            boost::hash_combine(hash, std::hash<std::string>{}(source));
        }

        boost::hash_combine(
          hash, std::hash<std::string>{}(m_ComputeContext->GetDevice().getInfo<CL_DEVICE_NAME>()));
        boost::hash_combine(hash, std::hash<std::string>{}(generateDefineSymbol()));

        // kernel replacements
        if (m_kernelReplacements)
        {
            for (const auto & replacement : *m_kernelReplacements)
            {
                const auto & [search, replace] = replacement;
                boost::hash_combine(hash, std::hash<std::string>{}(search));
                boost::hash_combine(hash, std::hash<std::string>{}(replace));
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

        auto currentHash = computeHash();

        // Check if we can load from binary cache first
        if (!m_cacheDirectory.empty() && loadProgramFromCache(currentHash))
        {
            std::cout << "CLProgram: Loaded program from binary cache (hash: " << currentHash << ")"
                      << std::endl;
            m_hashLastSuccessfulCompilation = currentHash;
            m_valid = true;
            m_isCompilationInProgress = false;

            // Set up callback user data properly for cached program
            m_callBackUserData.computeContext = m_ComputeContext.get();
            m_callBackUserData.callBack = &callBack;
            m_callBackUserData.sender = this;
            m_callBackUserData.program = m_program.get();

            callBackDispatcher(nullptr, &m_callBackUserData);
            return;
        }

        if (m_hashLastSuccessfulCompilation != 0 && m_hashLastSuccessfulCompilation == currentHash)
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
            m_hashLastSuccessfulCompilation = currentHash;

            // Save to binary cache if compilation succeeded
            if (!m_cacheDirectory.empty())
            {
                saveProgramToCache(currentHash);
                std::cout << "CLProgram: Saved program to binary cache (hash: " << currentHash
                          << ")" << std::endl;
            }

            // Always print build status (and logs on failure)
            logBuildStatusIfFailed(*m_program, m_ComputeContext->GetDevice(), m_logger);
        }
        catch (const std::exception & e)
        {
            m_ComputeContext->invalidate("Program build/compilation failed in CLProgram");
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
                m_ComputeContext->invalidate("Program library compilation failed in CLProgram");
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

    void CLProgram::setCacheDirectory(const std::filesystem::path & path)
    {
        std::cout << "CLProgram::setCacheDirectory called with path: " << path << std::endl;
        m_cacheDirectory = path;
        if (!path.empty() && !std::filesystem::exists(path))
        {
            std::filesystem::create_directories(path);
            std::cout << "CLProgram: Created cache directory: " << path << std::endl;
        }
    }

    void CLProgram::clearCache()
    {
        if (m_cacheDirectory.empty())
            return;

        try
        {
            for (const auto & entry : std::filesystem::directory_iterator(m_cacheDirectory))
            {
                if (entry.path().extension() == ".clcache")
                {
                    std::filesystem::remove(entry.path());
                }
            }
            std::cout << "CLProgram: Cleared cache directory: " << m_cacheDirectory << std::endl;
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logWarning("Failed to clear cache: " + std::string(e.what()));
            }
        }
    }

    bool CLProgram::loadProgramFromCache(size_t hash)
    {
        if (m_cacheDirectory.empty())
            return false;

        auto cachePath = m_cacheDirectory / (std::to_string(hash) + ".clcache");
        if (!std::filesystem::exists(cachePath))
            return false;

        try
        {
            std::ifstream file(cachePath, std::ios::binary);
            if (!file.is_open())
                return false;

            // Read device signature
            size_t deviceSigSize;
            file.read(reinterpret_cast<char *>(&deviceSigSize), sizeof(deviceSigSize));
            std::string cachedDeviceSignature(deviceSigSize, '\0');
            file.read(cachedDeviceSignature.data(), deviceSigSize);

            // Verify device signature matches
            if (cachedDeviceSignature != getDeviceSignature())
            {
                std::cout << "CLProgram: Cache device signature mismatch, ignoring cache"
                          << std::endl;
                return false;
            }

            // Read binary data
            size_t binarySize;
            file.read(reinterpret_cast<char *>(&binarySize), sizeof(binarySize));
            std::vector<unsigned char> binary(binarySize);
            file.read(reinterpret_cast<char *>(binary.data()), binarySize);

            // Create program from binary
            m_program =
              std::make_unique<cl::Program>(m_ComputeContext->GetContext(),
                                            std::vector<cl::Device>{m_ComputeContext->GetDevice()},
                                            std::vector<std::vector<unsigned char>>{binary});

            // Build the program (required even when loading from binary)
            try
            {
                m_program->build({m_ComputeContext->GetDevice()});
                std::cout << "CLProgram: Successfully loaded and built program from cache"
                          << std::endl;
                return true;
            }
            catch (const std::exception & e)
            {
                std::cout << "CLProgram: Failed to build cached program: " << e.what() << std::endl;
                return false;
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logWarning("Failed to load from cache: " + std::string(e.what()));
            }
            return false;
        }
    }

    void CLProgram::saveProgramToCache(size_t hash)
    {
        if (m_cacheDirectory.empty() || !m_program)
            return;

        try
        {
            // Get program binaries
            auto binaries = m_program->getInfo<CL_PROGRAM_BINARIES>();
            if (binaries.empty() || binaries[0].empty())
                return;

            auto cachePath = m_cacheDirectory / (std::to_string(hash) + ".clcache");
            std::ofstream file(cachePath, std::ios::binary);
            if (!file.is_open())
                return;

            // Write device signature
            std::string deviceSig = getDeviceSignature();
            size_t deviceSigSize = deviceSig.size();
            file.write(reinterpret_cast<const char *>(&deviceSigSize), sizeof(deviceSigSize));
            file.write(deviceSig.data(), deviceSigSize);

            // Write binary data
            size_t binarySize = binaries[0].size();
            file.write(reinterpret_cast<const char *>(&binarySize), sizeof(binarySize));
            file.write(reinterpret_cast<const char *>(binaries[0].data()), binarySize);

            std::cout << "CLProgram: Saved program binary to cache: " << cachePath << std::endl;
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logWarning("Failed to save to cache: " + std::string(e.what()));
            }
        }
    }

    std::string CLProgram::getDeviceSignature() const
    {
        try
        {
            auto device = m_ComputeContext->GetDevice();
            std::string deviceName = device.getInfo<CL_DEVICE_NAME>();
            std::string driverVersion = device.getInfo<CL_DRIVER_VERSION>();
            std::string deviceVersion = device.getInfo<CL_DEVICE_VERSION>();

            return deviceName + "|" + driverVersion + "|" + deviceVersion;
        }
        catch (...)
        {
            return "unknown_device";
        }
    }
}
