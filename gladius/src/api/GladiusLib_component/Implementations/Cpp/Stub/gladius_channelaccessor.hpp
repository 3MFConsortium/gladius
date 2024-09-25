/*++

Copyright (C) 2022 Jan Orend

All rights reserved.

Abstract: This is the class declaration of CChannelAccessor

*/


#ifndef __GLADIUSLIB_CHANNELACCESSOR
#define __GLADIUSLIB_CHANNELACCESSOR

#include "gladius_interfaces.hpp"

// Parent classes
#include <optional>

#include "BitmapChannel.h"
#include "gladius_resourceaccessor.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.


namespace gladius
{
    class Document;
}

namespace GladiusLib {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CChannelAccessor 
**************************************************************************************************************************/

class CChannelAccessor : public virtual IChannelAccessor, public virtual CResourceAccessor {
private:

    /**
    * Put private members here.
    */

protected:

    /**
    * Put protected members here.
    */

public:
  CChannelAccessor(gladius::Document * doc);
    /**
    * Put additional public members here. They will not be visible in the external API.
    */


    /**
    * Public member functions to implement.
    */
    
    GladiusLib::sChannelMetaInfo GetMetaInfo() override;

    void Copy(const GladiusLib_int64 nTargetPtr) override;

   
    std::string GetName() override;

    bool SwitchToChannel(const std::string & sName) override;
    GladiusLib_uint64 GetSize() override;
    bool Next() override;
    bool Prev() override;
    void Begin() override;
    void Evaluate(const GladiusLib_single fZ_mm, const GladiusLib_single fPixelWidth_mm,
        const GladiusLib_single fPixelHeight_mm) override;


private:
    gladius::Document * m_doc = nullptr;
    gladius::BitmapChannels::iterator m_iter;
    std::optional<gladius::BitmapLayer> m_bitmap;
    
};

} // namespace Impl
} // namespace GladiusLib

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __GLADIUSLIB_CHANNELACCESSOR
