#include "CLProgram.h"
#include "Profiling.h"
#include "TimeMeasurement.h"
#include "gpgpu.h"
#include <boost/functional/hash.hpp>
#include <cmrc/cmrc.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream> // TODO: Remove if no other translation units rely on side-effects; kept temporarily for potential error output fallbacks
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
        // --- Diagnostic helpers (OpenCL source/options dump) ---
        inline bool isOclDumpEnabled()
        {
            // try
            // {
            //     if (auto const * env = std::getenv("GLADIUS_OCL_DUMP"))
            //     {
            //         std::string v(env);
            //         return !v.empty() && v != "0" && v != "false" && v != "False";
            //     }
            // }
            // catch (...) {}
            return true;
        }

        inline std::filesystem::path ensureDumpDir(std::filesystem::path const & cacheDir)
        {
            std::filesystem::path dir = cacheDir.empty()
                                          ? (std::filesystem::current_path() / "ocl_dumps")
                                          : (cacheDir / "ocl_dumps");
            try
            {
                if (!std::filesystem::exists(dir))
                {
                    std::filesystem::create_directories(dir);
                }
            }
            catch (...)
            {
            }
            return dir;
        }

        inline void dumpTextFile(std::filesystem::path const & dir,
                                 std::string const & filename,
                                 std::string const & content)
        {
            try
            {
                std::ofstream f(dir / filename, std::ios::out | std::ios::trunc);
                if (!f.is_open())
                    return;
                f << content;
            }
            catch (...)
            {
            }
        }

        inline void dumpSources(std::filesystem::path const & dir,
                                std::string const & filename,
                                std::vector<std::string> const & sources)
        {
            try
            {
                std::ofstream f(dir / filename, std::ios::out | std::ios::trunc);
                if (!f.is_open())
                    return;
                // Add small header for readability
                f << "// OpenCL source dump (" << filename << ")\n";
                for (size_t i = 0; i < sources.size(); ++i)
                {
                    f << "\n// ---- Source chunk " << i << " ----\n\n";
                    f << sources[i];
                    if (!sources[i].empty() && sources[i].back() != '\n')
                        f << '\n';
                }
            }
            catch (...)
            {
            }
        }

        inline void dumpBuildOptions(std::filesystem::path const & dir,
                                     std::string const & filename,
                                     std::string const & options,
                                     std::string const & deviceSignature)
        {
            std::ostringstream ss;
            ss << "# Build Options\n"
               << (options.empty() ? std::string("<none>") : options) << "\n\n# Device\n"
               << deviceSignature << "\n";
            dumpTextFile(dir, filename, ss.str());
        }

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
                        std::cerr << "OpenCL: Build failed\n";
                        if (!buildLog.empty())
                        {
                            std::cerr << "Build log:\n" << buildLog << "\n";
                        }
                    }
                }
            }
            catch (...)
            {
                // best-effort
            }
        }
    }

    void validateBuildStatus(const cl::Program & program,
                             const cl::Device & device,
                             events::SharedLogger const & logger)
    {
        try
        {
            auto const status = program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(device);
            if (status != CL_BUILD_SUCCESS)
            {
                std::string const buildLog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
                std::string errorMsg = "OpenCL program build failed";
                if (!buildLog.empty())
                {
                    errorMsg += ": " + buildLog;
                }

                if (logger)
                {
                    logger->logError(errorMsg);
                }
                // Suppress direct stderr output when logger unavailable (MCP stdio cleanliness)

                throw std::runtime_error(errorMsg);
            }
        }
        catch (const std::runtime_error &)
        {
            throw; // Re-throw runtime errors (our build failures)
        }
        catch (...)
        {
            // best-effort for OpenCL API errors, but don't fail
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

        m_staticSources.reserve(m_staticSources.size() + filenames.size());
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
                // Suppress direct stderr output when logger unavailable
                throw std::runtime_error("missing file in resources: " + resourceFilename);
            }

            auto file = fs.open(resourceFilename);
            m_staticSources.emplace_back(std::string(file.begin(), file.end()));
        }

        // Update combined sources for compatibility
        m_sources.clear();
        m_sources.insert(m_sources.end(), m_staticSources.begin(), m_staticSources.end());
        m_sources.insert(m_sources.end(), m_dynamicSources.begin(), m_dynamicSources.end());
    }

    void CLProgram::addSource(const std::string & source)
    {
        ProfileFunction std::scoped_lock lock(m_compilationMutex);
        m_sources.emplace_back(source);
    }

    void CLProgram::addDynamicSource(const std::string & source)
    {
        ProfileFunction std::scoped_lock lock(m_compilationMutex);
        m_dynamicSources.emplace_back(source);

        // Update combined sources for compatibility
        m_sources.clear();
        m_sources.insert(m_sources.end(), m_staticSources.begin(), m_staticSources.end());
        m_sources.insert(m_sources.end(), m_dynamicSources.begin(), m_dynamicSources.end());
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
            validateBuildStatus(prog, m_ComputeContext->GetDevice(), m_logger);
        }
        m_lib = std::make_unique<cl::Program>(
          cl::linkProgram(progsToLink, "-create-library -enable-link-options"));
        validateBuildStatus(*m_lib, m_ComputeContext->GetDevice(), m_logger);
    }

    std::string CLProgram::generateDefineSymbol() const
    {
        ProfileFunction std::stringstream args;

        if (m_useFastRelaxedMath)
        {
            args << " -cl-fast-relaxed-math";
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

        // Apply replacements to the actual source vectors that will be used
        // for the two-level compilation and hashing.
        for (auto & source : m_staticSources)
        {
            applyKernelReplacements(source, *m_kernelReplacements);
        }
        for (auto & source : m_dynamicSources)
        {
            applyKernelReplacements(source, *m_kernelReplacements);
        }

        // Rebuild the combined sources for single-level fallback and other code paths
        m_sources.clear();
        m_sources.insert(m_sources.end(), m_staticSources.begin(), m_staticSources.end());
        m_sources.insert(m_sources.end(), m_dynamicSources.begin(), m_dynamicSources.end());
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

        // Include the dynamic interface header content in the hash as it is
        // compiled together with dynamic sources during linking
        try
        {
            auto const fs = cmrc::gladius_resources::get_filesystem();
            auto const headerPath = std::string("src/kernel/dynamic_interface.h");
            if (fs.exists(headerPath) && fs.is_file(headerPath))
            {
                auto file = fs.open(headerPath);
                std::string ifaceSrc(file.begin(), file.end());
                if (m_kernelReplacements)
                {
                    auto tmp = ifaceSrc; // applyKernelReplacements works in-place; keep ifaceSrc
                    applyKernelReplacements(tmp, *m_kernelReplacements);
                    ifaceSrc.swap(tmp);
                }
                boost::hash_combine(hash, std::hash<std::string>{}(ifaceSrc));
            }
        }
        catch (...)
        {
            // best effort; if we cannot load the header, skip adding it
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

    size_t CLProgram::computeStaticHash() const
    {
        ProfileFunction

          size_t hash = 0;

        // Calculate the hash of static sources, device name and additional defines
        for (const auto & source : m_staticSources)
        {
            boost::hash_combine(hash, std::hash<std::string>{}(source));
        }

        boost::hash_combine(
          hash, std::hash<std::string>{}(m_ComputeContext->GetDevice().getInfo<CL_DEVICE_NAME>()));

        // Also include the dynamic interface header in the static hash to tie the
        // cached static library to the interface/ABI used by the dynamic part.
        try
        {
            auto const fs = cmrc::gladius_resources::get_filesystem();
            auto const headerPath = std::string("src/kernel/dynamic_interface.h");
            if (fs.exists(headerPath) && fs.is_file(headerPath))
            {
                auto file = fs.open(headerPath);
                std::string ifaceSrc(file.begin(), file.end());
                if (m_kernelReplacements)
                {
                    auto tmp = ifaceSrc;
                    applyKernelReplacements(tmp, *m_kernelReplacements);
                    ifaceSrc.swap(tmp);
                }
                boost::hash_combine(hash, std::hash<std::string>{}(ifaceSrc));
            }
        }
        catch (...)
        {
            // best effort; if we cannot load the header, skip adding it
        }

        // Use unified defines for static hash computation to guarantee consistency
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

    size_t CLProgram::computeDynamicHash() const
    {
        ProfileFunction

          size_t hash = 0;

        // Calculate the hash of dynamic sources
        for (const auto & source : m_dynamicSources)
        {
            boost::hash_combine(hash, std::hash<std::string>{}(source));
        }
        // Ensure dynamic hash also reflects the same compile-time defines and replacements
        boost::hash_combine(hash, std::hash<std::string>{}(generateDefineSymbol()));
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

        // Compute separate hashes for static and dynamic parts
        auto staticHash = computeStaticHash();
        auto dynamicHash = computeDynamicHash();
        auto currentHash = computeHash(); // Keep for fallback

        // Dump prepared static/dynamic inputs (post-replacements) and options if requested
        if (isOclDumpEnabled())
        {
            auto const dumpDir = ensureDumpDir(m_cacheDirectory);
            try
            {
                dumpSources(
                  dumpDir, "static_" + std::to_string(staticHash) + ".cl", m_staticSources);
                // dynamic will be dumped later when we build the combined dynamic vector
                dumpBuildOptions(dumpDir,
                                 "options_common_" + std::to_string(staticHash) + "_" +
                                   std::to_string(dynamicHash) + ".txt",
                                 generateDefineSymbol(),
                                 getDeviceSignature());
            }
            catch (...)
            {
            }
        }

        // Check if we can load complete linked program from cache first
        if (m_cacheEnabled && !m_cacheDirectory.empty() && !m_staticSources.empty() &&
            !m_dynamicSources.empty() && loadLinkedProgramFromCache(staticHash, dynamicHash))
        {
            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Loaded linked program from cache (static: " +
                                  std::to_string(staticHash) +
                                  ", dynamic: " + std::to_string(dynamicHash) + ")");
            }
            m_hashLastSuccessfulCompilation = currentHash;
            m_valid = true;
            m_isCompilationInProgress = false;
            m_kernels.clear(); // Clear stale kernels when using cached program

            // Set up callback user data properly for cached program
            m_callBackUserData.computeContext = m_ComputeContext.get();
            m_callBackUserData.callBack = &callBack;
            m_callBackUserData.sender = this;
            m_callBackUserData.program = m_program.get();

            callBackDispatcher(nullptr, &m_callBackUserData);
            return;
        }

        // Fallback: Check if we can load from old single-level binary cache
        if (m_cacheEnabled && !m_cacheDirectory.empty() && loadProgramFromCache(currentHash))
        {
            if (m_logger)
            {
                m_logger->logInfo(
                  "CLProgram: Loaded program from single-level binary cache (hash: " +
                  std::to_string(currentHash) + ")");
            }
            m_hashLastSuccessfulCompilation = currentHash;
            m_valid = true;
            m_isCompilationInProgress = false;
            m_kernels.clear(); // Clear stale kernels when using cached program

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
            m_kernels.clear(); // Clear stale kernels when reusing existing program
            callBackDispatcher(nullptr, &m_callBackUserData);
            return;
        }

        m_valid = false;
        m_isCompilationInProgress = true;

        // Two-level compilation: try to load/compile static library, then link with dynamic
        if (m_cacheEnabled && !m_cacheDirectory.empty() && !m_staticSources.empty() &&
            !m_dynamicSources.empty())
        {
            cl::Program staticLibrary;
            bool staticLoaded = loadStaticLibraryFromCache(staticHash, staticLibrary);

            if (!staticLoaded)
            {
                // Compile static library from source
                if (m_logger)
                {
                    m_logger->logInfo("CLProgram: Compiling static library from source");
                }
                try
                {
                    staticLibrary = cl::Program(m_ComputeContext->GetContext(), m_staticSources);
                    // CRITICAL FIX: Use FULL symbols for compilation, not just static ones
                    // The cache key separation is handled in computeStaticHash(), not here
                    const auto arguments = generateDefineSymbol();
                    staticLibrary.compile(arguments.c_str());

                    // Save compiled static library to cache
                    saveStaticLibraryToCache(staticHash, staticLibrary);
                    if (m_logger)
                    {
                        m_logger->logInfo("CLProgram: Compiled and cached static library (hash: " +
                                          std::to_string(staticHash) + ") with args: " + arguments);
                    }
                }
                catch (const std::exception & e)
                {
                    if (m_logger)
                    {
                        m_logger->logError("CLProgram: Failed to compile static library: " +
                                           std::string(e.what()));
                    }
                    // Fall back to single-level compilation
                    goto single_level_compile;
                }
            }
            else
            {
                if (m_logger)
                {
                    m_logger->logInfo("CLProgram: Loaded static library from cache (hash: " +
                                      std::to_string(staticHash) + ")");
                }
            }

            // Compile dynamic source and link with static library
            try
            {
                cl::Program::Sources dynamicSourcesCombined;

                // Synthetic interface: only forward declarations & prototypes actually used
                // Avoids pulling full implementations a second time (which would cause
                // duplicate symbol definitions when linking with static library).
                std::string dynamicInterfaceSource;
                try
                {
                    // Load header from embedded resources (preferred maintenance path)
                    auto const fs = cmrc::gladius_resources::get_filesystem();
                    auto const headerPath = std::string("src/kernel/dynamic_interface.h");
                    if (fs.exists(headerPath) && fs.is_file(headerPath))
                    {
                        auto file = fs.open(headerPath);
                        dynamicInterfaceSource.assign(file.begin(), file.end());
                    }
                    else
                    {
                        throw std::runtime_error("dynamic_interface.h not found in resources");
                    }
                }
                catch (const std::exception & e)
                {

                    if (m_logger)
                    {
                        m_logger->logError(std::string("Failed to load dynamic interface: ") +
                                           e.what());
                    }
                }
                // Apply kernel replacements to the interface header for consistency
                if (m_kernelReplacements)
                {
                    applyKernelReplacements(dynamicInterfaceSource, *m_kernelReplacements);
                }
                dynamicSourcesCombined.emplace_back(dynamicInterfaceSource);
                dynamicSourcesCombined.insert(
                  dynamicSourcesCombined.end(), m_dynamicSources.begin(), m_dynamicSources.end());

                // Dump dynamic phase inputs if requested
                if (isOclDumpEnabled())
                {
                    auto const dumpDir = ensureDumpDir(m_cacheDirectory);
                    try
                    {
                        dumpSources(dumpDir,
                                    "dynamic_" + std::to_string(dynamicHash) + ".cl",
                                    dynamicSourcesCombined);
                        dumpBuildOptions(dumpDir,
                                         "options_dynamic_" + std::to_string(dynamicHash) + ".txt",
                                         generateDefineSymbol(),
                                         getDeviceSignature());
                    }
                    catch (...)
                    {
                    }
                }

                cl::Program dynamicProgram(m_ComputeContext->GetContext(), dynamicSourcesCombined);
                const auto arguments = generateDefineSymbol();
                dynamicProgram.compile(arguments.c_str());
                validateBuildStatus(dynamicProgram, m_ComputeContext->GetDevice(), m_logger);

                // Link static library with dynamic program
                std::vector<cl::Program> programs = {staticLibrary, dynamicProgram};
                m_program = std::make_unique<cl::Program>(cl::linkProgram(programs, ""));
                validateBuildStatus(*m_program, m_ComputeContext->GetDevice(), m_logger);

                if (m_logger)
                {
                    m_logger->logInfo(
                      "CLProgram: Successfully linked static library with dynamic program");
                }

                // Save linked program to cache
                saveLinkedProgramToCache(staticHash, dynamicHash);
                if (m_logger)
                {
                    m_logger->logInfo("CLProgram: Saved linked program to cache");
                }

                m_hashLastSuccessfulCompilation = currentHash;
                m_valid = true;
                m_kernels.clear();
                m_isCompilationInProgress = false;

                // Set up callback
                m_callBackUserData.computeContext = m_ComputeContext.get();
                m_callBackUserData.callBack = &callBack;
                m_callBackUserData.sender = this;
                m_callBackUserData.program = m_program.get();

                callBackDispatcher(nullptr, &m_callBackUserData);
                return;
            }
            catch (const std::exception & e)
            {
                if (m_logger)
                {
                    m_logger->logError("CLProgram: Failed to compile/link dynamic program: " +
                                       std::string(e.what()));
                }
                // Fall back to single-level compilation
                goto single_level_compile;
            }
        }

    single_level_compile:
        // Single-level compilation fallback
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
        // Dump single-level inputs if requested
        if (isOclDumpEnabled())
        {
            auto const dumpDir = ensureDumpDir(m_cacheDirectory);
            try
            {
                dumpSources(
                  dumpDir, "singlelevel_" + std::to_string(currentHash) + ".cl", m_sources);
                dumpBuildOptions(dumpDir,
                                 "options_singlelevel_" + std::to_string(currentHash) + ".txt",
                                 arguments,
                                 getDeviceSignature());
            }
            catch (...)
            {
            }
        }

        try
        {
            // write to file for debugging
            // dumpSource("debug.cl");
            m_program->build({m_ComputeContext->GetDevice()}, arguments.c_str(), nullptr, nullptr);
            m_hashLastSuccessfulCompilation = currentHash;

            // Save to binary cache if compilation succeeded
            if (m_cacheEnabled && !m_cacheDirectory.empty())
            {
                saveProgramToCache(currentHash);
                if (m_logger)
                {
                    m_logger->logInfo("CLProgram: Saved program to binary cache (hash: " +
                                      std::to_string(currentHash) + ")");
                }
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
            // Suppress direct stderr output when logger unavailable
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
                // Suppress direct stderr output when logger unavailable
            }

            logBuildStatusIfFailed(prog, m_ComputeContext->GetDevice(), m_logger);
        }
        progsToLink.push_back(*m_lib);

        m_program = std::make_unique<cl::Program>(cl::linkProgram(progsToLink));
        validateBuildStatus(*m_program, m_ComputeContext->GetDevice(), m_logger);

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

        // Clear all sources to start fresh
        m_staticSources.clear();
        m_dynamicSources.clear();
        m_sources.clear();

        loadSourceFromFile(filenames);
        addDynamicSource(dynamicSource);
        compileNonBlocking(callBack);
    }

    void CLProgram::buildFromSourceAndLinkWithLibNonBlocking(const FileNames & filenames,
                                                             const std::string & dynamicSource,
                                                             BuildCallBack & callBack)
    {
        ProfileFunction;

        m_valid = false;

        // Clear all sources to start fresh
        m_staticSources.clear();
        m_dynamicSources.clear();
        m_sources.clear();

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

        // Clear all sources to start fresh
        m_staticSources.clear();
        m_dynamicSources.clear();
        m_sources.clear();

        loadSourceFromFile(filenames);
        addSource(dynamicSource);
        compile(callBack);
    }

    void CLProgram::loadAndCompileLib(const FileNames & filenames)
    {
        ProfileFunction;

        m_valid = false;
        CL_ERROR(m_ComputeContext->GetQueue().finish());

        // Clear all sources to start fresh
        m_staticSources.clear();
        m_dynamicSources.clear();
        m_sources.clear();

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
        if (m_logger)
        {
            m_logger->logInfo("CLProgram::setCacheDirectory called with path: " + path.string());
        }
        m_cacheDirectory = path;
        if (!path.empty() && !std::filesystem::exists(path))
        {
            std::filesystem::create_directories(path);
            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Created cache directory: " + path.string());
            }
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
            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Cleared cache directory: " +
                                  m_cacheDirectory.string());
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logWarning("Failed to clear cache: " + std::string(e.what()));
            }
        }
    }

    void CLProgram::setCacheEnabled(bool enabled)
    {
        m_cacheEnabled = enabled;
        if (m_logger)
        {
            m_logger->logInfo("CLProgram: Cache " + std::string(enabled ? "enabled" : "disabled"));
        }
    }

    bool CLProgram::isCacheEnabled() const
    {
        return m_cacheEnabled;
    }

    bool CLProgram::loadProgramFromCache(size_t hash)
    {
        if (!m_cacheEnabled || m_cacheDirectory.empty())
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
                if (m_logger)
                {
                    m_logger->logInfo("CLProgram: Cache device signature mismatch, ignoring cache");
                }
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
                m_callBackUserData.program = m_program.get();
                // Ensure program is marked as valid and kernels cache is cleared
                m_valid = true;
                m_kernels.clear();
                if (m_logger)
                {
                    m_logger->logInfo(
                      "CLProgram: Successfully loaded and built program from cache");
                }
                return true;
            }
            catch (const std::exception & e)
            {
                if (m_logger)
                {
                    m_logger->logError("CLProgram: Failed to build cached program: " +
                                       std::string(e.what()));
                }
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
        if (!m_cacheEnabled || m_cacheDirectory.empty() || !m_program)
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

            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Saved program binary to cache: " +
                                  cachePath.string());
            }
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

    bool CLProgram::loadStaticLibraryFromCache(size_t staticHash, cl::Program & staticLibrary)
    {
        if (!m_cacheEnabled || m_cacheDirectory.empty())
            return false;

        auto cachePath = m_cacheDirectory / ("static_" + std::to_string(staticHash) + ".clcache");
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
                if (m_logger)
                {
                    m_logger->logInfo(
                      "CLProgram: Static library cache device signature mismatch, ignoring cache");
                }
                return false;
            }

            // Read binary data
            size_t binarySize;
            file.read(reinterpret_cast<char *>(&binarySize), sizeof(binarySize));
            std::vector<unsigned char> binary(binarySize);
            file.read(reinterpret_cast<char *>(binary.data()), binarySize);

            // Create program from binary
            staticLibrary = cl::Program(m_ComputeContext->GetContext(),
                                        std::vector<cl::Device>{m_ComputeContext->GetDevice()},
                                        std::vector<std::vector<unsigned char>>{binary});

            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Successfully loaded static library from cache");
            }
            return true;
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logWarning("Failed to load static library from cache: " +
                                     std::string(e.what()));
            }
            return false;
        }
    }

    void CLProgram::saveStaticLibraryToCache(size_t staticHash, const cl::Program & staticLibrary)
    {
        if (!m_cacheEnabled || m_cacheDirectory.empty())
            return;

        try
        {
            // Get program binaries
            auto binaries = staticLibrary.getInfo<CL_PROGRAM_BINARIES>();
            if (binaries.empty() || binaries[0].empty())
                return;

            auto cachePath =
              m_cacheDirectory / ("static_" + std::to_string(staticHash) + ".clcache");
            std::ofstream file(cachePath, std::ios::binary);
            if (!file.is_open())
                return;

            // Write device signature first
            std::string deviceSignature = getDeviceSignature();
            size_t deviceSigSize = deviceSignature.size();
            file.write(reinterpret_cast<const char *>(&deviceSigSize), sizeof(deviceSigSize));
            file.write(deviceSignature.data(), deviceSigSize);

            // Write binary data
            const auto & binary = binaries[0];
            size_t binarySize = binary.size();
            file.write(reinterpret_cast<const char *>(&binarySize), sizeof(binarySize));
            file.write(reinterpret_cast<const char *>(binary.data()), binarySize);

            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Saved static library to cache");
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logWarning("Failed to save static library to cache: " +
                                     std::string(e.what()));
            }
        }
    }

    bool CLProgram::loadLinkedProgramFromCache(size_t staticHash, size_t dynamicHash)
    {
        if (!m_cacheEnabled || m_cacheDirectory.empty())
            return false;

        auto cachePath = m_cacheDirectory / ("linked_" + std::to_string(staticHash) + "_" +
                                             std::to_string(dynamicHash) + ".clcache");
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
                if (m_logger)
                {
                    m_logger->logInfo(
                      "CLProgram: Linked program cache device signature mismatch, ignoring cache");
                }
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
                m_callBackUserData.program = m_program.get();
                // Ensure program is marked as valid and kernels cache is cleared
                m_valid = true;
                m_kernels.clear();
                if (m_logger)
                {
                    m_logger->logInfo(
                      "CLProgram: Successfully loaded and built linked program from cache");
                }
                return true;
            }
            catch (const std::exception & e)
            {
                if (m_logger)
                {
                    m_logger->logError("CLProgram: Failed to build cached linked program: " +
                                       std::string(e.what()));
                }
                return false;
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logWarning("Failed to load linked program from cache: " +
                                     std::string(e.what()));
            }
            return false;
        }
    }

    void CLProgram::saveLinkedProgramToCache(size_t staticHash, size_t dynamicHash)
    {
        if (!m_cacheEnabled || m_cacheDirectory.empty() || !m_program)
            return;

        try
        {
            // Get program binaries
            auto binaries = m_program->getInfo<CL_PROGRAM_BINARIES>();
            if (binaries.empty() || binaries[0].empty())
                return;

            auto cachePath = m_cacheDirectory / ("linked_" + std::to_string(staticHash) + "_" +
                                                 std::to_string(dynamicHash) + ".clcache");
            std::ofstream file(cachePath, std::ios::binary);
            if (!file.is_open())
                return;

            // Write device signature first
            std::string deviceSignature = getDeviceSignature();
            size_t deviceSigSize = deviceSignature.size();
            file.write(reinterpret_cast<const char *>(&deviceSigSize), sizeof(deviceSigSize));
            file.write(deviceSignature.data(), deviceSigSize);

            // Write binary data
            const auto & binary = binaries[0];
            size_t binarySize = binary.size();
            file.write(reinterpret_cast<const char *>(&binarySize), sizeof(binarySize));
            file.write(reinterpret_cast<const char *>(binary.data()), binarySize);

            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Saved linked program to cache");
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logWarning("Failed to save linked program to cache: " +
                                     std::string(e.what()));
            }
        }
    }
}
