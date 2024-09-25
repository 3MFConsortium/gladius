/*++

Copyright (C) 2022 Jan Orend

All rights reserved.

Abstract: This is a stub class definition of CChannelAccessor

*/

#include "gladius_channelaccessor.hpp"

#include "Document.h"
#include "gladius_interfaceexception.hpp"

// Include custom headers here.

using namespace GladiusLib::Impl;

/*************************************************************************************************************************
 Class definition of CChannelAccessor
**************************************************************************************************************************/

CChannelAccessor::CChannelAccessor(gladius::Document * doc)
      : m_doc(doc)
  {
    m_iter = m_doc->getBitmapChannels().begin();
    m_bitmap.reset();
  }


GladiusLib::sChannelMetaInfo CChannelAccessor::GetMetaInfo()
{
    if (!m_bitmap)
    {
        throw EGladiusLibInterfaceException(
          GLADIUSLIB_ERROR_GENERICEXCEPTION,
          "Evaluate has to be called before calling GetMetaInfo()");
    }
    sChannelMetaInfo metaInfo;

    metaInfo.m_MinPosition[0] = m_bitmap->position.x();
    metaInfo.m_MinPosition[1] = m_bitmap->position.y();

    metaInfo.m_MaxPosition[0] =
      m_bitmap->position.x() + m_bitmap->width_px * m_bitmap->pixelSize.x();
    metaInfo.m_MaxPosition[1] =
      m_bitmap->position.y() + m_bitmap->height_px* m_bitmap->pixelSize.y();

    metaInfo.m_Size[0] = static_cast<GladiusLib_int32>(m_bitmap->width_px);
    metaInfo.m_Size[1] = static_cast<GladiusLib_int32>(m_bitmap->height_px);

    metaInfo.m_RequiredMemory = static_cast<GladiusLib_int32>(
      m_bitmap->bitmapData.size() * sizeof(float));

    return metaInfo;
}

void CChannelAccessor::Copy(const GladiusLib_int64 nTargetPtr)
{

    auto & src = m_bitmap->bitmapData;

    float * dest = reinterpret_cast<float*>(nTargetPtr);
    std::copy(src.cbegin(), src.cend(), dest);
}

std::string CChannelAccessor::GetName()
{
    return m_iter->getName();
}

bool CChannelAccessor::SwitchToChannel(const std::string & sName)
{
    m_bitmap.reset();
    auto & channels = m_doc->getBitmapChannels();

    auto iter = std::find_if(channels.begin(),
                             channels.end(),
                             [sName](auto & channel) { return channel.getName() == sName; });

    if (iter == channels.end())
    {
        return false;
    }

    m_iter = iter;
    return true;
}

GladiusLib_uint64 CChannelAccessor::GetSize()
{
    return m_doc->getBitmapChannels().size();
}

bool CChannelAccessor::Next()
{
    if (std::next(m_iter) == m_doc->getBitmapChannels().end())
    {
        return false;
    }
    ++m_iter;
    m_bitmap.reset();
    return true;
}

bool CChannelAccessor::Prev()
{
    if (m_iter == m_doc->getBitmapChannels().begin())
    {
        return false;
    }
    --m_iter;
    m_bitmap.reset();
    return true;
}

void CChannelAccessor::Begin()
{
    m_iter = m_doc->getBitmapChannels().begin();
    m_bitmap.reset();
}

void CChannelAccessor::Evaluate(const GladiusLib_single fZ_mm, const GladiusLib_single fPixelWidth_mm,
    const GladiusLib_single fPixelHeight_mm)
{
    auto bitmap = m_iter->generate(fZ_mm, {fPixelWidth_mm, fPixelHeight_mm});
    m_bitmap.emplace(std::move(bitmap));
}
