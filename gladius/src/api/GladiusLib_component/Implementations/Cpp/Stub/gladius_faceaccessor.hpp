/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is the class declaration of CFaceAccessor

*/

#ifndef __GLADIUSLIB_FACEACCESSOR
#define __GLADIUSLIB_FACEACCESSOR

#include "gladius_interfaces.hpp"

// Parent classes
#include "Mesh.h"
#include "gladius_face.hpp"
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
         Class declaration of CFaceAccessor
        **************************************************************************************************************************/

        class CFaceAccessor : public virtual IFaceAccessor, public virtual CResourceAccessor
        {
          public:
            /**
             * Put additional public members here. They will not be visible in the external API.
             */

            void setMesh(gladius::SharedMesh mesh);
            /**
             * Public member functions to implement.
             */

            IFace * GetCurrentFace() override;
            GladiusLib_uint64 GetSize() override;
            bool Next() override;
            bool Prev() override;
            void Begin() override;

          private:
            gladius::SharedMesh m_mesh{};
            size_t m_index{};
        };

    } // namespace Impl
} // namespace GladiusLib

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __GLADIUSLIB_FACEACCESSOR
