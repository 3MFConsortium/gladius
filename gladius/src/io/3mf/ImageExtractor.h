#pragma once

#include "ImageStack.h"
#include "io/vdb.h"

#include <filesystem>
#include <lodepng.h>
#include <minizip/unzip.h>

#include <string>
#include <vector>

namespace lodepng
{
    LodePNGInfo getPNGHeaderInfo(const std::vector<unsigned char> & png);
}

namespace gladius::io
{

    std::filesystem::path removeLeadingSlash(std::filesystem::path const & path);

    using FileList = std::vector<std::filesystem::path>;
    enum class FileLoaderType
    {
        Archive,
        Filesystem
    };

    class ImageExtractor
    {
      public:
        ImageExtractor() = default;
        ~ImageExtractor();

        openvdb::GridBase::Ptr loadAsVdbGrid(FileList const & filenames,
                                             FileLoaderType fileLoaderType) const;

        bool loadFromArchive(std::filesystem::path const & filename);

        void close();

        std::vector<unsigned char>
        loadFileFromArchive(std::filesystem::path const & filename) const;

        std::vector<unsigned char>
        loadFileFromFilesystem(std::filesystem::path const & filename) const;

        ImageStack loadImageStack(FileList const & filenames);

        void printAllFiles() const;

        PixelFormat determinePixelFormat(std::filesystem::path const & filename) const;

        LodePNGInfo const & getPNGInfo() const;

      private:
        unzFile m_archive = nullptr;
        LodePNGInfo m_pngInfo{};
    };
} // namespace gladius::io