#pragma once

#include "ImageStack.h"
#include "src/io/vdb.h"

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


    class ImageExtractor
    {
      public:
        ImageExtractor() = default;
        ~ImageExtractor();

        bool open(std::filesystem::path const & filename);
        void close();
        std::vector<unsigned char> loadFile(std::filesystem::path const & filename) const;
        ImageStack loadImageStack(FileList const & filenames);

        openvdb::GridBase::Ptr loadAsVdbGrid(FileList const & filenames) const;
        void printAllFiles() const;

        PixelFormat determinePixelFormat(std::filesystem::path const & filename) const;

        LodePNGInfo const & getPNGInfo() const;

      private:
        unzFile m_archive = nullptr;
        LodePNGInfo m_pngInfo{};
    };
} // namespace gladius::io