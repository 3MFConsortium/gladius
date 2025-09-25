#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gladius
{
    using QueriedFilename = std::optional<std::filesystem::path>;

    using FilePatterns = std::vector<std::string>;
    auto querySaveFilename(FilePatterns const & filePattern, std::filesystem::path baseDir = {})
      -> QueriedFilename;

    auto queryLoadFilename(FilePatterns const & filePattern, std::filesystem::path baseDir = {})
      -> QueriedFilename;

    auto queryDirectory(std::filesystem::path baseDir = {}) -> QueriedFilename;
}