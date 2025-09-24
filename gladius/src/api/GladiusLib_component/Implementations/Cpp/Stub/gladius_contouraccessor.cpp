/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is a stub class definition of CContourAccessor

*/

#include "gladius_contouraccessor.hpp"

#include <iostream>

#include "gladius_interfaceexception.hpp"
#include "gladius_polygonaccessor.hpp"

// Include custom headers here.

using namespace GladiusLib::Impl;

/*************************************************************************************************************************
 Class definition of CContourAccessor
**************************************************************************************************************************/

void CContourAccessor::setContour(gladius::SharedPolyLines resource)
{
    m_polyLines = std::move(resource);
    m_iter = m_polyLines->cbegin();
}

IPolygonAccessor * CContourAccessor::GetCurrentPolygon()
{
    if (m_polyLines->empty())
    {
        throw std::out_of_range("Contour is empty");
    }
    auto * const accessor = new CPolygonAccessor();
    accessor->setPolygon(m_polyLines, m_iter);
    return accessor;
}

GladiusLib_uint64 CContourAccessor::GetSize()
{
    return m_polyLines->size();
}

bool CContourAccessor::Next()
{
    if (m_polyLines->empty() || std::next(m_iter) == std::cend(*m_polyLines))
    {
        return false;
    }
    ++m_iter;
    return true;
}

bool CContourAccessor::Prev()
{
    if (m_iter == std::cbegin(*m_polyLines))
    {
        return false;
    }
    --m_iter;
    return true;
}

void CContourAccessor::Begin()
{
    m_iter = std::cbegin(*m_polyLines);
}
