#pragma once

#include <functional>
#include <string_view>
#include <utility>
#include <vector>

#include "BitmapLayer.h"
#include "Contour.h"

namespace gladius
{
    // Alias for a function that generates a BitmapLayer
    using BitmapGenerator = std::function<BitmapLayer(float, Vector2)>;

    class BitmapChannel
    {
      public:
        // Constructor initializes the name and generator
        BitmapChannel(std::string_view name, BitmapGenerator generator)
            : m_name(name)
            , m_generator(std::move(generator))
        {
        }

        // Returns the name of the BitmapChannel
        [[nodiscard]] std::string const & getName() const;

        // Generates a BitmapLayer based on the provided parameters
        [[nodiscard]] BitmapLayer generate(float z_mm, Vector2 pixelSize) const;

      private:
        std::string m_name;          // Name of the BitmapChannel
        BitmapGenerator m_generator; // Function to generate BitmapLayer
    };

    // Alias for a collection of BitmapChannel objects
    using BitmapChannels = std::vector<BitmapChannel>;
}