#pragma once
#include "Contour.h"

#include <vector>

namespace gladius
{
    struct BitmapLayer
    {
        Vector2 position{};            // Position of the bitmap layer
        std::vector<float> bitmapData; // Bitmap data stored as a vector of floats
        Vector2 pixelSize{};           // Size of each pixel
        size_t width_px{};             // Width of the bitmap in pixels
        size_t height_px{};            // Height of the bitmap in pixels
    };
}