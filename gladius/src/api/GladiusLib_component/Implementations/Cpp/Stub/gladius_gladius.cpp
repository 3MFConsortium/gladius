/*++

Copyright (C) 2021 Jan Orend

All rights reserved.

Abstract: This is a stub class definition of CGladius

*/

#include "gladius_gladius.hpp"
#include "gladius_interfaceexception.hpp"

// Include custom headers here.
#include "gladius_boundingbox.hpp"
#include "gladius_channelaccessor.hpp"
#include "gladius_contouraccessor.hpp"
#include "gladius_detailederroraccessor.hpp"
#include "gladius_faceaccessor.hpp"
#include "io/MeshExporter.h"

using namespace GladiusLib::Impl;

/*************************************************************************************************************************
 Class definition of CGladius
**************************************************************************************************************************/

void CGladius::initCoreOrThrow()
{
    using namespace gladius;
    if (m_core)
    {
        return;
    }
    m_computeContext = std::make_shared<ComputeContext>(EnableGLOutput::disabled);

    if (m_computeContext->isValid() == 0)
    {
        std::cerr << "Failed creating OpenCL Context. Did you install proper GPU drivers?\n";
        throw std::runtime_error(
          "Failed creating OpenCL Context. Did you install proper GPU drivers?");
    }

    events::SharedLogger logger = std::make_shared<events::Logger>();

    m_core = std::make_shared<ComputeCore>(
      m_computeContext, gladius::RequiredCapabilities::ComputeOnly, logger);

    m_core->setCodeGenerator(gladius::CodeGenerator::Code); // faster, but takes longer to compile
    m_doc = std::make_unique<Document>(m_core);
    logger->addEvent({"Core and document created", events::Severity::Info});
}

void CGladius::LoadAssembly(const std::string & filename)
{
    initCoreOrThrow();
    m_doc->load(filename);
}

void CGladius::throwIfDocIsEmpty() const
{
    if (!m_doc)
    {
        std::cerr << "Failed to export, no assembly loaded\n";
        throw std::runtime_error("Failed to export, no assembly loaded");
    }
}

void CGladius::ExportSTL(const std::string & filename)
{
    using namespace gladius;
    throwIfDocIsEmpty();
    m_doc->exportAsStl(filename);
}

GladiusLib_single CGladius::GetFloatParameter(const std::string & sModelName,
                                              const std::string & sNodeName,
                                              const std::string & sParameterName)
{

    throwIfDocIsEmpty();

    gladius::ResourceId resourceId = std::stoul(sModelName);
    return m_doc->getFloatParameter(resourceId, sNodeName, sParameterName);
}

void CGladius::SetFloatParameter(const std::string & sModelName,
                                 const std::string & sNodeName,
                                 const std::string & sParameterName,
                                 const GladiusLib_single Value)
{
    throwIfDocIsEmpty();

    gladius::ResourceId resourceId = std::stoul(sModelName);
    m_doc->setFloatParameter(resourceId, sNodeName, sParameterName, Value);
}

std::string CGladius::GetStringParameter(const std::string & sModelName,
                                         const std::string & sNodeName,
                                         const std::string & sParameterName)
{
    throwIfDocIsEmpty();

    gladius::ResourceId resourceId = std::stoul(sModelName);
    return m_doc->getStringParameter(resourceId, sNodeName, sParameterName);
}

void CGladius::SetStringParameter(const std::string & sModelName,
                                  const std::string & sNodeName,
                                  const std::string & sParameterName,
                                  const std::string & sValue)
{
    throwIfDocIsEmpty();

    gladius::ResourceId resourceId = std::stoul(sModelName);
    m_doc->setStringParameter(resourceId, sNodeName, sParameterName, sValue);
}

GladiusLib::sVector3f CGladius::GetVector3fParameter(const std::string & sModelName,
                                                     const std::string & sNodeName,
                                                     const std::string & sParameterName)
{
    throwIfDocIsEmpty();

    gladius::ResourceId resourceId = std::stoul(sModelName);
    auto const vector = m_doc->getVector3fParameter(resourceId, sNodeName, sParameterName);
    return sVector3f{vector.x, vector.y, vector.z};
}

void CGladius::SetVector3fParameter(const std::string & sModelName,
                                    const std::string & sNodeName,
                                    const std::string & sParameterName,
                                    const GladiusLib_single fX,
                                    const GladiusLib_single fY,
                                    const GladiusLib_single fZ)
{
    throwIfDocIsEmpty();

    gladius::ResourceId resourceId = std::stoul(sModelName);
    m_doc->setVector3fParameter(resourceId, sNodeName, sParameterName, {fX, fY, fZ});
}

IContourAccessor * CGladius::GenerateContour(const GladiusLib_single fZ,
                                             const GladiusLib_single fOffset)
{
    gladius::nodes::SliceParameter sliceParameter;
    sliceParameter.zHeight_mm = fZ;
    auto contour = std::make_shared<gladius::PolyLines>(m_doc->generateContour(fZ, sliceParameter));
    auto * const contourAccessor = new CContourAccessor();
    contourAccessor->setContour(std::move(contour));
    return contourAccessor;
}

IBoundingBox * CGladius::ComputeBoundingBox()
{
    auto const bbox = m_doc->computeBoundingBox();
    auto * const newBBox = new CBoundingBox();
    newBBox->setMax({bbox.max.x, bbox.max.y, bbox.max.z});
    newBBox->setMin({bbox.min.x, bbox.min.y, bbox.min.z});

    return newBBox;
}

IFaceAccessor * CGladius::GeneratePreviewMesh()
{
    auto mesh = std::make_shared<gladius::Mesh>(m_doc->generateMesh());
    auto * const faceAccessor = new CFaceAccessor();
    faceAccessor->setMesh(mesh);
    return faceAccessor;
}

IChannelAccessor * CGladius::GetChannels()
{
    return new CChannelAccessor(m_doc.get());
}

IDetailedErrorAccessor * CGladius::GetDetailedErrorAccessor()
{
    auto eventLogger = m_doc->getSharedLogger();
    if (!eventLogger)
    {
        return nullptr;
    }

    auto * const detailedErrorAccessor = new CDetailedErrorAccessor();
    detailedErrorAccessor->setLogger(eventLogger);
    return detailedErrorAccessor;
}

void CGladius::ClearDetailedErrors()
{
    auto eventLogger = m_doc->getSharedLogger();
    if (!eventLogger)
    {
        return;
    }
    eventLogger->clear();
}

void CGladius::InjectSmoothingKernel(const std::string & sKernel)
{
    throwIfDocIsEmpty();
    m_doc->injectSmoothingKernel(sKernel);
}
