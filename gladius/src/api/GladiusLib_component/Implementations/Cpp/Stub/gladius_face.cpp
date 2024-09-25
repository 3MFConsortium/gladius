/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is a stub class definition of CFace

*/

#include "gladius_face.hpp"

#include "gladius_interfaceexception.hpp"
#include <utility>

// Include custom headers here.

using namespace GladiusLib::Impl;

/*************************************************************************************************************************
 Class definition of CFace
**************************************************************************************************************************/

GladiusLib::sVector3f toSVector3f(gladius::Vector3 vector)
{
    return GladiusLib::sVector3f{vector.x(), vector.y(), vector.z()};
}

CFace::CFace(gladius::Face face)
    : m_face(std::move(face))
{
}

GladiusLib::sVector3f CFace::GetVertexA()
{
    return toSVector3f(m_face.vertices[0]);
}

GladiusLib::sVector3f CFace::GetVertexB()
{
    return toSVector3f(m_face.vertices[1]);
}

GladiusLib::sVector3f CFace::GetVertexC()
{
    return toSVector3f(m_face.vertices[2]);
}

GladiusLib::sVector3f CFace::GetNormal()
{
    return toSVector3f(m_face.normal);
}

GladiusLib::sVector3f CFace::GetNormalA()
{
    return toSVector3f(m_face.vertexNormals[0]);
}

GladiusLib::sVector3f CFace::GetNormalB()
{
    return toSVector3f(m_face.vertexNormals[1]);
}

GladiusLib::sVector3f CFace::GetNormalC()
{
    return toSVector3f(m_face.vertexNormals[2]);
}
