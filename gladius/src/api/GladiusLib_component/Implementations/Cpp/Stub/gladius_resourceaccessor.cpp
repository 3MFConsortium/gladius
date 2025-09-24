/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is a stub class definition of CResourceAccessor

*/

#include "gladius_resourceaccessor.hpp"
#include "gladius_interfaceexception.hpp"

// Include custom headers here.

using namespace GladiusLib::Impl;

/*************************************************************************************************************************
 Class definition of CResourceAccessor
**************************************************************************************************************************/

GladiusLib_uint64 CResourceAccessor::GetSize()
{
    throw EGladiusLibInterfaceException(GLADIUSLIB_ERROR_NOTIMPLEMENTED);
}

bool CResourceAccessor::Next()
{
    throw EGladiusLibInterfaceException(GLADIUSLIB_ERROR_NOTIMPLEMENTED);
}

bool CResourceAccessor::Prev()
{
    throw EGladiusLibInterfaceException(GLADIUSLIB_ERROR_NOTIMPLEMENTED);
}

void CResourceAccessor::Begin()
{
    throw EGladiusLibInterfaceException(GLADIUSLIB_ERROR_NOTIMPLEMENTED);
}
