#include "Builder.h"

#include "Document.h"

namespace gladius::nodes
{
    // Find a node by display name (simple linear search)
    static NodeBase * findNodeByDisplayName(Model & target, const std::string & displayName)
    {
        for (auto & it : target)
        {
            auto * node = it.second.get();
            if (node && node->getDisplayName() == displayName)
            {
                return node;
            }
        }
        return nullptr;
    }

    nodes::Port &
    Builder::ensureConstantScalar(Model & target, const std::string & displayName, float value)
    {
        if (auto * existing = findNodeByDisplayName(target, displayName))
        {
            // Reuse existing node's value output port
            auto * valPort = existing->findOutputPort(FieldNames::Value);
            if (valPort)
            {
                return *valPort;
            }
        }

        auto node = target.create<nodes::ConstantScalar>();
        node->parameter().at(FieldNames::Value) = VariantParameter(value);
        node->setDisplayName(displayName);
        return node->getValueOutputPort();
    }

    void Builder::applyDistanceNormalization(Model & target, float units_per_mm)
    {
        if (units_per_mm == 1.0f)
        {
            return; // No-op
        }

        float const mm_per_unit = (units_per_mm != 0.0f) ? (1.0f / units_per_mm) : 1.0f;

        // Ensure a shared mm_per_unit constant exists
        auto & mmPerUnitPort = ensureConstantScalar(target, "mm_per_unit", mm_per_unit);

        // If outputs.shape has a source, insert (shape * mm_per_unit) => outputs.shape
        auto shapeSink = target.getEndNode()->getParameter(FieldNames::Shape);
        if (!shapeSink || !shapeSink->getSource().has_value())
        {
            return;
        }

        auto * srcPort = target.getPort(shapeSink->getSource()->portId);
        if (!srcPort)
        {
            return;
        }

        // Reuse existing ScaleDistance multiply if already present
        if (auto * existing = findNodeByDisplayName(target, "ScaleDistance"))
        {
            auto * out = existing->findOutputPort(FieldNames::Result);
            if (out)
            {
                shapeSink->setInputFromPort(*out);
                return;
            }
        }

        auto mul = target.create<nodes::Multiplication>();
        mul->setDisplayName("ScaleDistance");
        mul->setInputA(*srcPort);
        mul->setInputB(mmPerUnitPort);
        shapeSink->setInputFromPort(mul->getResultOutputPort());
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
        beamLatticeInput.setInputFromPort(resourceNode->getOutputValue());

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

        uniteNode->setInputA(*lastShapePort);
        uniteNode->setInputB(resourceShapePort);

        shapeSink->setInputFromPort(uniteNode->getResultOutputPort());
    }

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
        meshInput.setInputFromPort(resourceNode->getOutputValue());

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

        uniteNode->setInputA(*lastShapePort);
        uniteNode->setInputB(resourceShapePort);

        shapeSink->setInputFromPort(uniteNode->getResultOutputPort());
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
          .setInputFromPort(minVecNode->getVectorOutputPort());
        boxNode->parameter()
          .at(FieldNames::Max)
          .setInputFromPort(maxVecNode->getVectorOutputPort());

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

        uniteNode->setInputA(*lastShapePort);
        uniteNode->setInputB(boxNode->getOutputs().at(FieldNames::Shape));

        shapeSink->setInputFromPort(uniteNode->getResultOutputPort());
    }

    void Builder::addComponentRef(Model & target,
                                  Model & referencedModel,
                                  Matrix4x4 const & transformation,
                                  float unitScaleToModel)
    {
        auto coordinateSystemPort =
          addTransformationToInputCs(target, transformation, unitScaleToModel);

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
          .setInputFromPort(resourceNode->getOutputValue());

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

        uniteNode->setInputA(*lastShapePort);
        uniteNode->setInputB(resourceShapePort);

        shapeSink->setInputFromPort(uniteNode->getResultOutputPort());
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
          .setInputFromPort(resourceNode->getOutputValue());

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

        unionNode->setInputA(*lastShapePort);
        unionNode->setInputB(resourceShapePort);

        shapeSink->setInputFromPort(unionNode->getResultOutputPort());
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
          .setInputFromPort(resourceNode->getOutputValue());

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

        intersectionNode->setInputA(*lastShapePort);
        intersectionNode->setInputB(functionShapePort);

        shapeSink->setInputFromPort(intersectionNode->getResultOutputPort());
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
          .setInputFromPort(minVecNode->getVectorOutputPort());
        boundingBoxNode->parameter()
          .at(FieldNames::Max)
          .setInputFromPort(maxVecNode->getVectorOutputPort());

        // Create the function call for this level set
        nodes::Resource resourceNodeType;
        auto resourceNode = target.create(resourceNodeType);
        resourceNode->parameter().at(FieldNames::ResourceId) =
          VariantParameter(referencedModel.getResourceId());

        nodes::FunctionCall partType;
        auto functionCallNode = target.create(partType);

        functionCallNode->parameter()
          .at(FieldNames::FunctionId)
          .setInputFromPort(resourceNode->getOutputValue());

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
        intersectionNode->setInputA(boundingBoxNode->getOutputs().at(FieldNames::Shape));
        intersectionNode->setInputB(functionShapePort);

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
            shapeSink->setInputFromPort(intersectionNode->getResultOutputPort());
        }
        else
        {
            // Union this intersection with existing shapes using Min operation
            nodes::Min unionType;
            auto unionNode = target.create(unionType);
            unionNode->setInputA(*lastShapePort);
            unionNode->setInputB(intersectionNode->getResultOutputPort());
            shapeSink->setInputFromPort(unionNode->getResultOutputPort());
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
          .setInputFromPort(resourceNode->getOutputValue());
        imageSamplerNode->parameter()
          .at(FieldNames::UVW)
          .setInputFromPort(function->getBeginNode()->getOutputs().at(FieldNames::Pos));

        function->registerInputs(*imageSamplerNode);

        // Create constant node for scale
        auto scaleNode = function->create<nodes::ConstantScalar>();
        scaleNode->parameter().at(FieldNames::Value) = VariantParameter(samplingSettings.scale);
        scaleNode->setDisplayName("scale");

        auto scaleAsVectorNode = function->create<nodes::VectorFromScalar>();

        scaleAsVectorNode->setInputA(scaleNode->getValueOutputPort());

        // Create constant node for offset
        auto offsetNode = function->create<nodes::ConstantScalar>();
        offsetNode->parameter().at(FieldNames::Value) = VariantParameter(samplingSettings.offset);
        offsetNode->setDisplayName("offset");

        auto offsetAsVectorNode = function->create<nodes::VectorFromScalar>();
        offsetAsVectorNode->setInputA(offsetNode->getValueOutputPort());

        // Create multiplication node for the color
        auto multiplyNode = function->create<nodes::Multiplication>();
        multiplyNode->setInputA(imageSamplerNode->getOutputs().at(FieldNames::Color));
        multiplyNode->setInputB(scaleAsVectorNode->getResultOutputPort());

        // Create addition node for the color
        auto additionNode = function->create<nodes::Addition>();
        additionNode->setInputA(multiplyNode->getResultOutputPort());
        additionNode->setInputB(offsetAsVectorNode->getResultOutputPort());

        // Multiply node for alpha
        auto alphaMultiplyNode = function->create<nodes::Multiplication>();
        alphaMultiplyNode->setInputA(imageSamplerNode->getOutputs().at(FieldNames::Alpha));
        alphaMultiplyNode->setInputB(scaleNode->getValueOutputPort());

        // Addition node for alpha
        auto alphaAdditionNode = function->create<nodes::Addition>();
        alphaAdditionNode->setInputA(alphaMultiplyNode->getResultOutputPort());
        alphaAdditionNode->setInputB(offsetNode->getValueOutputPort());

        // Decompose color to provide separate outputs for red, green and blue
        auto decomposeColorNode = function->create<nodes::DecomposeVector>();
        decomposeColorNode->parameter()
          .at(FieldNames::A)
          .setInputFromPort(additionNode->getResultOutputPort());

        function->getEndNode()
          ->getParameter(FieldNames::Color)
          ->setInputFromPort(additionNode->getResultOutputPort());

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
          ->setInputFromPort(alphaAdditionNode->getResultOutputPort());

        assembly.updateInputsAndOutputs();
    }

    static nodes::Port &
    addScaleIfNeeded(Model & target, nodes::Port & inputPort, float unitScaleToModel)
    {
        // If scale is identity, return the input as-is
        if (unitScaleToModel == 1.0f)
        {
            return inputPort;
        }

        // Build the visible scaling chain with a single shared mm_per_unit constant
        // One
        auto & one = Builder::ensureConstantScalar(target, "One", 1.0f);
        // mm_per_unit (shared)
        const float mm_per_unit = (unitScaleToModel != 0.0f) ? (1.0f / unitScaleToModel) : 1.0f;
        auto & mmPerUnit = Builder::ensureConstantScalar(target, "mm_per_unit", mm_per_unit);

        // Division: units_per_mm = One / mm_per_unit
        NodeBase * existingDivision = findNodeByDisplayName(target, "Division");
        nodes::Port * unitsPerMmPort = nullptr;
        if (existingDivision)
        {
            unitsPerMmPort = existingDivision->findOutputPort(FieldNames::Result);
        }
        if (!unitsPerMmPort)
        {
            auto division = target.create<nodes::Division>();
            division->setDisplayName("Division");
            division->setInputA(one);
            division->setInputB(mmPerUnit);
            unitsPerMmPort = &division->getResultOutputPort();
        }

        // VectorFromScalar: make a (s,s,s) vector
        NodeBase * existingVec = findNodeByDisplayName(target, "VectorFromScalar");
        nodes::Port * vecPort = nullptr;
        if (existingVec)
        {
            vecPort = existingVec->findOutputPort(FieldNames::Result);
        }
        if (!vecPort)
        {
            auto toVec = target.create<nodes::VectorFromScalar>();
            toVec->setDisplayName("VectorFromScalar");
            toVec->setInputA(*unitsPerMmPort);
            vecPort = &toVec->getResultOutputPort();
        }

        // UnitScaling multiply: pos * (s,s,s), reuse if present
        if (auto * existingMul = findNodeByDisplayName(target, "UnitScaling"))
        {
            // Can't use typed setters on generic NodeBase pointer, keep map access here
            existingMul->parameter().at(FieldNames::A).setInputFromPort(inputPort);
            existingMul->parameter().at(FieldNames::B).setInputFromPort(*vecPort);
            return existingMul->getOutputs().at(FieldNames::Result);
        }

        auto mul = target.create<nodes::Multiplication>();
        mul->setDisplayName("UnitScaling");
        mul->setInputA(inputPort);
        mul->setInputB(*vecPort);
        return mul->getResultOutputPort();
    }

    nodes::Port & Builder::addTransformationToInputCs(Model & target,
                                                      Matrix4x4 const & transformation,
                                                      float unitScaleToModel)
    {
        auto const transformationNode = target.create<nodes::Transformation>();

        transformationNode->parameter().at(nodes::FieldNames::Transformation) =
          VariantParameter(transformation, ContentType::Transformation);

        // Start with begin position in mm, scale to model units if required
        auto & beginPos = target.getBeginNode()->getOutputs().at(FieldNames::Pos);
        auto & scaledPos = addScaleIfNeeded(target, beginPos, unitScaleToModel);
        transformationNode->parameter().at(nodes::FieldNames::Pos).setInputFromPort(scaledPos);

        return transformationNode->getOutputs().at(FieldNames::Pos);
    }

    nodes::Port & Builder::insertTransformation(Model & target,
                                                nodes::Port & inputPort,
                                                Matrix4x4 const & transformation,
                                                float unitScaleToModel)
    {
        auto const transformationNode = target.create<nodes::Transformation>();

        transformationNode->parameter().at(nodes::FieldNames::Transformation) =
          VariantParameter(transformation, ContentType::Transformation);

        auto & scaledPort = addScaleIfNeeded(target, inputPort, unitScaleToModel);
        transformationNode->parameter().at(nodes::FieldNames::Pos).setInputFromPort(scaledPort);

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

    void Builder::addCompositeModel(Document & doc,
                                    ResourceId modelId,
                                    Components const & componentIds,
                                    float unitScaleToModel)
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
                auto posPort =
                  addTransformationToInputCs(*model, component.transform, unitScaleToModel);
                addResourceRef(*model, ResourceKey{component.id}, posPort);
            }
            else if (getComponentType(doc, component.id) == ComponentType::SubModel)
            {
                auto & referencedModel = doc.getAssembly()->getFunctions().at(component.id);
                addComponentRef(*model, *referencedModel, component.transform, unitScaleToModel);
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

    void Builder::addBeamLatticeWithClipping(Model & target,
                                             ResourceKey const & beamLatticeKey,
                                             ResourceKey const & clippingMeshKey,
                                             int clippingMode,
                                             nodes::Port & coordinateSystemPort)
    {
        // Create beam lattice resource node
        nodes::Resource beamLatticeResourceNode;
        auto beamLatticeResource = target.create(beamLatticeResourceNode);
        beamLatticeResource->parameter().at(FieldNames::ResourceId) =
          VariantParameter(beamLatticeKey.getResourceId().value_or(0));

        // Create beam lattice SDF node
        nodes::SignedDistanceToBeamLattice beamLatticeNode;
        auto beamLattice = target.create(beamLatticeNode);
        beamLattice->parameter().at(FieldNames::Pos).setInputFromPort(coordinateSystemPort);
        beamLattice->parameter()
          .at(FieldNames::BeamLattice)
          .setInputFromPort(beamLatticeResource->getOutputValue());

        // Create clipping mesh resource node
        nodes::Resource clippingMeshResourceNode;
        auto clippingMeshResource = target.create(clippingMeshResourceNode);
        clippingMeshResource->parameter().at(FieldNames::ResourceId) =
          VariantParameter(clippingMeshKey.getResourceId().value_or(0));

        // Create clipping mesh SDF node
        nodes::SignedDistanceToMesh clippingMeshNode;
        auto clippingMesh = target.create(clippingMeshNode);
        clippingMesh->parameter().at(FieldNames::Pos).setInputFromPort(coordinateSystemPort);
        clippingMesh->parameter()
          .at(FieldNames::Mesh)
          .setInputFromPort(clippingMeshResource->getOutputValue());

        // Get the beam lattice and clipping mesh output ports
        auto & beamLatticeShapePort = beamLattice->getOutputs().at(FieldNames::Distance);
        auto & clippingMeshShapePort = clippingMesh->getOutputs().at(FieldNames::Distance);

        // Create clipping operation based on mode
        nodes::Port * clippedShapePort = nullptr;

        if (clippingMode == 1) // Inside clipping
        {
            // Max(beam_lattice, clipping_mesh) - keeps beam lattice inside clipping mesh
            auto intersectionNode = target.create<nodes::Max>();
            intersectionNode->setInputA(beamLatticeShapePort);
            intersectionNode->setInputB(clippingMeshShapePort);
            clippedShapePort = &intersectionNode->getResultOutputPort();
        }
        else if (clippingMode == 2) // Outside clipping
        {
            // Max(beam_lattice, -clipping_mesh) - keeps beam lattice outside clipping mesh
            // First negate the clipping mesh SDF
            auto negateNode = target.create<nodes::Multiplication>();
            auto minusOne = target.create<nodes::ConstantScalar>();
            minusOne->parameter().at(FieldNames::Value) = VariantParameter(-1.0f);

            negateNode->setInputA(clippingMeshShapePort);
            negateNode->parameter()
              .at(FieldNames::B)
              .setInputFromPort(minusOne->getValueOutputPort());

            // Then intersect with negated clipping mesh
            auto intersectionNode = target.create<nodes::Max>();
            intersectionNode->setInputA(beamLatticeShapePort);
            intersectionNode->setInputB(negateNode->getResultOutputPort());
            clippedShapePort = &intersectionNode->getResultOutputPort();
        }
        else
        {
            // No clipping or unknown mode - just use beam lattice
            clippedShapePort = &beamLatticeShapePort;
        }

        // Connect to the end node
        auto lastShapePort = getLastShape(target);
        auto shapeSink = target.getEndNode()->getParameter(FieldNames::Shape);

        if (!shapeSink)
        {
            throw std::runtime_error("End node is required to have a shape parameter");
        }

        if (!lastShapePort)
        {
            shapeSink->setInputFromPort(*clippedShapePort);
            return;
        }

        // Union with existing shapes
        auto uniteNode = target.create<nodes::Min>();
        uniteNode->setInputA(*lastShapePort);
        uniteNode->setInputB(*clippedShapePort);
        shapeSink->setInputFromPort(uniteNode->getResultOutputPort());
    }
}
