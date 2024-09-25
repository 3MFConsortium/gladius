/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is the class declaration of CGladius

*/

#ifndef __GLADIUSLIB_GLADIUS
#define __GLADIUSLIB_GLADIUS

#include "gladius_interfaces.hpp"

// Parent classes
#include "gladius_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "ComputeContext.h"
#include "compute/ComputeCore.h"
#include "Document.h"

namespace GladiusLib
{
    namespace Impl
    {

        class CGladius : public virtual IGladius, public virtual CBase
        {
          public:
            void LoadAssembly(const std::string & sValue) override;
            void throwIfDocIsEmpty() const;

            void ExportSTL(const std::string & sValue) override;

            GladiusLib_single GetFloatParameter(const std::string & sModelName,
                                                const std::string & sNodeName,
                                                const std::string & sParameterName) override;

            void SetFloatParameter(const std::string & sModelName,
                                   const std::string & sNodeName,
                                   const std::string & sParameterName,
                                   const GladiusLib_single Value) override;

            std::string GetStringParameter(const std::string & sModelName,
                                           const std::string & sNodeName,
                                           const std::string & sParameterName) override;

            void SetStringParameter(const std::string & sModelName,
                                    const std::string & sNodeName,
                                    const std::string & sParameterName,
                                    const std::string & sValue) override;

            GladiusLib::sVector3f GetVector3fParameter(const std::string & sModelName,
                                                       const std::string & sNodeName,
                                                       const std::string & sParameterName) override;

            void SetVector3fParameter(const std::string & sModelName,
                                      const std::string & sNodeName,
                                      const std::string & sParameterName,
                                      const GladiusLib_single fX,
                                      const GladiusLib_single fY,
                                      const GladiusLib_single fZ) override;

            IContourAccessor * GenerateContour(const GladiusLib_single fZ,
                                               const GladiusLib_single fOffset) override;

            IBoundingBox * ComputeBoundingBox() override;

            IFaceAccessor * GeneratePreviewMesh() override;

            IChannelAccessor * GetChannels() override;

            IDetailedErrorAccessor * GetDetailedErrorAccessor() override;

            void ClearDetailedErrors() override;

            void InjectSmoothingKernel(const std::string & sKernel) override;

          private:
            void initCoreOrThrow();

          private:
            std::unique_ptr<gladius::Document> m_doc;
            std::shared_ptr<gladius::ComputeCore> m_core;
            gladius::SharedComputeContext m_computeContext;
        };

    } // namespace Impl
} // namespace GladiusLib

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __GLADIUSLIB_GLADIUS
