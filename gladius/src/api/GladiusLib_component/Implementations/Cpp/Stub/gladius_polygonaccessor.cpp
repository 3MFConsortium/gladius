/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is a stub class definition of CPolygonAccessor

*/

#include "gladius_polygonaccessor.hpp"
#include "gladius_interfaceexception.hpp"

// Include custom headers here.

namespace GladiusLib::Impl
{

    /*************************************************************************************************************************
     Class definition of CPolygonAccessor
    **************************************************************************************************************************/

    void CPolygonAccessor::setPolygon(gladius::SharedPolyLines polyLines,
                                      gladius::PolyLines::const_iterator iterToPolyLine)
    {
        m_polyLines = std::move(polyLines);
        m_polyLineIter = iterToPolyLine;
        m_iter = m_polyLineIter->vertices.cbegin();
    }

    GladiusLib::sVector2f CPolygonAccessor::GetCurrentVertex()
    {
        return {m_iter->x(), m_iter->y()};
    }

    GladiusLib_uint64 CPolygonAccessor::GetSize()
    {
        return m_polyLineIter->vertices.size();
    }

    bool CPolygonAccessor::Next()
    {
        if (m_polyLineIter->vertices.empty() ||
            std::next(m_iter) == m_polyLineIter->vertices.cend())
        {
            return false;
        }
        ++m_iter;
        return true;
    }

    bool CPolygonAccessor::Prev()
    {
        if (m_polyLineIter->vertices.empty() || m_iter == m_polyLineIter->vertices.cbegin())
        {
            return false;
        }

        --m_iter;
        return true;
    }

    GladiusLib_single CPolygonAccessor::GetArea()
    {
        return m_polyLineIter->area;
    }

    void CPolygonAccessor::Begin()
    {
        m_iter = m_polyLineIter->vertices.cbegin();
    }
}
