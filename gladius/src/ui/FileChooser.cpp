
#include "FileChooser.h"
#include <string>
#include <tinyfiledialogs/tinyfiledialogs.h>

#include <filesystem>
namespace gladius
{
    auto querySaveFilename(FilePatterns const & filePattern, std::filesystem::path baseDir)
      -> QueriedFilename
    {
        if (filePattern.size() > 10)
        {
            throw std::runtime_error("More than 10 file patterns are not supported");
        }

        auto const numPattern = filePattern.size();

        char const * filePatterns[10];
        for (size_t i = 0u; i < numPattern; i++)
        {
            char const * pattern = filePattern[i].c_str();
            filePatterns[i] = pattern;
        }
        auto const fileName = tinyfd_saveFileDialog("Save File",
                                                    baseDir.string().c_str(),
                                                    static_cast<int>(numPattern),
                                                    filePatterns,
                                                    "Supported files");

        if (fileName == nullptr)
        {
            return {};
        }
        auto fileToSave = QueriedFilename(fileName);
        return fileToSave;
    }

    auto queryLoadFilename(FilePatterns const & filePattern, std::filesystem::path baseDir)
      -> QueriedFilename
    {
        if (filePattern.size() > 10)
        {
            throw std::runtime_error("More than 10 file patterns are not supported");
        }
        auto numPattern = filePattern.size();
        char const * filePatterns[10];
        for (size_t i = 0u; i < numPattern; i++)
        {
            char const * pattern = filePattern[i].c_str();
            filePatterns[i] = pattern;
        }
        auto const fileName = tinyfd_openFileDialog("Open File",
                                                    baseDir.string().c_str(),
                                                    static_cast<int>(numPattern),
                                                    filePatterns,
                                                    "Supported files",
                                                    0);
        if (fileName == nullptr)
        {
            return {};
        }
        return QueriedFilename(fileName);
    }

    auto queryDirectory(std::filesystem::path baseDir) -> QueriedFilename
    {
        auto const dirName = tinyfd_selectFolderDialog("Open Directory", baseDir.string().c_str());
        if (dirName == nullptr)
        {
            return {};
        }
        return QueriedFilename(dirName);
    }

}
