/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is the class declaration of CContourAccessor

*/

#ifndef __GLADIUSLIB_CONTOURACCESSOR
#define __GLADIUSLIB_CONTOURACCESSOR

#include "gladius_interfaces.hpp"

// Parent classes
#include "gladius_resourceaccessor.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "Contour.h"

namespace GladiusLib
{
    namespace Impl
    {

        /*************************************************************************************************************************
         Class declaration of CContourAccessor
        **************************************************************************************************************************/

        class CContourAccessor : public virtual IContourAccessor, public virtual CResourceAccessor
        {

          protected:
            /**
             * Put protected members here.
             */

          public:
            /**
             * Put additional public members here. They will not be visible in the external API.
             */
            void setContour(gladius::SharedPolyLines resource);

            /**
             * Public member functions to implement.
             */

            IPolygonAccessor * GetCurrentPolygon() override;
            GladiusLib_uint64 GetSize() override;
            bool Next() override;
            bool Prev() override;
            void Begin() override;

        private:
            gladius::SharedPolyLines m_polyLines;
            gladius::PolyLines::const_iterator m_iter;
        };

    } // namespace Impl
} // namespace GladiusLib

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __GLADIUSLIB_CONTOURACCESSOR
