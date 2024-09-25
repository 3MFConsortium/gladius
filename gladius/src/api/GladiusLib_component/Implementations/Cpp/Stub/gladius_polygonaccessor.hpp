/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is the class declaration of CPolygonAccessor

*/

#ifndef __GLADIUSLIB_POLYGONACCESSOR
#define __GLADIUSLIB_POLYGONACCESSOR

#include "gladius_interfaces.hpp"

// Parent classes
#include "Contour.h"
#include "gladius_resourceaccessor.hpp"
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
         Class declaration of CPolygonAccessor
        **************************************************************************************************************************/

        class CPolygonAccessor : public virtual IPolygonAccessor, public virtual CResourceAccessor
        {

          public:
            
            void setPolygon(gladius::SharedPolyLines polyLines,
                            gladius::PolyLines::const_iterator iterToPolyLine);

            
            GladiusLib::sVector2f GetCurrentVertex() override;
            GladiusLib_uint64 GetSize() override;
            bool Next() override;
            bool Prev() override;
            GladiusLib_single GetArea() override;
            void Begin() override;

        protected:
            
          private:
            gladius::SharedPolyLines m_polyLines;
            gladius::PolyLines::const_iterator m_polyLineIter;
            gladius::Vertices2d::const_iterator m_iter;
            float m_area {0.f};
        };

    } // namespace Impl
} // namespace GladiusLib

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __GLADIUSLIB_POLYGONACCESSOR
