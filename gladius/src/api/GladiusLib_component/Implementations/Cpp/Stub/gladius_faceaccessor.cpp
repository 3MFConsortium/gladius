/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is a stub class definition of CFaceAccessor

*/

#include "gladius_faceaccessor.hpp"

#include <utility>

#include "gladius_face.hpp"
#include "gladius_interfaceexception.hpp"

// Include custom headers here.

using namespace GladiusLib::Impl;

/*************************************************************************************************************************
 Class definition of CFaceAccessor
**************************************************************************************************************************/

void CFaceAccessor::setMesh(gladius::SharedMesh mesh)
{
    m_mesh = std::move(mesh);
}

IFace * CFaceAccessor::GetCurrentFace()
{
    return new CFace(m_mesh->getFace(m_index));
}

GladiusLib_uint64 CFaceAccessor::GetSize()
{
    return m_mesh->getNumberOfFaces();
}

bool CFaceAccessor::Next()
{
    if (m_index + 2 < GetSize())
    {
        ++m_index;
        return true;
    }

    return false;
}

bool CFaceAccessor::Prev()
{
    if (m_index > 0)
    {
        --m_index;
        return true;
    }

    return false;
}

void CFaceAccessor::Begin()
{
    m_index = 0u;
}
