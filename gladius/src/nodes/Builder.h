#pragma once

#include "Components.h"
#include "DerivedNodes.h"
#include "Model.h"

namespace gladius
{
    class Document;
}

namespace gladius::nodes
{
    enum class ComponentType
    {
        SubModel,
        GeometryResource
    };

    struct SamplingSettings
    {
        SamplingFilter filter = SamplingFilter::SF_LINEAR;
        TextureTileStyle tileStyleU = TextureTileStyle::TTS_REPEAT;
        TextureTileStyle tileStyleV = TextureTileStyle::TTS_REPEAT;
        TextureTileStyle tileStyleW = TextureTileStyle::TTS_REPEAT;
        float offset = 0.;
        float scale = 1.;
    };

    class Builder
    {

      public:
        // Ensure a visible ConstantScalar with given display name exists and returns its Value port
        static nodes::Port &
        ensureConstantScalar(Model & target, const std::string & displayName, float value);

        // Apply distance normalization at the end node: shape *= mm_per_unit
        // units_per_mm -> mm_per_unit = 1 / units_per_mm
        static void applyDistanceNormalization(Model & target, float units_per_mm);

        void addResourceRef(Model & target,
                            ResourceKey const & resourceKey,
                            nodes::Port & coordinateSystemPort);

        void addBeamLatticeRef(Model & target,
                               ResourceKey const & resourceKey,
                               nodes::Port & coordinateSystemPort);

        void addBeamLatticeWithClipping(Model & target,
                                        ResourceKey const & beamLatticeKey,
                                        ResourceKey const & clippingMeshKey,
                                        int clippingMode, // 0=none, 1=inside, 2=outside
                                        nodes::Port & coordinateSystemPort);

        void addBoundingBox(Model & target,
                            BoundingBox const & boundingBox,
                            nodes::Port & coordinateSystemPort);

        // Creates a coordinate system port by transforming the input position by the provided
        // transformation. If unitScaleToModel != 1, a scaling (multiplication) node is inserted
        // before the transformation so that positions (assumed in mm) are converted into the
        // 3MF model's unit.
        // unitScaleToModel = units_per_mm = 1 / (mm_per_unit)
        nodes::Port & addTransformationToInputCs(Model & target,
                                                 Matrix4x4 const & transformation,
                                                 float unitScaleToModel = 1.0f);

        // Inserts a transformation node on top of the provided input port. If unitScaleToModel != 1
        // a scaling is applied before the matrix transformation.
        nodes::Port & insertTransformation(Model & target,
                                           nodes::Port & inputPort,
                                           Matrix4x4 const & transformation,
                                           float unitScaleToModel = 1.0f);

        nodes::Port * getLastShape(Model & target);

        void addCompositeModel(Document & doc,
                               ResourceId modelId,
                               Components const & componentIds,
                               float unitScaleToModel = 1.0f);

        void addComponentRef(Model & target,
                             Model & referencedModel,
                             Matrix4x4 const & transformation,
                             float unitScaleToModel = 1.0f);

        void appendIntersectionWithFunction(Model & target,
                                            Model & referencedModel,
                                            nodes::Port & coordinateSystemPort,
                                            std::string const & sdfChannelName);

        void intersectFunctionWithDomain(Model & target,
                                         Model & referencedModel,
                                         nodes::Port & coordinateSystemPort,
                                         std::string const & sdfChannelName);

        void addLevelSetWithDomain(Model & target,
                                   Model & referencedModel,
                                   nodes::Port & functionCoordinateSystemPort,
                                   std::string const & sdfChannelName,
                                   BoundingBox const & boundingBox,
                                   nodes::Port & domainCoordinateSystemPort);

        ComponentType getComponentType(Document & doc, ResourceId modelId);

        void createFunctionFromImage3D(Assembly & assenbly,
                                       ResourceId functionModelId,
                                       ResourceId imageResourceId,
                                       SamplingSettings const & samplingSettings);

        void appendFunctionForColorOutput(Model & target,
                                          Model & referencedModel,
                                          nodes::Port & coordinateSystemPort,
                                          Matrix4x4 const & transformation);
    };

}
