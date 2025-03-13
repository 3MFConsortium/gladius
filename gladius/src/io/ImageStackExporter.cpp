#include "ImageStackExporter.h"

#include "MeshExporter.h"
#include "compute/ComputeCore.h"

#include <lodepng.h>

#include <filesystem>

namespace gladius::io
{

    Lib3MF::PMeshObject addBoundingBoxAsMesh(Lib3MF::PModel model, BoundingBox const & bb)
    {
        auto mesh = model->AddMeshObject();

        auto v0 = mesh->AddVertex({bb.min.x, bb.min.y, bb.min.z});
        auto v1 = mesh->AddVertex({bb.max.x, bb.min.y, bb.min.z});
        auto v2 = mesh->AddVertex({bb.max.x, bb.max.y, bb.min.z});
        auto v3 = mesh->AddVertex({bb.min.x, bb.max.y, bb.min.z});
        auto v4 = mesh->AddVertex({bb.min.x, bb.min.y, bb.max.z});
        auto v5 = mesh->AddVertex({bb.max.x, bb.min.y, bb.max.z});
        auto v6 = mesh->AddVertex({bb.max.x, bb.max.y, bb.max.z});
        auto v7 = mesh->AddVertex({bb.min.x, bb.max.y, bb.max.z});

        // Bottom face
        mesh->AddTriangle({v0, v2, v1});
        mesh->AddTriangle({v0, v3, v2});

        // Top face
        mesh->AddTriangle({v4, v5, v6});
        mesh->AddTriangle({v4, v6, v7});

        // Front face
        mesh->AddTriangle({v0, v5, v4});
        mesh->AddTriangle({v0, v1, v5});

        // Back face
        mesh->AddTriangle({v3, v6, v2});
        mesh->AddTriangle({v3, v7, v6});

        // Left face
        mesh->AddTriangle({v0, v7, v3});
        mesh->AddTriangle({v0, v4, v7});

        // Right face
        mesh->AddTriangle({v1, v6, v5});
        mesh->AddTriangle({v1, v2, v6});

        mesh->SetName("Bounding Box");

        return mesh;
    }

    Lib3MF::sTransform createtIdentityTransform()
    {
        Lib3MF::sTransform transform;
        transform.m_Fields[0][0] = 1.0f;
        transform.m_Fields[0][1] = 0.0f;
        transform.m_Fields[0][2] = 0.0f;

        transform.m_Fields[1][0] = 0.0f;
        transform.m_Fields[1][1] = 1.0f;
        transform.m_Fields[1][2] = 0.0f;

        transform.m_Fields[2][0] = 0.0f;
        transform.m_Fields[2][1] = 0.0f;
        transform.m_Fields[2][2] = 1.0f;

        transform.m_Fields[3][0] = 0.0f;
        transform.m_Fields[3][1] = 0.0f;
        transform.m_Fields[3][2] = 0.0f;

        return transform;
    }

    Lib3MF::sTransform createTransformToBoundingBoxNormaliced(BoundingBox const & bb)
    {
        Lib3MF::sTransform transform;

        float4 const size = {bb.max.x - bb.min.x, bb.max.y - bb.min.y, bb.max.z - bb.min.z, 0.0f};
        float4 const offset = {bb.min.x, bb.min.y, bb.min.z, 0.0f};

        if (size.x == 0.f || size.y == 0.f || size.z == 0.f)
        {
            throw std::runtime_error("Bounding box has zero size");
        }

        transform.m_Fields[0][0] = 1.0f / size.x;
        transform.m_Fields[0][1] = 0.0f;
        transform.m_Fields[0][2] = 0.0f;

        transform.m_Fields[1][0] = 0.0f;
        transform.m_Fields[1][1] = -1.0f / size.y;
        transform.m_Fields[1][2] = 0.0f;

        transform.m_Fields[2][0] = 0.0f;
        transform.m_Fields[2][1] = 0.0f;
        transform.m_Fields[2][2] = 1.f / size.z;

        transform.m_Fields[3][0] = -offset.x / size.x;
        transform.m_Fields[3][1] = 1.0f + offset.y / size.y;
        transform.m_Fields[3][2] = -offset.z / size.z;

        return transform;
    }

    void saveDistanceMapToImage(const DistanceMap & distmap, const std::filesystem::path & filename)
    {
        const auto & data = distmap.getData();
        const auto width = distmap.getWidth();
        const auto height = distmap.getHeight();

        std::vector<unsigned char> image(width * height * 4);

        for (size_t y = 0; y < height; y++)
        {
            for (size_t x = 0; x < width; x++)
            {
                auto const idx = 4 * (x + y * width);
                auto const value = data[x + y * width].x;
                auto const val = static_cast<unsigned char>(128.f + value);

                image[idx] = val;
                image[idx + 1] = (value < 0.f) ? val : 0;
                image[idx + 2] = (value >= 0.f) ? val : 0;
                image[idx + 3] = 255;
            }
        }

        auto const error = lodepng::encode(filename.string(), image, width, height, LCT_RGBA, 8);
        if (error)
        {
            throw std::runtime_error("Error while saving image: " + std::to_string(error));
        }
    }

    ImageStackExporter::ImageStackExporter(events::SharedLogger logger)
        : m_logger(std::move(logger))
    {
        try
        {
            m_wrapper = Lib3MF::CWrapper::loadLibrary();
        }
        catch (std::exception & e)
        {
            m_logger->addEvent({e.what(), events::Severity::Error});
            return;
        }
    }

    void ImageStackExporter::beginExport(const std::filesystem::path & fileName,
                                         ComputeCore & generator)
    {
        m_model3mf = m_wrapper->CreateModel();
        m_outputFilename = fileName;

        if (!generator.updateBBox())
        {
            throw std::runtime_error(
              "Computing bounding box failed. The model has probably not been compiled yet");
        }
        const auto bb = generator.getBoundingBox();

        if (!bb.has_value())
        {
            throw std::runtime_error("Bounding box is not set");
        }

        m_startHeight_mm = bb->min.z;
        m_endHeight_mm = bb->max.z;

        generator.setSliceHeight(bb->min.z - m_layerIncrement_mm);
        generator.updateClippingAreaToBoundingBox();
        generator.getResourceContext().requestDistanceMaps();

        m_sheetcount =
          static_cast<Lib3MF_uint32>(ceil(m_endHeight_mm - m_startHeight_mm) / m_layerIncrement_mm);
        auto & distmap = *(generator.getResourceContext().getDistanceMipMaps()[m_qualityLevel]);
        m_columnCountWorld = distmap.getWidth();
        m_rowCountWorld = distmap.getHeight();
        m_imageStack = m_model3mf->AddImageStack(m_columnCountWorld, m_rowCountWorld, m_sheetcount);

        m_progress = 0.;

        auto mesh = addBoundingBoxAsMesh(m_model3mf, *bb);

        auto funcFromImg3d = m_model3mf->AddFunctionFromImage3D(m_imageStack.get());
        funcFromImg3d->SetFilter(Lib3MF::eTextureFilter::Linear);
        funcFromImg3d->SetOffset(-0.5f);
        funcFromImg3d->SetScale(1.0f);
        funcFromImg3d->SetTileStyles(Lib3MF::eTextureTileStyle::Clamp,
                                     Lib3MF::eTextureTileStyle::Clamp,
                                     Lib3MF::eTextureTileStyle::Clamp);

        auto levelSet = m_model3mf->AddLevelSet();
        levelSet->SetFunction(funcFromImg3d.get());
        levelSet->SetMesh(mesh.get());

        auto transformToBB = createTransformToBoundingBoxNormaliced(*bb);

        levelSet->SetTransform(transformToBB);
        levelSet->SetMeshBBoxOnly(true);
        levelSet->SetChannelName("red");

        Lib3MF::sTransform identityTransform = createtIdentityTransform();
        m_model3mf->AddBuildItem(levelSet.get(), identityTransform);

        // update thumbnail
        try
        {
            auto image = generator.createThumbnailPng();
            auto thumbNail = m_model3mf->CreatePackageThumbnailAttachment();
            thumbNail->ReadFromBuffer(image.data);
        }
        catch (const std::exception & e)
        {
            m_logger->addEvent({e.what(), events::Severity::Error});
            return;
        }
    }

    using ImageData = std::vector<Lib3MF_uint8>;

    void flipXY(ImageData & data, size_t width, size_t height)
    {
        ImageData flippedData;
        flippedData.resize(data.size());

        size_t const numChannels = data.size() / (width * height);

        for (size_t y = 0; y < height; y++)
        {
            for (size_t x = 0; x < width; x++)
            {
                for (size_t c = 0; c < numChannels; c++)
                {
                    flippedData[c + (width - x - 1) * numChannels +
                                (height - y - 1) * width * numChannels] =
                      data[c + x * numChannels + (height - y - 1) * width * numChannels];
                }
            }
        }

        data = flippedData;
    }

    void swapXY(ImageData & data, size_t width, size_t height)
    {
        ImageData swappedData;
        swappedData.resize(data.size());

        size_t const numChannels = data.size() / (width * height);

        for (size_t y = 0; y < height; y++)
        {
            for (size_t x = 0; x < width; x++)
            {
                for (size_t c = 0; c < numChannels; c++)
                {
                    swappedData[c + y * numChannels + x * height * numChannels] =
                      data[c + x * numChannels + y * width * numChannels];
                }
            }
        }

        data = swappedData;
    }

    void swapAndFlipXY(ImageData & data, size_t width, size_t height)
    {
        ImageData swappedData;
        swappedData.resize(data.size());

        size_t const numChannels = data.size() / (width * height);

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                unsigned int indexTarget = (y * width + width - x - 1) * numChannels;
                unsigned int indexSource = ((height - y - 1) * width + x) * numChannels;
                for (unsigned int i = 0; i < numChannels; ++i)
                {
                    swappedData[indexTarget + i] = data[indexSource + i];
                }
            }
        }

        data = std::move(swappedData);
    }

    bool ImageStackExporter::advanceExport(ComputeCore & generator)
    {

        generator.setSliceHeight(m_startHeight_mm + m_currentSlice * m_layerIncrement_mm);
        generator.updateClippingAreaToBoundingBox();
        generator.generateSdfSlice();

        auto & distmap = *(generator.getResourceContext().getDistanceMipMaps()[m_qualityLevel]);
        distmap.read();

        std::vector<Lib3MF_uint8> inputData;

        inputData.resize(distmap.getData().size());
        if (inputData.size() != m_columnCountWorld * m_rowCountWorld)
        {
            throw std::runtime_error("Size of input data does not match the size of the image");
        }

        for (size_t i = 0; i < distmap.getData().size(); i++)
        {
            auto const value = distmap.getData()[i].x;

            float const grayValue = std::clamp(128.f + value * 1000.0f, 0.f, 255.f);
            auto const val = static_cast<unsigned char>(grayValue);
            inputData[i] = val;
        }

        swapAndFlipXY(inputData, m_columnCountWorld, m_rowCountWorld);

        std::vector<Lib3MF_uint8> imgPng;

        auto const error =
          lodepng::encode(imgPng, inputData, getColumnCountPng(), getRowCountPng(), LCT_GREY, 8);
        if (error)
        {
            throw std::runtime_error("Error while saving image: " + std::to_string(error));
        }

        std::string path = fmt::format(
          "/volume/{}/layer_{:03}.png", m_imageStack->GetUniqueResourceID(), m_currentSlice);
        m_imageStack->CreateSheetFromBuffer(m_currentSlice, path, imgPng);
        m_progress = (m_currentSlice - 1) / (m_endHeight_mm - m_startHeight_mm);

        m_currentSlice++;
        return m_currentSlice < m_sheetcount;
    }

    double ImageStackExporter::getProgress() const
    {
        return m_progress;
    }

    void ImageStackExporter::finalize()
    {
        // write the 3mf file
        auto writer = m_model3mf->QueryWriter("3mf");
        writer->WriteToFile(m_outputFilename.string());
    }

    Lib3MF_uint32 ImageStackExporter::getColumnCountPng() const
    {
        // return m_rowCountWorld;
        return m_columnCountWorld;
    }

    Lib3MF_uint32 ImageStackExporter::getRowCountPng() const
    {
        // return m_columnCountWorld;
        return m_rowCountWorld;
    }
}