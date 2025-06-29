#include "SvgWriter.h"
#include "ContourExtractor.h"

#include <iostream>

#include "gpgpu.h"

namespace gladius
{
    void SvgWriter::saveCurrentLayer(const std::filesystem::path & fileName,
                                     ComputeCore & generator)
    {
        m_fileName = fileName;
        m_file.open(fileName);

        writeHeader();

        auto sliceParameter = contourOnlyParameter();
        sliceParameter.zHeight_mm = generator.getSliceHeight();
        generator.requestContourUpdate(sliceParameter);
        writeLayer(generator.getContour()->getContour());

        m_file << "</svg>";
        m_file.close();
    }

    void SvgWriter::writeHeader()
    {
        m_file
          << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
          << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
          << "xmlns:xlink=\"http://www.w3.org/1999/xlink\" version=\"1.1\" baseProfile=\"full\" "
          << "width=\"800mm\" height=\"600mm\" viewBox=\"-400 - 300 800 600\">";
    }

    void SvgWriter::writeLayer(const PolyLines & polyLines)
    {

        m_file << "\n<path fill-rule=\"evenodd\" d=\"";
        for (auto const & polyLine : polyLines)
        {
            writePolyLine(polyLine);
        }
        m_file << "\"/>\n";
    }

    void SvgWriter::writePolyLine(const PolyLine & polyLine)
    {
        // Don't write contour if in exclude mode
        if (polyLine.contourMode == ContourMode::ExcludeFromSlice)
        {
            return;
        }

        if (polyLine.vertices.size() > 0)
        {
            m_file << "M " << polyLine.vertices[0].x() << "," << (400.f - polyLine.vertices[0].y())
                   << " ";
            for (size_t i = 1; i < polyLine.vertices.size(); ++i)
            {
                m_file << " L " << polyLine.vertices[i].x() << ","
                       << (400.f - polyLine.vertices[i].y()) << " ";
            }
            m_file << " z ";
        }
    }
}