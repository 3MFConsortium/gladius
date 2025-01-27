#include "ImageExtractor.h"

#include "ImageStack.h"
#include <filesystem>
#include <fmt/format.h>
#include <iostream>
#include <lodepng.h>
#include <minizip/unzip.h>
#include <string>
#include <vector>

namespace lodepng
{
    LodePNGInfo getPNGHeaderInfo(const std::vector<unsigned char> & png)
    {
        unsigned w, h;
        lodepng::State state;
        lodepng_inspect(&w, &h, &state, &png[0], png.size());
        return state.info_png;
    }
}
namespace gladius::io
{
    std::filesystem::path removeLeadingSlash(std::filesystem::path const & path)
    {
        auto str = path.wstring();
        if (str.front() == '/')
        {
            str.erase(0, 1);
        }
        return std::filesystem::path(str);
    }

    ImageExtractor::~ImageExtractor()
    {
        close();
    }

    bool ImageExtractor::loadFromArchive(std::filesystem::path const & filename)
    {
        // Close any previously opened archive
        close();

        // Open the zip archive
        m_archive = unzOpen(filename.string().c_str());
        if (!m_archive)
        {
            throw std::runtime_error(
              fmt::format("Error opening zip archive: {}", filename.string()));
        }

        return true;
    }

    void ImageExtractor::close()
    {
        if (m_archive)
        {
            unzClose(m_archive);
            m_archive = nullptr;
        }
    }

    std::vector<unsigned char>
    ImageExtractor::loadFileFromArchive(std::filesystem::path const & filename) const
    {
        if (!m_archive)
        {
            throw std::runtime_error("Error: zip archive not open");
        }
        auto filenameWithoutLeadingSlash = removeLeadingSlash(filename).string();
        // Locate the file in the archive
        if (unzLocateFile(m_archive, filenameWithoutLeadingSlash.c_str(), 0) != UNZ_OK)
        {
            throw std::runtime_error(
              fmt::format("Error locating file {} in zip archive", filename.string()));
        }

        // Get the file information
        unz_file_info fileInfo;
        if (unzGetCurrentFileInfo(m_archive, &fileInfo, nullptr, 0, nullptr, 0, nullptr, 0) !=
            UNZ_OK)
        {
            throw std::runtime_error(fmt::format(
              "Error getting file information for {} in zip archive", filename.string()));
        }

        // Allocate memory for the file contents
        std::vector<unsigned char> fileContents(fileInfo.uncompressed_size);

        // Read the file contents from the archive
        if (unzOpenCurrentFile(m_archive) != UNZ_OK)
        {
            throw std::runtime_error(
              fmt::format("Error opening file {} in zip archive", filename.string()));
        }
        if (unzReadCurrentFile(m_archive, fileContents.data(), fileInfo.uncompressed_size) < 0)
        {
            throw std::runtime_error(
              fmt::format("Error reading file {} from zip archive", filename.string()));
        }
        unzCloseCurrentFile(m_archive);

        return fileContents;
    }

    std::vector<unsigned char>
    ImageExtractor::loadFileFromFilesystem(std::filesystem::path const & filename) const
    {
        std::vector<unsigned char> fileContents;
        if (std::filesystem::exists(filename))
        {
            std::ifstream file(filename, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error(fmt::format("Error opening file {}", filename.string()));
            }

            file.seekg(0, std::ios::end);
            fileContents.resize(file.tellg());
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char *>(fileContents.data()), fileContents.size());
        }

        return fileContents;
    }

    std::string colorTypeToString(LodePNGColorType colorType)
    {
        switch (colorType)
        {
        case LCT_GREY:
            return "LCT_GREY";
        case LCT_RGB:
            return "LCT_RGB";
        case LCT_PALETTE:
            return "LCT_PALETTE";
        case LCT_GREY_ALPHA:
            return "LCT_GREY_ALPHA";
        case LCT_RGBA:
            return "LCT_RGBA";
        default:
            return "unknown";
        }
    }

    PixelFormat fromPngColorType(LodePNGColorMode const & colorMode)
    {
        if (colorMode.bitdepth == 1)
        {
            return PixelFormat::GRAYSCALE_1BIT;
        }
        if (colorMode.bitdepth == 4 || colorMode.bitdepth == 8)
        {
            switch (colorMode.colortype)
            {
            case LCT_GREY:
                return PixelFormat::GRAYSCALE_8BIT;
            case LCT_GREY_ALPHA:
                return PixelFormat::GRAYSCALE_ALPHA_8BIT;
            case LCT_RGB:
                return PixelFormat::RGB_8BIT;
            case LCT_RGBA:
                return PixelFormat::RGBA_8BIT;
            case LCT_PALETTE:
                return PixelFormat::RGBA_8BIT;
            default:
                throw std::runtime_error(fmt::format("Error: unsupported PNG color type {}", colorTypeToString(colorMode.colortype)));
            }
        }
        if (colorMode.bitdepth == 16)
        {
            switch (colorMode.colortype)
            {
            case LCT_GREY:
                return PixelFormat::GRAYSCALE_16BIT;
            case LCT_GREY_ALPHA:
                return PixelFormat::GRAYSCALE_ALPHA_16BIT;
            case LCT_RGB:
                return PixelFormat::RGB_16BIT;
            case LCT_RGBA:
                return PixelFormat::RGBA_16BIT;
            case LCT_PALETTE:
                return PixelFormat::RGBA_16BIT;
            default:
                throw std::runtime_error(fmt::format("Error: unsupported PNG color type {}", colorTypeToString(colorMode.colortype)));
            }
        }

        throw std::runtime_error(fmt::format("Error: unsupported PNG bit depth {}", colorMode.bitdepth));
    }

    ImageStack ImageExtractor::loadImageStack(FileList const & filenames)
    {
        ImageStack images;
        images.reserve(filenames.size());

        for (const auto & filename : filenames)
        {
            auto const fileContents = loadFileFromArchive(filename);
            if (fileContents.empty())
            {
                continue;
            }

            std::vector<unsigned char> image;
            unsigned int width, height;
            unsigned const int error = lodepng::decode(image, width, height, fileContents);
            if (error)
            {
                throw std::runtime_error(
                  fmt::format("Error decoding PNG {}: {}", filename.string(), error));
            }

            Image img{std::move(image), width, height};
            img.swapXYData();
            m_pngInfo = lodepng::getPNGHeaderInfo(fileContents);
            img.setFormat(fromPngColorType(m_pngInfo.color));
            img.setBitDepth(m_pngInfo.color.bitdepth);
            images.emplace_back(std::move(img));
        }

        return images;
    }

    openvdb::GridBase::Ptr ImageExtractor::loadAsVdbGrid(FileList const & filenames,
                                                         FileLoaderType fileLoaderType) const
    {
        if (filenames.empty())
        {
            throw std::runtime_error("Error: no files to load");
        }

        ;

        auto const fileContents = fileLoaderType == FileLoaderType::Archive
                                    ? loadFileFromArchive(filenames.front())
                                    : loadFileFromFilesystem(filenames.front());
        if (fileContents.empty())
        {
            throw std::runtime_error("Error: empty file contents");
        }

        std::vector<unsigned char> image;
        unsigned int width, height;
        unsigned const int error = lodepng::decode(image, width, height, fileContents);
        if (error)
        {
            throw std::runtime_error(
              fmt::format("Error decoding PNG {}: {}", filenames.front().string(), error));
        }
        auto const pngInfo = lodepng::getPNGHeaderInfo(fileContents);
        auto const pixelFormat = fromPngColorType(pngInfo.color);

        if (image.size() % (width * height) != 0)
        {
            throw std::runtime_error("Error: image data size is not a multiple of width * height");
        }

        Image img{std::move(image), width, height};
        img.setFormat(pixelFormat);
        img.setBitDepth(pngInfo.color.bitdepth);

        if (img.getFormat() != PixelFormat::GRAYSCALE_8BIT)
        {
            throw std::runtime_error(
              "Error: only grayscale 8 bit images are supported for VDB import");
        }
        {
            openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create(0.);

            const auto transform = openvdb::math::Transform::createLinearTransform(1.);
            grid->setTransform(transform);
            grid->setName("sdf");

            auto accessor = grid->getAccessor();

            auto zIndex = 0;
            for (auto & filename : filenames)
            {

                auto const fileContents = fileLoaderType == FileLoaderType::Archive
                                            ? loadFileFromArchive(filename)
                                            : loadFileFromFilesystem(filename);
                if (fileContents.empty())
                {
                    zIndex++;
                    continue;
                }

                std::vector<unsigned char> image;
                unsigned int width, height;
                unsigned const int error = lodepng::decode(image, width, height, fileContents);
                grid->setGridClass(openvdb::GRID_LEVEL_SET);

                if (error)
                {
                    throw std::runtime_error(
                      fmt::format("Error decoding PNG {}: {}", filename.string(), error));
                }

                auto const pngInfoCurrent = lodepng::getPNGHeaderInfo(fileContents);

                Image img{std::move(image), width, height};
                img.setFormat(fromPngColorType(pngInfoCurrent.color));
                img.setBitDepth(pngInfoCurrent.color.bitdepth);

                unsigned int const numChannels =
                  img.getData().size() / (img.getWidth() * img.getHeight());

                for (unsigned int y = 0; y < img.getHeight(); ++y)
                {
                    for (unsigned int x = 0; x < img.getWidth(); ++x)
                    {
                        unsigned int index = (y * img.getWidth() + x) * numChannels;
                        float value = img.getData()[index] / 255.0f;
                        accessor.setValue(openvdb::Coord(img.getWidth() - x - 1, y, zIndex), value);
                    }
                }

                zIndex++;
                grid->pruneGrid();
            }

            return grid;
        }

        return {};
    }

    void ImageExtractor::printAllFiles() const
    {
        if (!m_archive)
        {
            throw std::runtime_error("Error: zip archive not open");
        }

        unzGoToFirstFile(m_archive);
        do
        {
            char filename[1024];
            unz_file_info fileInfo;
            unzGetCurrentFileInfo(
              m_archive, &fileInfo, filename, sizeof(filename), nullptr, 0, nullptr, 0);
            std::cout << filename << std::endl;
        } while (unzGoToNextFile(m_archive) == UNZ_OK);
    }

    PixelFormat ImageExtractor::determinePixelFormat(std::filesystem::path const & filename) const
    {
        auto const fileContents = loadFileFromArchive(filename);
        if (fileContents.empty())
        {
            throw std::runtime_error("Error: empty file contents");
        }

        auto const pngInfo = lodepng::getPNGHeaderInfo(fileContents);
        return fromPngColorType(pngInfo.color);
    }

    LodePNGInfo const & ImageExtractor::getPNGInfo() const
    {
        return m_pngInfo;
    }
} // namespace gladius::io

