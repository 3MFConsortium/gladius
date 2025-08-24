#pragma once

#include "Components.h"
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
        void addResourceRef(Model & target,
                            ResourceKey const & resourceKey,
                            nodes::Port & coordinateSystemPort);

        void addBoundingBox(Model & target,
                            BoundingBox const & boundingBox,
                            nodes::Port & coordinateSystemPort);

        nodes::Port & addTransformationToInputCs(Model & target, Matrix4x4 const & transformation);

        nodes::Port & insertTransformation(Model & target,
                                           nodes::Port & inputPort,
                                           Matrix4x4 const & transformation);

        nodes::Port * getLastShape(Model & target);

        void addCompositeModel(Document & doc, ResourceId modelId, Components const & componentIds);

        void
        addComponentRef(Model & target, Model & referencedModel, Matrix4x4 const & transformation);

        void appendIntersectionWithFunction(Model & target,
                                            Model & referencedModel,
                                            nodes::Port & coordinateSystemPort,
                                            std::string const & sdfChannelName);

        void intersectFunctionWithDomain(Model & target,
                                         Model & referencedModel,
                                         nodes::Port & coordinateSystemPort,
                                         std::string const & sdfChannelName);

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
