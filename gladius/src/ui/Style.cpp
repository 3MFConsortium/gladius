#include "Style.h"

#include <nodes/DerivedNodes.h>
#include <nodes/nodesfwd.h>
#include <algorithm>
#include <cmath>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

namespace gladius::ui
{

    // Function to generate a unique color for each node type
    ImVec4 generateUniqueColor(size_t index)
    {
        // Generate a color using a simple algorithm to ensure uniqueness
        const size_t numColors = 50;
        const size_t hueStep = 360 / numColors;
        size_t hue = (index * hueStep) % 360;
        size_t saturation = 80 + (index % 20) * 20; // Vary saturation slightly
        size_t value = 60 + (index % 5) * 20;       // Vary value slightly

        // Convert HSV to RGB (simple approximation)
        float c = (value / 100.0f) * (saturation / 100.0f);
        float x = c * (1.0f - abs(fmod(hue / 60.0f, 2.0f) - 1.0f));
        float m = (value / 100.0f) - c;

        float r, g, b;
        if (hue < 60)
        {
            r = c, g = x, b = 0;
        }
        else if (hue < 120)
        {
            r = x, g = c, b = 0;
        }
        else if (hue < 180)
        {
            r = 0, g = c, b = x;
        }
        else if (hue < 240)
        {
            r = 0, g = x, b = c;
        }
        else if (hue < 300)
        {
            r = x, g = 0, b = c;
        }
        else
        {
            r = c, g = 0, b = x;
        }

        return ImVec4(r + m, g + m, b + m, 1.0f);
    }

    NodeTypeToColor createNodeTypeToColors()
    {
        NodeTypeToColor map;
        size_t index = 0;

        // Helper lambda to add node type to the map
        auto addNodeType = [&map, &index](auto nodeType)
        { map[typeid(nodeType)] = generateUniqueColor(index++); };

        // Add each node type to the map
        std::apply([&](auto... nodeTypes) { (addNodeType(nodeTypes), ...); },
                   gladius::nodes::NodeTypes{});

        return map;
    };
} // namespace gladius
