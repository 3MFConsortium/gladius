#include "CLProgram.h"
#include "Profiling.h"
#include "TimeMeasurement.h"
#include "gpgpu.h"
#include <boost/functional/hash.hpp>
#include <chrono>
#include <cmrc/cmrc.hpp>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream> // TODO: Remove if no other translation units rely on side-effects; kept temporarily for potential error output fallbacks
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

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
            return false;
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

        // Dump the OpenCL build log to a file if available
        inline void dumpBuildLog(std::filesystem::path const & dir,
                                 std::string const & filename,
                                 cl::Program const & program,
                                 cl::Device const & device)
        {
            try
            {
                std::string const log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);
                if (!log.empty())
                {
                    dumpTextFile(dir, filename, log);
                }
            }
            catch (...)
            {
                // Ignore logging failures
            }
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
                        // --- Diagnostic helpers (OpenCL source/options + build logs dump) ---
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

        constexpr uint32_t CACHE_FILE_MAGIC = 0x434C4348u; // 'CLCH'
        constexpr uint16_t CACHE_FORMAT_VERSION = 2u;
        constexpr std::chrono::milliseconds CACHE_LOCK_RETRY_DELAY{10};
        constexpr std::chrono::seconds CACHE_STALE_LOCK_THRESHOLD{30};
        constexpr size_t CACHE_LOCK_MAX_ATTEMPTS = 200;
        constexpr uint64_t CACHE_MAX_METADATA_LENGTH = 1ull << 20;               // 1 MiB
        constexpr uint64_t CACHE_MAX_BINARY_LENGTH = 512ull * 1024ull * 1024ull; // 512 MiB

        struct CacheFilePayload
        {
            std::string deviceSignature;
            std::string buildSignature;
            std::vector<unsigned char> binary;
        };

        [[nodiscard]] uint64_t computeBinaryChecksum(std::vector<unsigned char> const & data)
        {
            size_t seed = 0;
            boost::hash_range(seed, data.begin(), data.end());
            return static_cast<uint64_t>(seed);
        }

        [[nodiscard]] std::filesystem::path makeLockPath(std::filesystem::path const & cachePath)
        {
            std::filesystem::path lockPath = cachePath;
            lockPath += ".lock";
            return lockPath;
        }

        class ScopedCacheFileLock
        {
          public:
            ScopedCacheFileLock(std::filesystem::path lockPath, events::SharedLogger logger)
                : m_lockPath(std::move(lockPath))
                , m_logger(std::move(logger))
            {
                acquire();
            }

            ScopedCacheFileLock(ScopedCacheFileLock const &) = delete;
            ScopedCacheFileLock & operator=(ScopedCacheFileLock const &) = delete;

            ~ScopedCacheFileLock()
            {
                if (m_acquired)
                {
                    std::error_code ec;
                    std::filesystem::remove(m_lockPath, ec);
                    if (ec && m_logger)
                    {
                        m_logger->logWarning("CLProgram: Failed to release cache lock '" +
                                             m_lockPath.string() + "': " + ec.message());
                    }
                }
            }

            [[nodiscard]] bool acquired() const
            {
                return m_acquired;
            }

          private:
            void acquire()
            {
                using namespace std::chrono_literals;

                for (size_t attempt = 0; attempt < CACHE_LOCK_MAX_ATTEMPTS; ++attempt)
                {
                    std::error_code ec;
                    if (std::filesystem::create_directory(m_lockPath, ec))
                    {
                        updateTimestamp();
                        m_acquired = true;
                        return;
                    }

                    if (ec && ec.value() != EEXIST)
                    {
                        logWarning("create_directory failed: " + ec.message());
                        return;
                    }

                    if ((attempt + 1) % 50 == 0 && isLockStale())
                    {
                        std::filesystem::remove(m_lockPath, ec);
                        if (!ec)
                        {
                            continue;
                        }
                    }

                    std::this_thread::sleep_for(CACHE_LOCK_RETRY_DELAY);
                }

                logWarning("timed out waiting for cache lock");
            }

            void updateTimestamp() const
            {
                std::error_code ec;
                std::filesystem::last_write_time(
                  m_lockPath, std::filesystem::file_time_type::clock::now(), ec);
                (void) ec; // Non-fatal; ignore errors.
            }

            [[nodiscard]] bool isLockStale() const
            {
                std::error_code ec;
                auto const lastWrite = std::filesystem::last_write_time(m_lockPath, ec);
                if (ec)
                {
                    return false;
                }
                auto const now = std::filesystem::file_time_type::clock::now();
                auto const age = now - lastWrite;
                if (age < std::filesystem::file_time_type::duration::zero())
                {
                    return false;
                }
                auto const threshold =
                  std::chrono::duration_cast<std::filesystem::file_time_type::duration>(
                    CACHE_STALE_LOCK_THRESHOLD);
                return age > threshold;
            }

            void logWarning(std::string const & message) const
            {
                if (m_logger)
                {
                    m_logger->logWarning("CLProgram: Cache lock issue: " + message + " (" +
                                         m_lockPath.string() + ")");
                }
            }

            std::filesystem::path m_lockPath;
            events::SharedLogger m_logger;
            bool m_acquired = false;
        };

        [[nodiscard]] bool waitForLockRelease(const std::filesystem::path & lockPath,
                                              events::SharedLogger const & logger)
        {
            using namespace std::chrono_literals;

            for (size_t attempt = 0; attempt < CACHE_LOCK_MAX_ATTEMPTS; ++attempt)
            {
                std::error_code ec;
                if (!std::filesystem::exists(lockPath, ec))
                {
                    return true;
                }

                if ((attempt + 1) % 50 == 0)
                {
                    std::error_code staleEc;
                    auto const lastWrite = std::filesystem::last_write_time(lockPath, staleEc);
                    if (!staleEc)
                    {
                        auto const now = std::filesystem::file_time_type::clock::now();
                        auto const age = now - lastWrite;
                        auto const threshold =
                          std::chrono::duration_cast<std::filesystem::file_time_type::duration>(
                            CACHE_STALE_LOCK_THRESHOLD);
                        if (age > threshold &&
                            age > std::filesystem::file_time_type::duration::zero())
                        {
                            std::filesystem::remove(lockPath, staleEc);
                            if (!staleEc)
                            {
                                return true;
                            }
                        }
                    }
                }

                std::this_thread::sleep_for(CACHE_LOCK_RETRY_DELAY);
            }

            if (logger)
            {
                logger->logWarning("CLProgram: Cache lock wait timed out for " + lockPath.string());
            }
            return false;
        }

        template <typename T>
        bool writePrimitive(std::ofstream & stream, const T & value)
        {
            stream.write(reinterpret_cast<const char *>(&value), sizeof(T));
            return static_cast<bool>(stream);
        }

        template <typename T>
        bool readPrimitive(std::ifstream & stream, T & value)
        {
            stream.read(reinterpret_cast<char *>(&value), sizeof(T));
            return static_cast<bool>(stream);
        }

        [[nodiscard]] std::optional<CacheFilePayload>
        readCacheFile(const std::filesystem::path & cachePath,
                      events::SharedLogger const & logger,
                      std::string const & context)
        {
            std::error_code ec;
            if (!std::filesystem::exists(cachePath, ec) || ec)
            {
                return std::nullopt;
            }

            auto const lockPath = makeLockPath(cachePath);
            if (!waitForLockRelease(lockPath, logger))
            {
                return std::nullopt;
            }

            std::ifstream stream(cachePath, std::ios::binary);
            if (!stream.is_open())
            {
                if (logger)
                {
                    logger->logWarning("CLProgram: Failed to open cache file '" +
                                       cachePath.string() + "' for " + context);
                }
                return std::nullopt;
            }

            uint32_t magic = 0;
            uint16_t version = 0;
            uint16_t reserved = 0;
            uint64_t checksum = 0;
            uint64_t deviceLength = 0;
            uint64_t buildLength = 0;
            uint64_t binaryLength = 0;

            if (!readPrimitive(stream, magic) || magic != CACHE_FILE_MAGIC)
            {
                return std::nullopt;
            }
            if (!readPrimitive(stream, version) || version != CACHE_FORMAT_VERSION)
            {
                return std::nullopt;
            }
            if (!readPrimitive(stream, reserved))
            {
                return std::nullopt;
            }
            if (!readPrimitive(stream, checksum) || !readPrimitive(stream, deviceLength) ||
                !readPrimitive(stream, buildLength) || !readPrimitive(stream, binaryLength))
            {
                return std::nullopt;
            }

            if (deviceLength > CACHE_MAX_METADATA_LENGTH ||
                buildLength > CACHE_MAX_METADATA_LENGTH || binaryLength == 0 ||
                binaryLength > CACHE_MAX_BINARY_LENGTH)
            {
                return std::nullopt;
            }

            CacheFilePayload payload;
            payload.deviceSignature.resize(static_cast<std::size_t>(deviceLength));
            payload.buildSignature.resize(static_cast<std::size_t>(buildLength));
            payload.binary.resize(static_cast<std::size_t>(binaryLength));

            stream.read(payload.deviceSignature.data(), static_cast<std::streamsize>(deviceLength));
            stream.read(payload.buildSignature.data(), static_cast<std::streamsize>(buildLength));
            stream.read(reinterpret_cast<char *>(payload.binary.data()),
                        static_cast<std::streamsize>(binaryLength));

            if (!stream)
            {
                return std::nullopt;
            }

            if (computeBinaryChecksum(payload.binary) != checksum)
            {
                if (logger)
                {
                    logger->logWarning("CLProgram: Cache checksum mismatch for '" +
                                       cachePath.string() + "' in " + context);
                }
                return std::nullopt;
            }

            return payload;
        }

        bool writeCacheFile(const std::filesystem::path & cachePath,
                            const CacheFilePayload & payload,
                            events::SharedLogger const & logger,
                            std::string const & context)
        {
            auto parent = cachePath.parent_path();
            if (!parent.empty())
            {
                std::error_code ec;
                std::filesystem::create_directories(parent, ec);
                if (ec)
                {
                    if (logger)
                    {
                        logger->logWarning("CLProgram: Failed to create cache directory '" +
                                           parent.string() + "': " + ec.message());
                    }
                    return false;
                }
            }

            auto const lockPath = makeLockPath(cachePath);
            ScopedCacheFileLock lock(lockPath, logger);
            if (!lock.acquired())
            {
                return false;
            }

            auto const tmpPath = cachePath.string() + ".tmp";
            std::error_code ec;
            std::filesystem::remove(tmpPath, ec);

            std::ofstream stream(tmpPath, std::ios::binary | std::ios::trunc);
            if (!stream.is_open())
            {
                if (logger)
                {
                    logger->logWarning("CLProgram: Failed to open cache temp file '" + tmpPath +
                                       "' for " + context);
                }
                return false;
            }

            auto const checksum = computeBinaryChecksum(payload.binary);

            uint32_t const magic = CACHE_FILE_MAGIC;
            uint16_t const version = CACHE_FORMAT_VERSION;
            uint16_t const reserved = 0;
            uint64_t const deviceLength = payload.deviceSignature.size();
            uint64_t const buildLength = payload.buildSignature.size();
            uint64_t const binaryLength = payload.binary.size();

            if (!writePrimitive(stream, magic) || !writePrimitive(stream, version) ||
                !writePrimitive(stream, reserved) || !writePrimitive(stream, checksum) ||
                !writePrimitive(stream, deviceLength) || !writePrimitive(stream, buildLength) ||
                !writePrimitive(stream, binaryLength))
            {
                return false;
            }

            stream.write(payload.deviceSignature.data(),
                         static_cast<std::streamsize>(payload.deviceSignature.size()));
            stream.write(payload.buildSignature.data(),
                         static_cast<std::streamsize>(payload.buildSignature.size()));
            stream.write(reinterpret_cast<const char *>(payload.binary.data()),
                         static_cast<std::streamsize>(payload.binary.size()));

            stream.flush();
            if (!stream)
            {
                return false;
            }
            stream.close();

            std::filesystem::remove(cachePath, ec);
            ec.clear();
            std::filesystem::rename(tmpPath, cachePath, ec);
            if (ec)
            {
                if (logger)
                {
                    logger->logWarning("CLProgram: Failed to finalize cache file '" +
                                       cachePath.string() + "': " + ec.message());
                }
                std::filesystem::remove(tmpPath);
                return false;
            }

            return true;
        }
    }

    void validateBuildStatus(const cl::Program & program,
                             const cl::Device & device,
                             events::SharedLogger const & logger)
    {
        try
        {
            auto const status = program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(device);
            std::string const buildLog = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device);

            // Forward build log to logger if available; avoid noisy stderr
            if (!buildLog.empty() && logger)
            {
                logger->logWarning(std::string("OpenCL build log:\n") + buildLog);
            }

            if (status != CL_BUILD_SUCCESS)
            {
                std::string errorMsg =
                  "OpenCL program build failed (status: " + std::to_string(status) + ")";
                if (!buildLog.empty())
                {
                    errorMsg += ": " + buildLog;
                }

                if (logger)
                {
                    logger->logError(errorMsg);
                }

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
        if (m_enableTwoLevelPipeline && m_cacheEnabled && !m_cacheDirectory.empty() &&
            !m_staticSources.empty() && !m_dynamicSources.empty() &&
            loadLinkedProgramFromCache(staticHash, dynamicHash))
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
        if (m_logger)
        {
            m_logger->logInfo("CLProgram: Two-level compilation check - cacheEnabled: " +
                              std::to_string(m_cacheEnabled) + ", cacheDirectory: '" +
                              m_cacheDirectory.string() + "'" +
                              ", staticSources: " + std::to_string(m_staticSources.size()) +
                              ", dynamicSources: " + std::to_string(m_dynamicSources.size()));
        }
        if (m_enableTwoLevelPipeline && m_cacheEnabled && !m_cacheDirectory.empty() &&
            !m_staticSources.empty() && !m_dynamicSources.empty())
        {
            cl::Program staticLibrary;
            bool staticLoaded = loadStaticLibraryFromCache(staticHash, staticLibrary);

            if (!staticLoaded)
            {
                // Compile static library from source - include ALL files (headers +
                // implementations)
                if (m_logger)
                {
                    m_logger->logInfo("CLProgram: Compiling static library from source");
                }
                try
                {
                    // Create static library sources with headers + implementations
                    cl::Program::Sources staticLibrarySources;
                    const auto fs = cmrc::gladius_resources::get_filesystem();

                    // Include ALL files (headers + implementations) in static library
                    for (const auto & filename : m_sourceFilenames)
                    {
                        const auto resourceFilename = std::string("src/kernel/" + filename);
                        if (fs.exists(resourceFilename) && fs.is_file(resourceFilename))
                        {
                            auto file = fs.open(resourceFilename);
                            std::string source(file.begin(), file.end());
                            if (m_kernelReplacements)
                            {
                                applyKernelReplacements(source, *m_kernelReplacements);
                            }
                            staticLibrarySources.emplace_back(source);
                        }
                    }

                    // Compile objects for the static library (no final link yet)
                    cl::Program staticObjects(m_ComputeContext->GetContext(), staticLibrarySources);
                    const auto arguments = generateDefineSymbol();
                    staticObjects.compile(arguments.c_str());
                    validateBuildStatus(staticObjects, m_ComputeContext->GetDevice(), m_logger);

                    if (isOclDumpEnabled())
                    {
                        auto const dumpDir = ensureDumpDir(m_cacheDirectory);
                        dumpBuildLog(dumpDir,
                                     "buildlog_static_" + std::to_string(staticHash) + ".txt",
                                     staticObjects,
                                     m_ComputeContext->GetDevice());
                    }

                    // Retain compiled objects for subsequent final link
                    staticLibrary = staticObjects;

                    // Save compiled static objects to cache for future runs
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
                    compileSingleLevel(callBack, currentHash);
                    return;
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

                // Include all header files (.h) in dynamic compilation for full type/macro access
                // Only .cl files (implementations) should be excluded to avoid duplicate symbols
                const auto fs = cmrc::gladius_resources::get_filesystem();

                // Re-load and include all header files from the filenames list
                for (const auto & filename : m_sourceFilenames)
                {
                    if (filename.ends_with(".h"))
                    {
                        const auto resourceFilename = std::string("src/kernel/" + filename);
                        if (fs.exists(resourceFilename) && fs.is_file(resourceFilename))
                        {
                            auto file = fs.open(resourceFilename);
                            std::string headerSource(file.begin(), file.end());
                            if (m_kernelReplacements)
                            {
                                applyKernelReplacements(headerSource, *m_kernelReplacements);
                            }
                            dynamicSourcesCombined.emplace_back(headerSource);
                        }
                    }
                }

                // Add the generated model code
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
                if (isOclDumpEnabled())
                {
                    auto const dumpDir = ensureDumpDir(m_cacheDirectory);
                    dumpBuildLog(dumpDir,
                                 "buildlog_dynamic_" + std::to_string(dynamicHash) + ".txt",
                                 dynamicProgram,
                                 m_ComputeContext->GetDevice());
                }
                validateBuildStatus(dynamicProgram, m_ComputeContext->GetDevice(), m_logger);

                // Link static library with dynamic program
                std::vector<cl::Program> programs = {staticLibrary, dynamicProgram};
                const auto linkOptions = generateDefineSymbol();
                m_program =
                  std::make_unique<cl::Program>(cl::linkProgram(programs, linkOptions.c_str()));

                // Some drivers require an explicit build() after link() to materialize kernels
                try
                {
                    m_program->build({m_ComputeContext->GetDevice()});
                }
                catch (const std::exception &)
                {
                    // build() can be a no-op after link() on some drivers; continue to validate
                }

                if (isOclDumpEnabled())
                {
                    auto const dumpDir = ensureDumpDir(m_cacheDirectory);
                    dumpBuildLog(dumpDir,
                                 "buildlog_linked_" + std::to_string(dynamicHash) + "_" +
                                   std::to_string(staticHash) + ".txt",
                                 *m_program,
                                 m_ComputeContext->GetDevice());
                }

                // Validate and log detailed diagnostics of the linked program
                validateBuildStatus(*m_program, m_ComputeContext->GetDevice(), m_logger);
                if (m_logger)
                {
                    m_logger->logInfo(makeProgramDiagnostics(*m_program,
                                                             m_ComputeContext->GetDevice(),
                                                             generateDefineSymbol(),
                                                             "link(executable)"));
                }

                // Guard: some drivers may produce an empty executable when linking only a library
                // with a header-only dynamic part Be strict here: require an EXECUTABLE binary with
                // at least one kernel, otherwise force fallback.
                try
                {
                    // Check for kernels
                    auto const numKernels = m_program->getInfo<CL_PROGRAM_NUM_KERNELS>();
                    // Check kernel names (can be empty on some drivers when no kernels are present)
                    std::string kernelNames;
                    try
                    {
                        kernelNames = m_program->getInfo<CL_PROGRAM_KERNEL_NAMES>();
                    }
                    catch (...)
                    {
                        // ignore, keep empty
                    }
                    // Check binary type
                    cl_program_binary_type binType{};
                    try
                    {
                        m_program->getBuildInfo(
                          m_ComputeContext->GetDevice(), CL_PROGRAM_BINARY_TYPE, &binType);
                    }
                    catch (...)
                    {
                        binType = CL_PROGRAM_BINARY_TYPE_NONE;
                    }

                    bool const hasKernels = (numKernels > 0) || (!kernelNames.empty());
                    bool const isExecutable = (binType == CL_PROGRAM_BINARY_TYPE_EXECUTABLE);
                    if (!hasKernels || !isExecutable)
                    {
                        if (m_logger)
                        {
                            m_logger->logWarning(
                              std::string("CLProgram: Linked program validation failed (kernels=") +
                              std::to_string(numKernels) +
                              ", executable=" + (isExecutable ? "true" : "false") +
                              "). Falling back to single-level build.");
                        }
                        // Force fallback by throwing to the outer catch which already routes to
                        // single_level_compile
                        throw std::runtime_error(
                          "Linked program invalid: no kernels or non-executable binary");
                    }
                }
                catch (const std::exception &)
                {
                    // Re-throw to be handled by the outer catch which falls back to single-level
                    throw;
                }

                if (m_logger)
                {
                    m_logger->logInfo(
                      "CLProgram: Successfully linked static library with dynamic program");
                }

                // Extra safety: ensure the program is executable and has kernels before caching
                try
                {
                    cl_program_binary_type binType{};
                    m_program->getBuildInfo(
                      m_ComputeContext->GetDevice(), CL_PROGRAM_BINARY_TYPE, &binType);
                    auto const numKernels = m_program->getInfo<CL_PROGRAM_NUM_KERNELS>();
                    if (binType != CL_PROGRAM_BINARY_TYPE_EXECUTABLE || numKernels == 0)
                    {
                        if (m_logger)
                        {
                            m_logger->logWarning("CLProgram: Not caching linked program (invalid: "
                                                 "no kernels or non-executable)");
                        }
                    }
                    else
                    {
                        // Save linked program to cache (validated)
                        saveLinkedProgramToCache(staticHash, dynamicHash);
                    }
                }
                catch (...)
                {
                    // On diagnostics failure, avoid caching to be safe
                }
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
                std::string errorDetails =
                  "CLProgram: Failed to compile/link dynamic program: " + std::string(e.what());
                if (m_logger)
                {
                    m_logger->logError(errorDetails);
                }
                // Avoid extra stderr noise; logger already captured details
                // Fall back to single-level compilation
                compileSingleLevel(callBack, currentHash);
                return;
            }
        }

        // If two-level path not taken or not applicable, perform single-level compile
        compileSingleLevel(callBack, currentHash);
        return;
    }

    void CLProgram::compileNonBlocking(BuildCallBack & callBack)
    {
        ProfileFunction m_kernelCompilation = std::async([&]() { this->compile(callBack); });
    }

    void CLProgram::compileSingleLevel(BuildCallBack & callBack, size_t currentHash)
    {
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
            if (isOclDumpEnabled())
            {
                auto const dumpDir = ensureDumpDir(m_cacheDirectory);
                dumpBuildLog(dumpDir,
                             std::string("buildlog_singlelevel_") + std::to_string(currentHash) +
                               ".txt",
                             *m_program,
                             m_ComputeContext->GetDevice());
            }
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
        buildFromSourceAndLinkWithLibImpl(filenames, dynamicSource, callBack, true);
    }

    void CLProgram::buildFromSourceAndLinkWithLib(const FileNames & filenames,
                                                  const std::string & dynamicSource,
                                                  BuildCallBack & callBack)
    {
        ProfileFunction;
        buildFromSourceAndLinkWithLibImpl(filenames, dynamicSource, callBack, false);
    }

    void CLProgram::buildFromSourceAndLinkWithLibImpl(const FileNames & filenames,
                                                      const std::string & dynamicSource,
                                                      BuildCallBack & callBack,
                                                      bool nonBlocking)
    {
        m_valid = false;

        // Clear all sources to start fresh
        m_staticSources.clear();
        m_dynamicSources.clear();
        m_sources.clear();

        // Store filenames for use in compile() function
        m_sourceFilenames = filenames;

        loadSourceFromFile(filenames);
        // Add as dynamic source to enable proper two-level compilation and hashing
        addDynamicSource(dynamicSource);

        if (nonBlocking)
        {
            compileNonBlocking(callBack);
        }
        else
        {
            compile(callBack);
        }
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
        {
            return false;
        }

        auto const cachePath = m_cacheDirectory / (std::to_string(hash) + ".clcache");
        auto payloadOpt = readCacheFile(cachePath, m_logger, "single-level program binary");
        if (!payloadOpt.has_value())
        {
            return false;
        }

        auto payload = std::move(*payloadOpt);
        auto const expectedDevice = getDeviceSignature();
        if (payload.deviceSignature != expectedDevice)
        {
            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Cache device signature mismatch for program " +
                                  std::to_string(hash));
            }
            return false;
        }

        auto const expectedSignature = makeSingleLevelBuildSignature(hash);
        if (payload.buildSignature != expectedSignature)
        {
            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Cache build signature mismatch for program " +
                                  std::to_string(hash));
            }
            return false;
        }

        try
        {
            m_program = std::make_unique<cl::Program>(
              m_ComputeContext->GetContext(),
              std::vector<cl::Device>{m_ComputeContext->GetDevice()},
              std::vector<std::vector<unsigned char>>{payload.binary});

            m_program->build({m_ComputeContext->GetDevice()});
            m_callBackUserData.program = m_program.get();
            m_valid = true;
            m_kernels.clear();
            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Loaded program from binary cache (hash: " +
                                  std::to_string(hash) + ")");
            }
            return true;
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logError("CLProgram: Failed to rebuild cached program (hash: " +
                                   std::to_string(hash) + ") - " + e.what());
            }
            return false;
        }
    }

    void CLProgram::saveProgramToCache(size_t hash)
    {
        if (!m_cacheEnabled || m_cacheDirectory.empty() || !m_program)
        {
            return;
        }

        try
        {
            auto binaries = m_program->getInfo<CL_PROGRAM_BINARIES>();
            if (binaries.empty() || binaries[0].empty())
            {
                return;
            }

            auto const cachePath = m_cacheDirectory / (std::to_string(hash) + ".clcache");
            CacheFilePayload payload;
            payload.deviceSignature = getDeviceSignature();
            payload.buildSignature = makeSingleLevelBuildSignature(hash);
            payload.binary = binaries[0];

            if (!writeCacheFile(cachePath, payload, m_logger, "single-level program binary"))
            {
                return;
            }

            if (m_logger)
            {
                m_logger->logInfo(
                  "CLProgram: Saved program to binary cache (hash: " + std::to_string(hash) + ")");
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logWarning("CLProgram: Failed to save program cache (hash: " +
                                     std::to_string(hash) + ") - " + e.what());
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

    std::string CLProgram::makeSingleLevelBuildSignature(size_t programHash) const
    {
        return std::to_string(programHash) + "|" + generateDefineSymbol();
    }

    std::string CLProgram::makeStaticLibrarySignature(size_t staticHash) const
    {
        return std::to_string(staticHash) + "|" + generateDefineSymbol();
    }

    std::string CLProgram::makeLinkedProgramSignature(size_t staticHash, size_t dynamicHash) const
    {
        return std::to_string(staticHash) + "|" + std::to_string(dynamicHash) + "|" +
               generateDefineSymbol();
    }

    bool CLProgram::loadStaticLibraryFromCache(size_t staticHash, cl::Program & staticLibrary)
    {
        if (!m_cacheEnabled || m_cacheDirectory.empty())
        {
            return false;
        }

        auto const cachePath =
          m_cacheDirectory / ("static_" + std::to_string(staticHash) + ".clcache");
        auto payloadOpt = readCacheFile(cachePath, m_logger, "static library");
        if (!payloadOpt.has_value())
        {
            return false;
        }

        auto payload = std::move(*payloadOpt);
        if (payload.deviceSignature != getDeviceSignature())
        {
            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Static cache device mismatch, ignoring cache");
            }
            return false;
        }

        if (payload.buildSignature != makeStaticLibrarySignature(staticHash))
        {
            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Static cache build mismatch, ignoring cache");
            }
            return false;
        }

        try
        {
            staticLibrary = cl::Program(m_ComputeContext->GetContext(),
                                        std::vector<cl::Device>{m_ComputeContext->GetDevice()},
                                        std::vector<std::vector<unsigned char>>{payload.binary});
            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Loaded static library from cache (hash: " +
                                  std::to_string(staticHash) + ")");
            }
            return true;
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logWarning("CLProgram: Failed to load static library binary (hash: " +
                                     std::to_string(staticHash) + ") - " + e.what());
            }
            return false;
        }
    }

    void CLProgram::saveStaticLibraryToCache(size_t staticHash, const cl::Program & staticLibrary)
    {
        if (!m_cacheEnabled || m_cacheDirectory.empty())
        {
            return;
        }

        try
        {
            auto binaries = staticLibrary.getInfo<CL_PROGRAM_BINARIES>();
            if (binaries.empty() || binaries[0].empty())
            {
                return;
            }

            auto const cachePath =
              m_cacheDirectory / ("static_" + std::to_string(staticHash) + ".clcache");
            CacheFilePayload payload;
            payload.deviceSignature = getDeviceSignature();
            payload.buildSignature = makeStaticLibrarySignature(staticHash);
            payload.binary = binaries[0];

            if (!writeCacheFile(cachePath, payload, m_logger, "static library"))
            {
                return;
            }

            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Saved static library to cache (hash: " +
                                  std::to_string(staticHash) + ")");
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logWarning("CLProgram: Failed to cache static library (hash: " +
                                     std::to_string(staticHash) + ") - " + e.what());
            }
        }
    }

    bool CLProgram::loadLinkedProgramFromCache(size_t staticHash, size_t dynamicHash)
    {
        if (!m_cacheEnabled || m_cacheDirectory.empty())
        {
            return false;
        }

        auto const cachePath = m_cacheDirectory / ("linked_" + std::to_string(staticHash) + "_" +
                                                   std::to_string(dynamicHash) + ".clcache");
        auto payloadOpt = readCacheFile(cachePath, m_logger, "linked program");
        if (!payloadOpt.has_value())
        {
            return false;
        }

        auto payload = std::move(*payloadOpt);
        if (payload.deviceSignature != getDeviceSignature())
        {
            if (m_logger)
            {
                m_logger->logInfo(
                  "CLProgram: Linked program cache device mismatch, ignoring cache");
            }
            return false;
        }

        if (payload.buildSignature != makeLinkedProgramSignature(staticHash, dynamicHash))
        {
            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Linked program cache build mismatch, ignoring cache");
            }
            return false;
        }

        try
        {
            auto cachedProgram = std::make_unique<cl::Program>(
              m_ComputeContext->GetContext(),
              std::vector<cl::Device>{m_ComputeContext->GetDevice()},
              std::vector<std::vector<unsigned char>>{payload.binary});

            try
            {
                cachedProgram->build({m_ComputeContext->GetDevice()});
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

            try
            {
                auto const numKernels = cachedProgram->getInfo<CL_PROGRAM_NUM_KERNELS>();
                if (numKernels == 0)
                {
                    if (m_logger)
                    {
                        m_logger->logWarning("CLProgram: Cached linked program has zero kernels, "
                                             "recompile required");
                    }
                    return false;
                }
            }
            catch (...)
            {
            }

            m_program = std::move(cachedProgram);
            m_callBackUserData.program = m_program.get();
            m_valid = true;
            m_kernels.clear();

            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Loaded linked program from cache (hashes: " +
                                  std::to_string(staticHash) + ", " + std::to_string(dynamicHash) +
                                  ")");
                m_logger->logInfo(makeProgramDiagnostics(*m_program,
                                                         m_ComputeContext->GetDevice(),
                                                         generateDefineSymbol(),
                                                         "load(linked_cache)"));
            }

            return true;
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logWarning(
                  "CLProgram: Failed to load linked program from cache (hashes: " +
                  std::to_string(staticHash) + ", " + std::to_string(dynamicHash) + ") - " +
                  e.what());
            }
            return false;
        }
    }

    void CLProgram::saveLinkedProgramToCache(size_t staticHash, size_t dynamicHash)
    {
        if (!m_cacheEnabled || m_cacheDirectory.empty() || !m_program)
        {
            return;
        }

        try
        {
            auto binaries = m_program->getInfo<CL_PROGRAM_BINARIES>();
            if (binaries.empty() || binaries[0].empty())
            {
                return;
            }

            CacheFilePayload payload;
            payload.deviceSignature = getDeviceSignature();
            payload.buildSignature = makeLinkedProgramSignature(staticHash, dynamicHash);
            payload.binary = binaries[0];

            auto const cachePath =
              m_cacheDirectory / ("linked_" + std::to_string(staticHash) + "_" +
                                  std::to_string(dynamicHash) + ".clcache");
            if (!writeCacheFile(cachePath, payload, m_logger, "linked program"))
            {
                return;
            }

            if (m_logger)
            {
                m_logger->logInfo("CLProgram: Saved linked program to cache (hashes: " +
                                  std::to_string(staticHash) + ", " + std::to_string(dynamicHash) +
                                  ")");
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->logWarning("CLProgram: Failed to cache linked program (hashes: " +
                                     std::to_string(staticHash) + ", " +
                                     std::to_string(dynamicHash) + ") - " + e.what());
            }
        }
    }
}
