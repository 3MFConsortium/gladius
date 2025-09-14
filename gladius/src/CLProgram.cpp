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
                else
                {
                    std::cerr << errorMsg << "\n";
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
                else
                {
                    std::cerr << "missing file in resources: " << resourceFilename << "\n";
                }
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

        // Check if we can load complete linked program from cache first
        if (!m_cacheDirectory.empty() && !m_staticSources.empty() && !m_dynamicSources.empty() &&
            loadLinkedProgramFromCache(staticHash, dynamicHash))
        {
            std::cout << "CLProgram: Loaded linked program from cache (static: " << staticHash
                      << ", dynamic: " << dynamicHash << ")" << std::endl;
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
        if (!m_cacheDirectory.empty() && loadProgramFromCache(currentHash))
        {
            std::cout << "CLProgram: Loaded program from single-level binary cache (hash: "
                      << currentHash << ")" << std::endl;
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
        if (!m_cacheDirectory.empty() && !m_staticSources.empty() && !m_dynamicSources.empty())
        {
            cl::Program staticLibrary;
            bool staticLoaded = loadStaticLibraryFromCache(staticHash, staticLibrary);

            if (!staticLoaded)
            {
                // Compile static library from source
                std::cout << "CLProgram: Compiling static library from source" << std::endl;
                try
                {
                    staticLibrary = cl::Program(m_ComputeContext->GetContext(), m_staticSources);
                    const auto arguments = generateDefineSymbol();
                    staticLibrary.compile(arguments.c_str());

                    // Save compiled static library to cache
                    saveStaticLibraryToCache(staticHash, staticLibrary);
                    std::cout << "CLProgram: Compiled and cached static library (hash: "
                              << staticHash << ")" << std::endl;
                }
                catch (const std::exception & e)
                {
                    std::cout << "CLProgram: Failed to compile static library: " << e.what()
                              << std::endl;
                    // Fall back to single-level compilation
                    goto single_level_compile;
                }
            }
            else
            {
                std::cout << "CLProgram: Loaded static library from cache (hash: " << staticHash
                          << ")" << std::endl;
            }

            // Compile dynamic source and link with static library
            try
            {
                cl::Program::Sources dynamicSourcesCombined;

                // Synthetic interface: only forward declarations & prototypes actually used
                // Avoids pulling full implementations a second time (which would cause
                // duplicate symbol definitions when linking with static library).
                static char const * kDynamicInterface = R"(
#ifndef GLADIUS_DYNAMIC_IFACE_GUARD
#define GLADIUS_DYNAMIC_IFACE_GUARD
// Synthetic dynamic interface:
//  - Minimal forward declarations mirroring a subset of types.h needed by generated dynamic kernels
//  - Function prototypes (no bodies) for utility functions referenced directly by generated code
//  - PAYLOAD_ARGS / PASS_PAYLOAD_ARGS macro pair (mirrors arguments.h) so that calls using
//    payload(..., PASS_PAYLOAD_ARGS) compile without including full headers (avoids duplicate symbols)
//  - Prototype for payload() itself (implemented in static library SDF sources)
//  - Keep this list minimal; if new unresolved identifiers appear, add ONLY their prototypes here
//    instead of including entire headers.
struct BoundingBox { float4 min; float4 max; };
enum PrimitiveType { SDF_OUTER_POLYGON, SDF_INNER_POLYGON, SDF_BEAMS, SDF_MESH_TRIANGLES, SDF_MESH_KD_ROOT_NODE, SDF_MESH_KD_NODE, SDF_LINES, SDF_VDB, SDF_VDB_BINARY, SDF_VDB_FACE_INDICES, SDF_VDB_GRAYSCALE_8BIT, SDF_IMAGESTACK, SDF_BEAM_LATTICE, SDF_BEAM, SDF_BALL, SDF_BEAM_BVH_NODE, SDF_PRIMITIVE_INDICES, SDF_BEAM_LATTICE_VOXEL_INDEX, SDF_BEAM_LATTICE_VOXEL_TYPE};
struct PrimitiveMeta { float4 center; int start; int end; float scaling; enum PrimitiveType primitiveType; struct BoundingBox boundingBox; float4 approximationTop; float4 approximationBottom; };
struct RenderingSettings { float time_s; float z_mm; int flags; int approximation; float quality; float weightDistToNb; float weightMidPoint; float normalOffset; };
struct Command { int type; int id; int placeholder0; int placeholder1; int args[32]; int output[32]; };
// Function prototypes referenced from generated model kernels
float3 matrixVectorMul3f(float16 matrix, float3 vector);
float glsl_mod1f(float a, float b);
float bbBox(float3 pos, float3 bbmin, float3 bbmax);
float payload(float3 pos, int startIndex, int endIndex, \
    float4 buildArea, __global struct PrimitiveMeta *primitives, int primitivesSize, \
    __global float *data, int dataSize, struct RenderingSettings renderingSettings, \
    __read_only image3d_t preCompSdf, __global float *parameter, __global struct Command *cmds, \
    int sizeOfCmds, struct BoundingBox preCompSdfBBox);
// Payload macros (mirrors arguments.h)
#ifndef PAYLOAD_ARGS
#define PAYLOAD_ARGS \
    float4 buildArea, __global struct PrimitiveMeta *primitives, int primitivesSize, \
      __global float *data, int dataSize, struct RenderingSettings renderingSettings, \
      __read_only image3d_t preCompSdf, __global float *parameter, __global struct Command *cmds, \
      int sizeOfCmds, struct BoundingBox preCompSdfBBox
#define PASS_PAYLOAD_ARGS \
    buildArea, primitives, primitivesSize, data, dataSize, renderingSettings, preCompSdf, \
      parameter, cmds, sizeOfCmds, preCompSdfBBox
#endif
#endif // GLADIUS_DYNAMIC_IFACE_GUARD
)";
                dynamicSourcesCombined.emplace_back(kDynamicInterface);
                dynamicSourcesCombined.insert(
                  dynamicSourcesCombined.end(), m_dynamicSources.begin(), m_dynamicSources.end());

                cl::Program dynamicProgram(m_ComputeContext->GetContext(), dynamicSourcesCombined);
                const auto arguments = generateDefineSymbol();
                dynamicProgram.compile(arguments.c_str());
                validateBuildStatus(dynamicProgram, m_ComputeContext->GetDevice(), m_logger);

                // Link static library with dynamic program
                std::vector<cl::Program> programs = {staticLibrary, dynamicProgram};
                m_program = std::make_unique<cl::Program>(cl::linkProgram(programs, ""));
                validateBuildStatus(*m_program, m_ComputeContext->GetDevice(), m_logger);

                std::cout << "CLProgram: Successfully linked static library with dynamic program"
                          << std::endl;

                // Save linked program to cache
                saveLinkedProgramToCache(staticHash, dynamicHash);
                std::cout << "CLProgram: Saved linked program to cache" << std::endl;

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
                std::cout << "CLProgram: Failed to compile/link dynamic program: " << e.what()
                          << std::endl;
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
        addDynamicSource(dynamicSource);
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
                m_callBackUserData.program = m_program.get();
                // Ensure program is marked as valid and kernels cache is cleared
                m_valid = true;
                m_kernels.clear();
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

    bool CLProgram::loadStaticLibraryFromCache(size_t staticHash, cl::Program & staticLibrary)
    {
        if (m_cacheDirectory.empty())
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
                std::cout
                  << "CLProgram: Static library cache device signature mismatch, ignoring cache"
                  << std::endl;
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

            std::cout << "CLProgram: Successfully loaded static library from cache" << std::endl;
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
        if (m_cacheDirectory.empty())
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

            std::cout << "CLProgram: Saved static library to cache" << std::endl;
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
        if (m_cacheDirectory.empty())
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
                std::cout
                  << "CLProgram: Linked program cache device signature mismatch, ignoring cache"
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
                m_callBackUserData.program = m_program.get();
                // Ensure program is marked as valid and kernels cache is cleared
                m_valid = true;
                m_kernels.clear();
                std::cout << "CLProgram: Successfully loaded and built linked program from cache"
                          << std::endl;
                return true;
            }
            catch (const std::exception & e)
            {
                std::cout << "CLProgram: Failed to build cached linked program: " << e.what()
                          << std::endl;
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
        if (m_cacheDirectory.empty() || !m_program)
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

            std::cout << "CLProgram: Saved linked program to cache" << std::endl;
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
