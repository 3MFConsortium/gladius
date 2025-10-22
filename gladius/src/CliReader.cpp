#include "CliReader.h"
#include "kernel/types.h"

#include "scopeguard.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <istream>
#include <sstream>

namespace gladius
{

    void CliReader::read(const std::filesystem::path & fileName, Primitives & primitives)
    {
        primitives.clear();
        m_file.open(fileName);

        // close the file when we leave this scope using a scope guard
        auto guard = scope_guard([this] { m_file.close(); });

        if (!m_file.is_open())
        {
            throw std::runtime_error("failed to open " + fileName.string());
        }

        auto constexpr HatchCommand = "$$HATCHES/";
        auto constexpr PolyLineCommand = "$$POLYLINE/";
        auto constexpr UnitsCommand = "$$UNITS/";
        auto constexpr LayerCommand = "$$LAYER/";

        float unit{1.0f};

        float previousHeight = 0.f;
        float zheight = 0.f;
        float layerThickness_mm = 0.1f;

        while (!m_file.eof())
        {
            std::string lineStr;
            std::getline(m_file, lineStr);
            if (lineStr.rfind(UnitsCommand, 0) != std::string::npos)
            {
                std::stringstream line(lineStr);
                const auto dataStart = std::strlen(UnitsCommand);
                line.seekg(dataStart, std::ios_base::beg);
                line >> unit;
            }

            if (lineStr.rfind(LayerCommand, 0) != std::string::npos)
            {
                std::stringstream line(lineStr);
                const auto dataStart = std::strlen(LayerCommand);
                line.seekg(dataStart, std::ios_base::beg);
                int z = 0;
                line >> z;
                previousHeight = zheight;
                zheight = z * unit;
            }

            if (lineStr.rfind(HatchCommand, 0) != std::string::npos)
            {
                std::stringstream line(lineStr);

                const auto dataStart = std::strlen(HatchCommand);
                line.seekg(dataStart, std::ios_base::beg);

                int id{};
                int size{};
                char delimeter = 0;
                line >> id;
                line >> delimeter;
                line >> size;

                PrimitiveMeta metaData{};
                metaData.primitiveType = SDF_LINES;
                metaData.start = static_cast<int>(primitives.data.getData().size());

                while (!line.eof())
                {
                    float x{};

                    float y{};
                    line >> delimeter >> x >> delimeter >> y;
                    primitives.data.getData().push_back(x * unit);
                    primitives.data.getData().push_back(y * unit);
                }
                metaData.end = static_cast<int>(primitives.data.getData().size());
                metaData.boundingBox.min.z = previousHeight;
                metaData.boundingBox.max.z = zheight;
                primitives.primitives.getData().emplace_back(metaData);
            }

            if (lineStr.rfind(PolyLineCommand, 0) != std::string::npos)
            {
                std::stringstream line(lineStr);

                const auto dataStart = std::strlen(PolyLineCommand);

                line.seekg(dataStart, std::ios_base::beg);
                int id{};
                int dir{};
                int size{};
                char delimeter = 0;
                line >> id;
                line >> delimeter;
                line >> dir;
                line >> delimeter;
                line >> size;

                PrimitiveMeta metaData{};
                metaData.primitiveType = (dir == 1) ? SDF_OUTER_POLYGON : SDF_INNER_POLYGON;
                metaData.start = static_cast<int>(primitives.data.getData().size());
                while (!line.eof())
                {
                    float x{};
                    float y{};
                    line >> delimeter >> x >> delimeter >> y;
                    primitives.data.getData().push_back(x * unit);
                    primitives.data.getData().push_back(y * unit);
                    metaData.center = {{x * unit, y * unit, 0.f, 0.f}};
                }

                metaData.end = static_cast<int>(primitives.data.getData().size() - 2);
                metaData.boundingBox.min.z = previousHeight;
                metaData.boundingBox.max.z = zheight;
                primitives.primitives.getData().emplace_back(metaData);
            }
        }
        calculateBoundingVolumes(primitives);
        primitives.write();
    }

    void CliReader::calculateBoundingVolumes(Primitives & primitives)
    {
        for (auto & primitive : primitives.primitives.getData())
        {
            // Initialize bounding box to extreme values
            primitive.boundingBox.min.x = std::numeric_limits<float>::max();
            primitive.boundingBox.min.y = std::numeric_limits<float>::max();
            primitive.boundingBox.max.x = std::numeric_limits<float>::lowest();
            primitive.boundingBox.max.y = std::numeric_limits<float>::lowest();

            int numPoints = 0;

            // Ensure start and end are within bounds
            if (primitive.start < 0 || primitive.end > primitives.data.getData().size())
            {
                continue; // Skip invalid ranges
            }

            // Calculate bounding box and center
            for (int i = primitive.start; i < primitive.end; i += 2)
            {
                const auto point =
                  cl_float2{primitives.data.getData()[i], primitives.data.getData()[i + 1]};
                primitive.center.x += point.x;
                primitive.center.y += point.y;

                primitive.boundingBox.min.x = std::min(point.x, primitive.boundingBox.min.x);
                primitive.boundingBox.min.y = std::min(point.y, primitive.boundingBox.min.y);
                primitive.boundingBox.max.x = std::max(point.x, primitive.boundingBox.max.x);
                primitive.boundingBox.max.y = std::max(point.y, primitive.boundingBox.max.y);

                ++numPoints;
            }

            // Calculate the average center if there are points
            if (numPoints > 0)
            {
                primitive.center.x /= static_cast<float>(numPoints);
                primitive.center.y /= static_cast<float>(numPoints);
            }
        }
    }
}
