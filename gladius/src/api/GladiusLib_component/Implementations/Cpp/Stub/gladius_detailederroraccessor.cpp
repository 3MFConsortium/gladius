/*++

Copyright (C) 2022 Jan Orend

All rights reserved.

Abstract: This is a stub class definition of CDetailedErrorAccessor

*/

#include "gladius_detailederroraccessor.hpp"
#include "gladius_interfaceexception.hpp"

// Include custom headers here.

using namespace GladiusLib::Impl;

/*************************************************************************************************************************
 Class definition of CDetailedErrorAccessor
**************************************************************************************************************************/

void CDetailedErrorAccessor::setLogger(gladius::events::SharedLogger logger)
{
    m_sharedLogger = std::move(logger);
    Begin();
}

GladiusLib_uint64 CDetailedErrorAccessor::GetSize()
{
    if (!m_sharedLogger)
    {
        return 0u;
    }

    return m_sharedLogger->size();
}

bool CDetailedErrorAccessor::Next()
{
    if (!m_sharedLogger)
    {
        return false;
    }

    if (std::next(m_iter) == m_sharedLogger->end())
    {
        return false;
    }
    ++m_iter;
    return true;
}

bool CDetailedErrorAccessor::Prev()
{
    if (!m_sharedLogger)
    {
        return false;
    }

    if (m_iter == m_sharedLogger->begin())
    {
        return false;
    }
    --m_iter;
    return true;
}

void CDetailedErrorAccessor::Begin()
{
    if (!m_sharedLogger)
    {
        return;
    }
    m_iter = m_sharedLogger->begin();
}

std::string CDetailedErrorAccessor::GetTitle()
{
    if (!m_sharedLogger)
    {
        return {"No logger set"};
    }

    return m_iter->getMessage();
}

std::string CDetailedErrorAccessor::GetDescription()
{
    if (!m_sharedLogger)
    {
        return {"No logger set"};
    }

    return m_iter->getMessage();
}

GladiusLib_uint32 CDetailedErrorAccessor::GetSeverity()
{
    if (!m_sharedLogger)
    {
        return 2u;
    }
    switch (m_iter->getSeverity())
    {
    case gladius::events::Severity::Info:
        return 0u;
    case gladius::events::Severity::Warning:
        return 1u;
    case gladius::events::Severity::Error:
    case gladius::events::Severity::FatalError:
        return 2u;
    }
    return 2u;
}
