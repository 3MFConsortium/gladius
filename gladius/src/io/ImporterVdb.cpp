#include "ImporterVdb.h"

#include "Builder.h"
#include "nodes/Assembly.h"
#include "nodes/Model.h"
#include "src/Document.h"
#include "src/Primitives.h"
#include "src/ResourceContext.h"
#include "src/io/VdbImporter.h"
#include "vdb.h"

#include <cstring>
#include <fmt/format.h>

#include "Document.h"
#include "Model.h"
#include "nodes/nodesfwd.h"
#include "src/Profiling.h"
#include "src/exceptions.h"
#include "src/types.h"
namespace gladius::io
{

    void loadFromOpenVdbFile(std::filesystem::path const filename, Document & doc)
    {
        ProfileFunction;

        io::ImporterVdb importer;
        importer.load(filename, doc);
    }

    void ImporterVdb::load(std::filesystem::path const & filename, Document & doc)
    {
        using namespace gladius::nodes;

        auto & resMan = doc.getGeneratorContext().resourceManager;

        vdb::VdbImporter vdbImporter;
        ResourceId const openVdbResourceId = 123;
        auto openVdbResourceKey = ResourceKey{openVdbResourceId};
        openvdb::GridBase::Ptr grid = vdbImporter.load(filename);
        
        auto voxelSize = grid->transform().voxelSize();
        auto dimensions = grid->evalActiveVoxelDim();
        
        resMan.addResource(openVdbResourceKey, std::move(grid));

        auto & res = resMan.getResource(openVdbResourceKey);

        res.setInUse(true);
        res.load();
        

        auto function = doc.getAssembly()->assemblyModel();

        auto resourceNode = function->create<nodes::Resource>();
        resourceNode->parameter().at(FieldNames::ResourceId) = VariantParameter(openVdbResourceId);

        auto imageSamplerNode = function->create<nodes::ImageSampler>();

        imageSamplerNode->parameter().at(FieldNames::Filter) =
          VariantParameter(static_cast<int>(SamplingFilter::SF_LINEAR));
        imageSamplerNode->parameter().at(FieldNames::TileStyleU) =
          VariantParameter(static_cast<int>(TextureTileStyle::TTS_CLAMP));
        imageSamplerNode->parameter().at(FieldNames::TileStyleV) =
          VariantParameter(static_cast<int>(TextureTileStyle::TTS_CLAMP));
        imageSamplerNode->parameter().at(FieldNames::TileStyleW) =
          VariantParameter(static_cast<int>(TextureTileStyle::TTS_CLAMP));


        float3 worldToIndexFactor = float3{1.0f / static_cast<float>(voxelSize.x()),
                                           1.0f / static_cast<float>(voxelSize.y()),
                                           1.0f / static_cast<float>(voxelSize.z())};
        float3 worldToUVWScale = float3(worldToIndexFactor.x / static_cast<float>(dimensions.x()),
                                        worldToIndexFactor.y / static_cast<float>(dimensions.y()),
                                        worldToIndexFactor.z / static_cast<float>(dimensions.z()));

        auto worldToUVWScaleNode = function->create<nodes::ConstantVector>();
        worldToUVWScaleNode->parameter().at(FieldNames::X) =
          VariantParameter(static_cast<float>(worldToUVWScale.x));
        worldToUVWScaleNode->parameter().at(FieldNames::Y) =
          VariantParameter(static_cast<float>(worldToUVWScale.y));
        worldToUVWScaleNode->parameter().at(FieldNames::Z) =
          VariantParameter(static_cast<float>(worldToUVWScale.z));

        worldToUVWScaleNode->setDisplayName("scaling");

        auto toUVWSpaceNode = function->create<nodes::Multiplication>();
        toUVWSpaceNode->parameter()
          .at(FieldNames::A)
          .setInputFromPort(function->getBeginNode()->getOutputs().at(FieldNames::Pos));
        toUVWSpaceNode->parameter()
          .at(FieldNames::B)
          .setInputFromPort(worldToUVWScaleNode->getOutputs().at(FieldNames::Vector));

        imageSamplerNode->parameter()
          .at(FieldNames::ResourceId)
          .setInputFromPort(resourceNode->getOutputs().at(FieldNames::Value));
        imageSamplerNode->parameter()
          .at(FieldNames::UVW)
          .setInputFromPort(toUVWSpaceNode->getOutputs().at(FieldNames::Result));

        auto decomposeColorNode = function->create<nodes::DecomposeVector>();
        decomposeColorNode->parameter()
          .at(FieldNames::A)
          .setInputFromPort(imageSamplerNode->getOutputs().at(FieldNames::Color));

        function->getEndNode()
          ->parameter()
          .at(FieldNames::Shape)
          .setInputFromPort(decomposeColorNode->getOutputs().at(FieldNames::X));

        doc.getAssembly()->updateInputsAndOutputs();
    }

}
