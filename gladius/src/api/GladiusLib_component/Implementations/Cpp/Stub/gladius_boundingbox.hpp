/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is the class declaration of CBoundingBox

*/

#ifndef __GLADIUSLIB_BOUNDINGBOX
#define __GLADIUSLIB_BOUNDINGBOX

#include "gladius_interfaces.hpp"

// Parent classes
#include "gladius_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.

namespace GladiusLib
{
    namespace Impl
    {

        /*************************************************************************************************************************
         Class declaration of CBoundingBox
        **************************************************************************************************************************/

        class CBoundingBox : public virtual IBoundingBox, public virtual CBase
        {
          public:
            GladiusLib::sVector3f GetMin() override;
            GladiusLib::sVector3f GetMax() override;

            void setMin(sGladiusLibVector3f min);
            void setMax(sGladiusLibVector3f max);
          private:
            sGladiusLibVector3f m_min;
            sGladiusLibVector3f m_max;

        };

    } // namespace Impl
} // namespace GladiusLib

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __GLADIUSLIB_BOUNDINGBOX
