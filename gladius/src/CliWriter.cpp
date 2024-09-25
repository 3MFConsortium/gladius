#include "CliWriter.h"
#include "ContourExtractor.h"
#include "gpgpu.h"
#include "scopeguard.h"

#include <iostream>

namespace gladius
{
    void CliWriter::saveCurrentLayer(const std::filesystem::path & fileName,
                                     ComputeCore & generator)
    {
        m_fileName = fileName;
        m_file.open(fileName);
        if (!m_file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + fileName.string());
        }

        auto guard = scope_guard([this] { m_file.close(); });

        writeHeader();

        m_file << "$$GEOMETRYSTART\n";
        auto sliceParameter = contourOnlyParameter();
        sliceParameter.zHeight_mm = generator.getSliceHeight();
        generator.requestContourUpdate(sliceParameter);
        writeLayer(generator.getContour().getContour(), sliceParameter.zHeight_mm);

        m_file << "$$GEOMETRYEND\n";
    }

    void CliWriter::save(const std::filesystem::path & fileName, ComputeCore & generator)
    {
        m_fileName = fileName;
        m_file.open(fileName);

        if (!m_file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + fileName.string());
        }

        auto guard = scope_guard([this] { m_file.close(); });

        writeHeader();

        if (!generator.updateBBox())
        {
            throw std::runtime_error("Cli export failed: Bounding box is not available yet");
        }

        const auto bb = generator.getBoundingBox();
        const auto startZ = std::max(0.f, bb->min.z);
        const auto numberLayer = static_cast<int>((bb->max.z - startZ) / m_layerThickness_mm);
        generator.setSliceHeight(startZ);

        generator.requestContourUpdate(contourOnlyParameter());
        auto sliceParameter = contourOnlyParameter();
        for (auto i = 0; i < numberLayer; i++)
        {
            const auto z_previous = generator.getSliceHeight();
            const auto z_mm = roundToLayerThickness(z_previous + m_layerThickness_mm);
            sliceParameter.zHeight_mm = z_mm;
            generator.requestContourUpdate(sliceParameter);
            writeLayer(generator.getContour().getContour(), z_mm);
            std::cout << "Exporting layer " << i << " of " << numberLayer << " with height " << z_mm
                      << " mm\n";
        }

        m_file << "$$GEOMETRYEND\n";
    }

    std::filesystem::path CliWriter::getFilename() const
    {
        return m_fileName;
    }

    void CliWriter::beginExport(const std::filesystem::path & fileName, ComputeCore & generator)
    {
        m_fileName = fileName;
        try
        {
            m_file.open(fileName);
        }
        catch (const std::exception & e)
        {
            std::cerr << e.what() << '\n';
        }
        writeHeader();
        generator.updateBBoxOrThrow();

        const auto bb = generator.getBoundingBox();

        m_startHeight_mm = bb->min.z;
        m_endHeight_mm = bb->max.z;
        generator.setSliceHeight(bb->min.z);
    }

    bool CliWriter::advanceExport(ComputeCore & generator)
    {
        auto const z_previous = generator.getSliceHeight();
        auto const z_mm = roundToLayerThickness(z_previous + m_layerThickness_mm);

        auto sliceParameter = contourOnlyParameter();
        sliceParameter.zHeight_mm = z_mm;
        generator.setSliceHeight(z_mm);
        generator.requestContourUpdate(sliceParameter);
        writeLayer(generator.getContour().getContour(), z_mm);

        m_progress = (z_mm - m_startHeight_mm) / (m_endHeight_mm - m_startHeight_mm);
        return z_mm < generator.getBoundingBox()->max.z + m_layerThickness_mm;
    }

    void CliWriter::finalizeExport()
    {
        m_file << "$$GEOMETRYEND\n";
        m_file.close();
    }

    float CliWriter::getProgress() const
    {
        return m_progress;
    }

    void CliWriter::writeHeader()
    {
        m_file << "$$HEADERSTART\n";
        m_file << "$$ASCII\n";
        m_file << "$$UNITS/1\n";
        m_file << "$$VERSION/200\n";
        m_file << "$$LABEL/1, part1\n";
        m_file << "$$LAYERS/1\n";
        m_file << "$$HEADEREND\n";

        m_file << "$$GEOMETRYSTART\n";
    }

    void CliWriter::writeLayer(const PolyLines & polyLines, double z_mm)
    {
        m_file << "$$LAYER/" << z_mm << "\n";
        for (auto & polyLine : polyLines)
        {
            writePolyLine(polyLine);
        }
    }

    void CliWriter::writePolyLine(const PolyLine & polyLine)
    {
        ++modelId;
        auto constexpr seperator{","};

        if (polyLine.contourMode == ContourMode::ExcludeFromSlice)
        {
            return;
        }

        m_file << "$$POLYLINE/" << modelId << seperator << static_cast<int>(polyLine.contourMode)
               << seperator << polyLine.vertices.size();
        for (auto & vertex : polyLine.vertices)
        {
            m_file << seperator << vertex.x();
            m_file << seperator << vertex.y();
        }
        m_file << "\n";
    }

    float CliWriter::roundToLayerThickness(float value) const
    {
        return roundTo(value, m_layerThickness_mm);
    }
}
