/*++

Copyright (C) 2022 Jan Orend

All rights reserved.

Abstract: This is the class declaration of CDetailedErrorAccessor

*/

#ifndef __GLADIUSLIB_DETAILEDERRORACCESSOR
#define __GLADIUSLIB_DETAILEDERRORACCESSOR

#include "gladius_interfaces.hpp"

// Parent classes
#include "EventLogger.h"
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
         Class declaration of CDetailedErrorAccessor
        **************************************************************************************************************************/

        class CDetailedErrorAccessor : public virtual IDetailedErrorAccessor, public virtual CBase
        {
          private:
            gladius::events::SharedLogger m_sharedLogger;
            gladius::events::Events::iterator m_iter;
          protected:
            /**
             * Put protected members here.
             */

          public:
            /**
             * Put additional public members here. They will not be visible in the external API.
             */

            /// <summary>
            /// Set the logger to use as source.
            /// </summary>
            /// <param name="logger"></param>
            void setLogger(gladius::events::SharedLogger logger);

            /**
             * Public member functions to implement.
             */

            GladiusLib_uint64 GetSize() override;

            bool Next() override;

            bool Prev() override;

            void Begin() override;

            std::string GetTitle() override;

            std::string GetDescription() override;

            GladiusLib_uint32 GetSeverity() override;

        };

    } // namespace Impl
} // namespace GladiusLib

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __GLADIUSLIB_DETAILEDERRORACCESSOR
