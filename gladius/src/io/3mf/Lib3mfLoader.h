#pragma once

#include <filesystem>
#include <optional>
#include <iostream>

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#  include <limits.h>
#else
#  include <unistd.h>
#  include <limits.h>
#endif

// Include complete C++ bindings for Lib3MF (types + wrapper + inline API)
#include <lib3mf_types.hpp>
#include <lib3mf_abi.hpp>
#include <lib3mf_implicit.hpp>

namespace gladius::io
{
    namespace detail
    {
        inline std::optional<std::filesystem::path> executableDirectory()
        {
            try
            {
#if defined(__linux__)
                // Prefer procfs symlink
                std::filesystem::path exe = std::filesystem::read_symlink("/proc/self/exe");
                if (!exe.empty())
                {
                    return exe.parent_path();
                }
#elif defined(_WIN32)
                wchar_t buffer[MAX_PATH];
                DWORD len = ::GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
                if (len > 0)
                {
                    return std::filesystem::path(buffer).parent_path();
                }
#elif defined(__APPLE__)
                char buffer[PATH_MAX];
                uint32_t size = sizeof(buffer);
                if (_NSGetExecutablePath(buffer, &size) == 0)
                {
                    return std::filesystem::path(buffer).parent_path();
                }
#else
                // Fallback: argv[0] not available here; leave unset
#endif
            }
            catch (...)
            {
                // ignore
            }
            return std::nullopt;
        }
    } // namespace detail

    // Load Lib3MF while temporarily switching CWD to the executable directory, then restore it.
    inline Lib3MF::PWrapper loadLib3mfScoped()
    {
        std::filesystem::path previous;
        bool changed = false;
        try
        {
            previous = std::filesystem::current_path();
        }
        catch (...)
        {
            // ignore; if this throws we can't restore later, but best effort
        }

        try
        {
            auto exeDir = detail::executableDirectory();
            if (exeDir && std::filesystem::exists(*exeDir))
            {
                try
                {
                    std::filesystem::current_path(*exeDir);
                    changed = true;
                }
                catch (...)
                {
                    std::cerr << "Warning: Could not change working directory to executable directory "
                                 "("
                              << exeDir->string() << ")\n";
                }
            }

            auto wrapper = Lib3MF::CWrapper::loadLibrary();

            if (changed)
            {
                try
                {
                    std::filesystem::current_path(previous);
                }
                catch (...)
                {
                }
            }

            return wrapper;
        }
        catch (...)
        {
            if (changed)
            {
                try
                {
                    std::filesystem::current_path(previous);
                }
                catch (...)
                {
                }
            }
            throw;
        }
    }
} // namespace gladius::io
