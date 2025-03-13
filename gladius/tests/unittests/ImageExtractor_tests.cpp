
#include "io/3mf/ImageExtractor.h"

#include <gtest/gtest.h>

namespace gladius_tests
{
    using namespace gladius;

    struct TestFiles
    {
        static auto constexpr Volumetric3mf = "testdata/box.imagestack.3mf";
    };

    TEST(ImageExtractor, Open_Valid3mfFile_ReturnsTrue)
    {
        // arrange
        io::ImageExtractor extractor;

        // act
        auto const result = extractor.loadFromArchive(TestFiles::Volumetric3mf);

        // assert
        EXPECT_TRUE(result);
    }

    TEST(ImageExtractor, Open_Invalid3mfFile_ThrowsException)
    {
        // arrange
        io::ImageExtractor extractor;

        // act (expect exception)
        EXPECT_THROW(extractor.loadFromArchive("testdata/Invalid.3mf"), std::runtime_error);
    }

    TEST(ImageExtractor, LoadFile_ValidFile_ReturnsFileContent)
    {
        // arrange
        io::ImageExtractor extractor;

        std::filesystem::path currentPath = std::filesystem::current_path();
        std::cout << "Current path is: " << currentPath << std::endl;
        auto const completeFilePath = currentPath / TestFiles::Volumetric3mf;
        std::cout << "Complete file path is: " << completeFilePath << std::endl;
        ASSERT_TRUE(std::filesystem::exists(completeFilePath));

        extractor.loadFromArchive(TestFiles::Volumetric3mf);

        // act
        auto const result = extractor.loadFileFromArchive("volume/1/layer_001.png");

        // assert
        EXPECT_FALSE(result.empty());
    }

    TEST(ImageExtractor, RemoveLeadingSlash_PathWithLeadingSlash_ReturnsPathWithoutLeadingSlash)
    {
        // arrange
        auto const path = std::filesystem::path("/test/path");

        // act
        auto const result = io::removeLeadingSlash(path);

        // assert
        EXPECT_EQ(result, std::filesystem::path("test/path"));
    }

    // Test if the size of the image is correct
    TEST(ImageExtractor, LoadFile_ValidFile_ReturnsCorrectSize)
    {
        // arrange
        io::ImageExtractor extractor;
        extractor.loadFromArchive(TestFiles::Volumetric3mf);

        // act
        io::ImageStack const imgStack = extractor.loadImageStack({"volume/1/layer_001.png"});
        auto firstImage = imgStack.front();
        auto pngInfo = extractor.getPNGInfo();

        constexpr auto numChannels = 4; // lodepng always decodes to RGBA
        auto numPixels = firstImage.getWidth() * firstImage.getHeight();
        auto numBytes = numPixels * numChannels;
        // assert
        EXPECT_EQ(firstImage.getData().size(), numBytes);
    }
} // namespace gladius_tests