/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is the class declaration of CFace

*/

#ifndef __GLADIUSLIB_FACE
#define __GLADIUSLIB_FACE

#include <gladius_interfaces.hpp>

// Parent classes
#include "Mesh.h"
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
         Class declaration of CFace
        **************************************************************************************************************************/

        class CFace : public virtual IFace, public virtual CBase
        {
          public:
            CFace(gladius::Face face);
            ;

            /**
             * Put additional public members here. They will not be visible in the external API.
             */

            /**
             * Public member functions to implement.
             */

            GladiusLib::sVector3f GetVertexA() override;
            GladiusLib::sVector3f GetVertexB() override;
            GladiusLib::sVector3f GetVertexC() override;
            GladiusLib::sVector3f GetNormal() override;
            GladiusLib::sVector3f GetNormalA() override;
            GladiusLib::sVector3f GetNormalB() override;
            GladiusLib::sVector3f GetNormalC() override;

          private:
            gladius::Face m_face;
        };

    } // namespace Impl
} // namespace GladiusLib

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __GLADIUSLIB_FACE
