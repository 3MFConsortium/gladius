/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is a stub class definition of CBoundingBox

*/

#include "gladius_boundingbox.hpp"
#include "gladius_interfaceexception.hpp"

// Include custom headers here.


using namespace GladiusLib::Impl;

/*************************************************************************************************************************
 Class definition of CBoundingBox 
**************************************************************************************************************************/

GladiusLib::sVector3f CBoundingBox::GetMin()
{
    return m_min;
}

GladiusLib::sVector3f CBoundingBox::GetMax()
{
    return m_max;
}

void CBoundingBox::setMin(sGladiusLibVector3f min)
{
    m_min = min;
}

void CBoundingBox::setMax(sGladiusLibVector3f max)
{
    m_max = max;
}
