#include "Builder.h"

#include "Document.h"

namespace gladius::nodes
{
    void Builder::addResourceRef(Model & target,
                                 ResourceKey const & resourceKey,
                                 nodes::Port & coordinateSystemPort)
    {
        nodes::Resource resourceNodeType;
        auto resourceNode = target.create(resourceNodeType);
        resourceNode->parameter().at(FieldNames::ResourceId) =
          VariantParameter(resourceKey.getResourceId().value_or(0));

        nodes::SignedDistanceToMesh importedGeometryType;
        auto importNode = target.create(importedGeometryType);

        importNode->parameter().at(FieldNames::Pos).setInputFromPort(coordinateSystemPort);

        auto & meshInput = importNode->parameter().at(FieldNames::Mesh);
        meshInput.setInputFromPort(resourceNode->getOutputs().at(FieldNames::Value));

        auto & resourceShapePort = importNode->getOutputs().at(FieldNames::Distance);
        auto lastShapePort = getLastShape(target);
        auto shapeSink = target.getEndNode()->getParameter(FieldNames::Shape);

        if (!shapeSink)
        {
            throw std::runtime_error("End node is required to have a shape parameter");
        }
        if (!lastShapePort)
        {
            shapeSink->setInputFromPort(resourceShapePort);
            return;
        }
        auto uniteNode = target.create<nodes::Min>();

        uniteNode->parameter().at(FieldNames::A).setInputFromPort(*lastShapePort);
        uniteNode->parameter().at(FieldNames::B).setInputFromPort(resourceShapePort);

        shapeSink->setInputFromPort(uniteNode->getOutputs().at(FieldNames::Result));
    }

    void Builder::addBeamLatticeRef(Model & target,
                                    ResourceKey const & resourceKey,
                                    nodes::Port & coordinateSystemPort)
    {
        nodes::Resource resourceNodeType;
        auto resourceNode = target.create(resourceNodeType);
        resourceNode->parameter().at(FieldNames::ResourceId) =
          VariantParameter(resourceKey.getResourceId().value_or(0));

        nodes::SignedDistanceToBeamLattice importedGeometryType;
        auto importNode = target.create(importedGeometryType);

        importNode->parameter().at(FieldNames::Pos).setInputFromPort(coordinateSystemPort);

        auto & beamLatticeInput = importNode->parameter().at(FieldNames::BeamLattice);
        beamLatticeInput.setInputFromPort(resourceNode->getOutputs().at(FieldNames::Value));

        auto & resourceShapePort = importNode->getOutputs().at(FieldNames::Distance);
        auto lastShapePort = getLastShape(target);
        auto shapeSink = target.getEndNode()->getParameter(FieldNames::Shape);

        if (!shapeSink)
        {
            throw std::runtime_error("End node is required to have a shape parameter");
        }
        if (!lastShapePort)
        {
            shapeSink->setInputFromPort(resourceShapePort);
            return;
        }
        auto uniteNode = target.create<nodes::Min>();

        uniteNode->parameter().at(FieldNames::A).setInputFromPort(*lastShapePort);
        uniteNode->parameter().at(FieldNames::B).setInputFromPort(resourceShapePort);

        shapeSink->setInputFromPort(uniteNode->getOutputs().at(FieldNames::Result));
    }

    void Builder::addBoundingBox(Model & target,
                                 BoundingBox const & boundingBox,
                                 nodes::Port & coordinateSystemPort)
    {
        auto boxNode = target.create<BoxMinMax>();

        boxNode->parameter().at(FieldNames::Pos).setInputFromPort(coordinateSystemPort);

        auto minVecNode = target.create<nodes::ConstantVector>();
        minVecNode->parameter().at(FieldNames::X) = VariantParameter(boundingBox.min.x);
        minVecNode->parameter().at(FieldNames::Y) = VariantParameter(boundingBox.min.y);
        minVecNode->parameter().at(FieldNames::Z) = VariantParameter(boundingBox.min.z);

        auto maxVecNode = target.create<nodes::ConstantVector>();
        maxVecNode->parameter().at(FieldNames::X) = VariantParameter(boundingBox.max.x);
        maxVecNode->parameter().at(FieldNames::Y) = VariantParameter(boundingBox.max.y);
        maxVecNode->parameter().at(FieldNames::Z) = VariantParameter(boundingBox.max.z);

        boxNode->parameter()
          .at(FieldNames::Min)
          .setInputFromPort(minVecNode->getOutputs().at(FieldNames::Vector));
        boxNode->parameter()
          .at(FieldNames::Max)
          .setInputFromPort(maxVecNode->getOutputs().at(FieldNames::Vector));

        auto lastShapePort = getLastShape(target);
        auto shapeSink = target.getEndNode()->getParameter(FieldNames::Shape);

        if (!shapeSink)
        {
            throw std::runtime_error("End node is required to have a shape parameter");
        }

        if (!lastShapePort)
        {
            shapeSink->setInputFromPort(boxNode->getOutputs().at(FieldNames::Shape));
            return;
        }

        // Union the bounding box with existing shapes using Min node (for SDFs, Min = union)
        auto uniteNode = target.create<nodes::Min>();

        uniteNode->parameter().at(FieldNames::A).setInputFromPort(*lastShapePort);
        uniteNode->parameter()
          .at(FieldNames::B)
          .setInputFromPort(boxNode->getOutputs().at(FieldNames::Shape));

        shapeSink->setInputFromPort(uniteNode->getOutputs().at(FieldNames::Result));
    }

    void Builder::addComponentRef(Model & target,
                                  Model & referencedModel,
                                  Matrix4x4 const & transformation)
    {

        auto coordinateSystemPort = addTransformationToInputCs(target, transformation);

        nodes::Resource resourceNodeType;
        auto resourceNode = target.create(resourceNodeType);
        resourceNode->parameter().at(FieldNames::ResourceId) =
          VariantParameter(referencedModel.getResourceId());

        nodes::FunctionCall partType;
        auto functionCallNode = target.create(partType);
        functionCallNode->updateInputsAndOutputs(referencedModel); // Neccessary to add the ports

        functionCallNode->parameter().at(FieldNames::Pos).setInputFromPort(coordinateSystemPort);

        functionCallNode->parameter()
          .at(FieldNames::FunctionId)
          .setInputFromPort(resourceNode->getOutputs().at(FieldNames::Value));

        functionCallNode->updateInputsAndOutputs(referencedModel);
        target.registerInputs(*functionCallNode);
        target.registerOutputs(*functionCallNode);

        auto resourceShapePort = functionCallNode->getOutputs().at(FieldNames::Shape);
        auto lastShapePort = getLastShape(target);
        auto shapeSink = target.getEndNode()->getParameter(FieldNames::Shape);

        if (!shapeSink)
        {
            throw std::runtime_error("End node is required to have a shape parameter");
        }
        if (!lastShapePort)
        {
            shapeSink->setInputFromPort(resourceShapePort);
            return;
        }

        auto uniteNode = target.create<nodes::Min>();

        uniteNode->parameter().at(FieldNames::A).setInputFromPort(*lastShapePort);
        uniteNode->parameter().at(FieldNames::B).setInputFromPort(resourceShapePort);

        shapeSink->setInputFromPort(uniteNode->getOutputs().at(FieldNames::Result));
    }

    void Builder::appendIntersectionWithFunction(Model & target,
                                                 Model & referencedModel,
                                                 nodes::Port & coordinateSystemPort,
                                                 std::string const & sdfChannelName)
    {
        nodes::Resource resourceNodeType;
        auto resourceNode = target.create(resourceNodeType);
        resourceNode->parameter()[FieldNames::ResourceId] =
          VariantParameter(referencedModel.getResourceId());

        nodes::FunctionCall partType;
        auto functionCallNode = target.create(partType);

        functionCallNode->parameter()
          .at(FieldNames::FunctionId)
          .setInputFromPort(resourceNode->getOutputs().at(FieldNames::Value));

        functionCallNode->updateInputsAndOutputs(referencedModel);
        target.registerInputs(*functionCallNode);
        target.registerOutputs(*functionCallNode);

        // check if partNode has a pos parameter
        auto iterPosInput = functionCallNode->parameter().find(FieldNames::Pos);
        if (iterPosInput == std::end(functionCallNode->parameter()))
        {
            throw std::runtime_error("Entry function has no pos input");
        }

        iterPosInput->second.setInputFromPort(coordinateSystemPort);

        // check, if part has a shape
        auto iterShapeOutput = functionCallNode->getOutputs().find(sdfChannelName);
        if (iterShapeOutput == std::end(functionCallNode->getOutputs()))
        {
            throw std::runtime_error(
              fmt::format("Entry function has no output with the name {}", sdfChannelName));
        }

        if (iterShapeOutput->second.getTypeIndex() != nodes::ParameterTypeIndex::Float)
        {
            throw std::runtime_error(fmt::format("The output {} is not a scalar", sdfChannelName));
        }

        auto & resourceShapePort = iterShapeOutput->second;

        auto lastShapePort = getLastShape(target);
        auto shapeSink = target.getEndNode()->getParameter(FieldNames::Shape);

        if (!shapeSink)
        {
            throw std::runtime_error("End node is required to have a shape parameter");
        }
        if (!lastShapePort)
        {
            shapeSink->setInputFromPort(resourceShapePort);
            return;
        }

        nodes::Min unionType;
        auto unionNode = target.create(unionType);

        unionNode->parameter().at(FieldNames::A).setInputFromPort(*lastShapePort);
        unionNode->parameter().at(FieldNames::B).setInputFromPort(resourceShapePort);

        shapeSink->setInputFromPort(unionNode->getOutputs().at(FieldNames::Result));
    }

    void Builder::intersectFunctionWithDomain(Model & target,
                                              Model & referencedModel,
                                              nodes::Port & coordinateSystemPort,
                                              std::string const & sdfChannelName)
    {
        nodes::Resource resourceNodeType;
        auto resourceNode = target.create(resourceNodeType);
        resourceNode->parameter()[FieldNames::ResourceId] =
          VariantParameter(referencedModel.getResourceId());

        nodes::FunctionCall partType;
        auto functionCallNode = target.create(partType);

        functionCallNode->parameter()
          .at(FieldNames::FunctionId)
          .setInputFromPort(resourceNode->getOutputs().at(FieldNames::Value));

        functionCallNode->updateInputsAndOutputs(referencedModel);
        target.registerInputs(*functionCallNode);
        target.registerOutputs(*functionCallNode);

        // check if partNode has a pos parameter
        auto iterPosInput = functionCallNode->parameter().find(FieldNames::Pos);
        if (iterPosInput == std::end(functionCallNode->parameter()))
        {
            throw std::runtime_error("Entry function has no pos input");
        }

        iterPosInput->second.setInputFromPort(coordinateSystemPort);

        // check, if part has a shape
        auto iterShapeOutput = functionCallNode->getOutputs().find(sdfChannelName);
        if (iterShapeOutput == std::end(functionCallNode->getOutputs()))
        {
            throw std::runtime_error(
              fmt::format("Entry function has no output with the name {}", sdfChannelName));
        }

        if (iterShapeOutput->second.getTypeIndex() != nodes::ParameterTypeIndex::Float)
        {
            throw std::runtime_error(fmt::format("The output {} is not a scalar", sdfChannelName));
        }

        auto & functionShapePort = iterShapeOutput->second;

        auto lastShapePort = getLastShape(target);
        auto shapeSink = target.getEndNode()->getParameter(FieldNames::Shape);

        if (!shapeSink)
        {
            throw std::runtime_error("End node is required to have a shape parameter");
        }
        if (!lastShapePort)
        {
            // If no previous shape (bounding box), just set the function output directly
            shapeSink->setInputFromPort(functionShapePort);
            return;
        }

        // Intersect the function with the existing shape (bounding box) using Max node
        // In SDF: Max = intersection (farther surface wins, effectively clipping the function to
        // the domain)
        nodes::Max intersectionType;
        auto intersectionNode = target.create(intersectionType);

        intersectionNode->parameter().at(FieldNames::A).setInputFromPort(*lastShapePort);
        intersectionNode->parameter().at(FieldNames::B).setInputFromPort(functionShapePort);

        shapeSink->setInputFromPort(intersectionNode->getOutputs().at(FieldNames::Result));
    }

    void Builder::addLevelSetWithDomain(Model & target,
                                        Model & referencedModel,
                                        nodes::Port & functionCoordinateSystemPort,
                                        std::string const & sdfChannelName,
                                        BoundingBox const & boundingBox,
                                        nodes::Port & domainCoordinateSystemPort)
    {
        // First, create the bounding box for this specific level set
        auto boundingBoxNode = target.create<nodes::BoxMinMax>();
        boundingBoxNode->parameter()
          .at(FieldNames::Pos)
          .setInputFromPort(domainCoordinateSystemPort);

        // Create min vector node
        auto minVecNode = target.create<nodes::ConstantVector>();
        minVecNode->parameter().at(FieldNames::X) = VariantParameter(boundingBox.min.x);
        minVecNode->parameter().at(FieldNames::Y) = VariantParameter(boundingBox.min.y);
        minVecNode->parameter().at(FieldNames::Z) = VariantParameter(boundingBox.min.z);

        // Create max vector node
        auto maxVecNode = target.create<nodes::ConstantVector>();
        maxVecNode->parameter().at(FieldNames::X) = VariantParameter(boundingBox.max.x);
        maxVecNode->parameter().at(FieldNames::Y) = VariantParameter(boundingBox.max.y);
        maxVecNode->parameter().at(FieldNames::Z) = VariantParameter(boundingBox.max.z);

        // Connect min/max vectors to bounding box
        boundingBoxNode->parameter()
          .at(FieldNames::Min)
          .setInputFromPort(minVecNode->getOutputs().at(FieldNames::Vector));
        boundingBoxNode->parameter()
          .at(FieldNames::Max)
          .setInputFromPort(maxVecNode->getOutputs().at(FieldNames::Vector));

        // Create the function call for this level set
        nodes::Resource resourceNodeType;
        auto resourceNode = target.create(resourceNodeType);
        resourceNode->parameter().at(FieldNames::ResourceId) =
          VariantParameter(referencedModel.getResourceId());

        nodes::FunctionCall partType;
        auto functionCallNode = target.create(partType);

        functionCallNode->parameter()
          .at(FieldNames::FunctionId)
          .setInputFromPort(resourceNode->getOutputs().at(FieldNames::Value));

        functionCallNode->updateInputsAndOutputs(referencedModel);
        target.registerInputs(*functionCallNode);
        target.registerOutputs(*functionCallNode);

        // Set the position input for the function
        auto iterPosInput = functionCallNode->parameter().find(FieldNames::Pos);
        if (iterPosInput == std::end(functionCallNode->parameter()))
        {
            throw std::runtime_error("Entry function has no pos input");
        }
        iterPosInput->second.setInputFromPort(functionCoordinateSystemPort);

        // Get the function output
        auto iterShapeOutput = functionCallNode->getOutputs().find(sdfChannelName);
        if (iterShapeOutput == std::end(functionCallNode->getOutputs()))
        {
            throw std::runtime_error(
              fmt::format("Entry function has no output with the name {}", sdfChannelName));
        }
        if (iterShapeOutput->second.getTypeIndex() != nodes::ParameterTypeIndex::Float)
        {
            throw std::runtime_error(fmt::format("The output {} is not a scalar", sdfChannelName));
        }
        auto & functionShapePort = iterShapeOutput->second;

        // Intersect the function with its bounding box (Max operation for SDF intersection)
        nodes::Max intersectionType;
        auto intersectionNode = target.create(intersectionType);
        intersectionNode->parameter()
          .at(FieldNames::A)
          .setInputFromPort(boundingBoxNode->getOutputs().at(FieldNames::Shape));
        intersectionNode->parameter().at(FieldNames::B).setInputFromPort(functionShapePort);

        // Now union this result with any existing shapes (Min operation for SDF union)
        auto lastShapePort = getLastShape(target);
        auto shapeSink = target.getEndNode()->getParameter(FieldNames::Shape);
        if (!shapeSink)
        {
            throw std::runtime_error("End node is required to have a shape parameter");
        }

        if (!lastShapePort)
        {
            // First level set - just set the intersection result directly
            shapeSink->setInputFromPort(intersectionNode->getOutputs().at(FieldNames::Result));
        }
        else
        {
            // Union this intersection with existing shapes using Min operation
            nodes::Min unionType;
            auto unionNode = target.create(unionType);
            unionNode->parameter().at(FieldNames::A).setInputFromPort(*lastShapePort);
            unionNode->parameter()
              .at(FieldNames::B)
              .setInputFromPort(intersectionNode->getOutputs().at(FieldNames::Result));
            shapeSink->setInputFromPort(unionNode->getOutputs().at(FieldNames::Result));
        }
    }

    void Builder::appendFunctionForColorOutput(Model & target,
                                               Model & referencedModel,
                                               nodes::Port & coordinateSystemPort,
                                               Matrix4x4 const & transformation)
    {

        auto resourceNode = target.create<nodes::Resource>();
        resourceNode->parameter().at(FieldNames::ResourceId) =
          VariantParameter(referencedModel.getResourceId());

        auto functionCallNode = target.create<nodes::FunctionCall>();

        functionCallNode->parameter()
          .at(FieldNames::FunctionId)
          .setInputFromPort(resourceNode->getOutputs().at(FieldNames::Value));

        functionCallNode->updateInputsAndOutputs(referencedModel);
        target.registerInputs(*functionCallNode);
        target.registerOutputs(*functionCallNode);

        // check if partNode has a pos parameter
        auto iterPosInput = functionCallNode->parameter().find(FieldNames::Pos);
        if (iterPosInput == std::end(functionCallNode->parameter()))
        {
            throw std::runtime_error("Color function has no pos input");
        }

        auto uvwCoordinatesPort =
          insertTransformation(target, coordinateSystemPort, transformation);

        iterPosInput->second.setInputFromPort(uvwCoordinatesPort);

        // check, if part has a color
        auto iterColorOutput = functionCallNode->getOutputs().find(FieldNames::Color);
        if (iterColorOutput == std::end(functionCallNode->getOutputs()))
        {
            throw std::runtime_error("Color function has no color output");
        }

        auto & resourceColorPort = iterColorOutput->second;

        // connect color output to color input of end node
        auto colorSink = target.getEndNode()->getParameter(FieldNames::Color);
        if (!colorSink)
        {
            throw std::runtime_error("End node is required to have a color parameter");
        }

        colorSink->setInputFromPort(resourceColorPort);
    }

    void Builder::createFunctionFromImage3D(Assembly & assembly,
                                            ResourceId functionModelId,
                                            ResourceId imageResourceId,
                                            SamplingSettings const & samplingSettings)
    {
        assembly.addModelIfNotExisting(functionModelId);
        auto function = assembly.findModel(functionModelId);
        if (!function)
        {
            throw std::runtime_error("Could not find function model");
        }

        function->setManaged(true);
        function->createBeginEndWithDefaultInAndOuts();
        function->getEndNode()->parameter()[FieldNames::Color] =
          VariantParameter(float3(1.f, 1.f, 1.f));
        function->getEndNode()->parameter()[FieldNames::Red] = VariantParameter(1.f);
        function->getEndNode()->parameter()[FieldNames::Green] = VariantParameter(1.f);
        function->getEndNode()->parameter()[FieldNames::Blue] = VariantParameter(1.f);
        function->getEndNode()->parameter()[FieldNames::Alpha] = VariantParameter(1.f);

        function->setDisplayName(fmt::format("functionFromImage3D_{}", functionModelId));

        auto imageSamplerNode = function->create<nodes::ImageSampler>();

        imageSamplerNode->parameter().at(FieldNames::Filter) =
          VariantParameter(static_cast<int>(samplingSettings.filter));
        imageSamplerNode->parameter().at(FieldNames::TileStyleU) =
          VariantParameter(static_cast<int>(samplingSettings.tileStyleU));
        imageSamplerNode->parameter().at(FieldNames::TileStyleV) =
          VariantParameter(static_cast<int>(samplingSettings.tileStyleV));
        imageSamplerNode->parameter().at(FieldNames::TileStyleW) =
          VariantParameter(static_cast<int>(samplingSettings.tileStyleW));

        auto resourceNode = function->create<nodes::Resource>();
        resourceNode->parameter().at(FieldNames::ResourceId) = VariantParameter(imageResourceId);

        imageSamplerNode->parameter()
          .at(FieldNames::ResourceId)
          .setInputFromPort(resourceNode->getOutputs().at(FieldNames::Value));
        imageSamplerNode->parameter()
          .at(FieldNames::UVW)
          .setInputFromPort(function->getBeginNode()->getOutputs().at(FieldNames::Pos));

        function->registerInputs(*imageSamplerNode);

        // Create constant node for scale
        auto scaleNode = function->create<nodes::ConstantScalar>();
        scaleNode->parameter().at(FieldNames::Value) = VariantParameter(samplingSettings.scale);
        scaleNode->setDisplayName("scale");

        auto scaleAsVectorNode = function->create<nodes::VectorFromScalar>();
        scaleAsVectorNode->parameter()
          .at(FieldNames::A)
          .setInputFromPort(scaleNode->getOutputs().at(FieldNames::Value));

        // Create constant node for offset
        auto offsetNode = function->create<nodes::ConstantScalar>();
        offsetNode->parameter().at(FieldNames::Value) = VariantParameter(samplingSettings.offset);
        offsetNode->setDisplayName("offset");

        auto offsetAsVectorNode = function->create<nodes::VectorFromScalar>();
        offsetAsVectorNode->parameter()
          .at(FieldNames::A)
          .setInputFromPort(offsetNode->getOutputs().at(FieldNames::Value));

        // Create multiplication node for the color
        auto multiplyNode = function->create<nodes::Multiplication>();
        multiplyNode->parameter()
          .at(FieldNames::A)
          .setInputFromPort(imageSamplerNode->getOutputs().at(FieldNames::Color));
        multiplyNode->parameter()
          .at(FieldNames::B)
          .setInputFromPort(scaleAsVectorNode->getOutputs().at(FieldNames::Result));

        // Create addition node for the color
        auto additionNode = function->create<nodes::Addition>();
        additionNode->parameter()
          .at(FieldNames::A)
          .setInputFromPort(multiplyNode->getOutputs().at(FieldNames::Result));
        additionNode->parameter()
          .at(FieldNames::B)
          .setInputFromPort(offsetAsVectorNode->getOutputs().at(FieldNames::Result));

        // Multiply node for alpha
        auto alphaMultiplyNode = function->create<nodes::Multiplication>();
        alphaMultiplyNode->parameter()
          .at(FieldNames::A)
          .setInputFromPort(imageSamplerNode->getOutputs().at(FieldNames::Alpha));
        alphaMultiplyNode->parameter()
          .at(FieldNames::B)
          .setInputFromPort(scaleNode->getOutputs().at(FieldNames::Value));

        // Addition node for alpha
        auto alphaAdditionNode = function->create<nodes::Addition>();
        alphaAdditionNode->parameter()
          .at(FieldNames::A)
          .setInputFromPort(alphaMultiplyNode->getOutputs().at(FieldNames::Result));
        alphaAdditionNode->parameter()
          .at(FieldNames::B)
          .setInputFromPort(offsetNode->getOutputs().at(FieldNames::Value));

        // Decompose color to provide separate outputs for red, green and blue
        auto decomposeColorNode = function->create<nodes::DecomposeVector>();
        decomposeColorNode->parameter()
          .at(FieldNames::A)
          .setInputFromPort(additionNode->getOutputs().at(FieldNames::Result));

        function->getEndNode()
          ->getParameter(FieldNames::Color)
          ->setInputFromPort(additionNode->getOutputs().at(FieldNames::Result));

        function->getEndNode()
          ->getParameter(FieldNames::Red)
          ->setInputFromPort(decomposeColorNode->getOutputs().at(FieldNames::X));
        function->getEndNode()
          ->getParameter(FieldNames::Green)
          ->setInputFromPort(decomposeColorNode->getOutputs().at(FieldNames::Y));
        function->getEndNode()
          ->getParameter(FieldNames::Blue)
          ->setInputFromPort(decomposeColorNode->getOutputs().at(FieldNames::Z));

        function->getEndNode()
          ->getParameter(FieldNames::Alpha)
          ->setInputFromPort(alphaAdditionNode->getOutputs().at(FieldNames::Result));

        assembly.updateInputsAndOutputs();
    }

    nodes::Port & Builder::addTransformationToInputCs(Model & target,
                                                      Matrix4x4 const & transformation)
    {
        auto const transformationNode = target.create<nodes::Transformation>();

        transformationNode->parameter().at(nodes::FieldNames::Transformation) =
          VariantParameter(transformation, ContentType::Transformation);

        transformationNode->parameter()
          .at(nodes::FieldNames::Pos)
          .setInputFromPort(target.getBeginNode()->getOutputs().at(FieldNames::Pos));

        return transformationNode->getOutputs().at(FieldNames::Pos);
    }

    nodes::Port & Builder::insertTransformation(Model & target,
                                                nodes::Port & inputPort,
                                                Matrix4x4 const & transformation)
    {
        auto const transformationNode = target.create<nodes::Transformation>();

        transformationNode->parameter().at(nodes::FieldNames::Transformation) =
          VariantParameter(transformation, ContentType::Transformation);

        transformationNode->parameter().at(nodes::FieldNames::Pos).setInputFromPort(inputPort);

        return transformationNode->getOutputs().at(FieldNames::Pos);
    }

    Port * Builder::getLastShape(Model & target)
    {
        auto * const shapeSink = target.getEndNode()->getParameter(FieldNames::Shape);
        if (!shapeSink)
        {
            return nullptr;
        }

        auto const & shapeSource = shapeSink->getSource();
        if (!shapeSource.has_value())
        {
            return nullptr;
        }
        auto const portIter = target.getPortRegistry().find(shapeSource.value().portId);
        if (portIter == target.getPortRegistry().end())
        {
            return nullptr;
        }
        return portIter->second;
    }

    void
    Builder::addCompositeModel(Document & doc, ResourceId modelId, Components const & componentIds)
    {
        if (doc.getAssembly()->findModel(modelId))
        {
            return;
        }

        doc.getAssembly()->addModelIfNotExisting(modelId);
        auto & model = doc.getAssembly()->getFunctions().at(modelId);
        model->createBeginEnd();
        model->setManaged(true);

        for (auto & component : componentIds)
        {
            if (getComponentType(doc, component.id) == ComponentType::GeometryResource)
            {
                auto posPort = addTransformationToInputCs(*model, component.transform);
                addResourceRef(*model, ResourceKey{component.id}, posPort);
            }
            else if (getComponentType(doc, component.id) == ComponentType::SubModel)
            {
                auto & referencedModel = doc.getAssembly()->getFunctions().at(component.id);
                addComponentRef(*model, *referencedModel, component.transform);
            }
        }
    }

    ComponentType Builder::getComponentType(Document & doc, ResourceId modelId)
    {
        if (doc.getGeneratorContext().resourceManager.hasResource(ResourceKey{modelId}))
        {
            return ComponentType::GeometryResource;
        }

        return ComponentType::SubModel;
    }
}
