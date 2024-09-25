#include "testhelper.h"

#include <array>
#include <iostream>

#ifdef WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include <lodepng.h>
#include <fmt/format.h>

#include "BitmapLayer.h"

namespace gladius_integration_tests
{
    std::optional<std::filesystem::path> findGladiusSharedLib()
    {
        std::array<std::string_view, 2> const pathsToTest{
          "./", "../../src/api/GladiusLib_component/Implementations/Cpp"};
        std::array<std::string_view, 2> const binaryNames{"gladiuslib.dll", "gladiuslib.so"};

        for (auto const & path : pathsToTest)
        {
            try
            {
                auto const absolutePath = std::filesystem::canonical(path);
                for (auto const & binaryName : binaryNames)
                {
                    if (std::filesystem::exists(absolutePath / binaryName))
                    {
                        return absolutePath / binaryName;
                    }
                }
            }
            catch (std::exception const & e)
            {
                std::cout << "Exception thrown during search of shared library: " << e.what()
                          << "\n";
            }
        }

        return {};
    }

    void saveBitmapLayer(std::filesystem::path const & filename,
                         std::vector<float> & data,
                         unsigned int width_px,
                         unsigned int height_px)
    {
        std::vector<unsigned char> image;
        image.resize(data.size());

         std::cout << "exporting bitmap to " << filename << "\n";
        std::transform(data.begin(),
                       data.end(),
                       image.begin(),
                       [](auto value) { return static_cast<unsigned char>(value * 1000.f); });

        auto const error = lodepng::encode(filename.string(), image, width_px, height_px, LodePNGColorType::LCT_GREY);

        if (error)
        {
            throw std::runtime_error(
               fmt::format("encoder error {} : {}", error , lodepng_error_text(error)));
        }
    }

}
