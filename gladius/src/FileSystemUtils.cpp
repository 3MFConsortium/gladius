#include "FileSystemUtils.h"

#include <filesystem>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _MSVC_LANG
#include <unistd.h>
#endif

namespace gladius
{
    std::filesystem::path getAppDir()
    {
#ifdef WIN32
        char * executablePath;
        if (_get_pgmptr(&executablePath) != 0)
        {
            return {};
        }
        return std::filesystem::path{executablePath}.parent_path();
#else
        char executablePath[PATH_MAX];
        ssize_t len = ::readlink("/proc/self/exe", executablePath, sizeof(executablePath));
        if (len == -1 || len >= sizeof(executablePath))
            len = 0;
        executablePath[len] = '\0';

        return std::filesystem::path{executablePath}.parent_path();
#endif
    }
}
