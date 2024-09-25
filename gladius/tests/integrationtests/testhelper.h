#pragma once
#include <filesystem>
#include <optional>
#include <vector>


namespace gladius_integration_tests
{
    [[nodiscard]] std::optional<std::filesystem::path> findGladiusSharedLib();
    
     void saveBitmapLayer(std::filesystem::path const & filename,
                         std::vector<float> & data,
                         unsigned int width_px,
                         unsigned int height_px);
}
