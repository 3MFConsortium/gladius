#include "ThreemfThumbnailExtractor.h"
#include <algorithm>
#include <fmt/format.h>
#include <lodepng.h>

namespace gladius::ui
{
    ThreemfThumbnailExtractor::ThreemfThumbnailExtractor(events::SharedLogger logger)
        : m_logger(std::move(logger))
    {
        try
        {
            m_wrapper = Lib3MF::CWrapper::loadLibrary();
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->addEvent({e.what(), events::Severity::Error});
            }
        }
    }

    ThreemfThumbnailExtractor::~ThreemfThumbnailExtractor() = default;

    std::vector<unsigned char>
    ThreemfThumbnailExtractor::extractThumbnail(const std::filesystem::path & filePath)
    {
        std::vector<unsigned char> thumbnailData;

        if (!m_wrapper)
        {
            return thumbnailData;
        }

        try
        {
            auto model = m_wrapper->CreateModel();
            auto reader = model->QueryReader("3mf");

            reader->SetStrictModeActive(false);
            reader->ReadFromFile(filePath.string());

            if (model->HasPackageThumbnailAttachment())
            {
                auto thumbnail = model->GetPackageThumbnailAttachment();
                if (thumbnail)
                {
                    thumbnail->WriteToBuffer(thumbnailData);
                }
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->addEvent({fmt::format("Failed to extract thumbnail from {}: {}",
                                                filePath.string(),
                                                e.what()),
                                    events::Severity::Warning});
            }
        }

        return thumbnailData;
    }

    void ThreemfThumbnailExtractor::loadThumbnail(ThumbnailInfo & info)
    {
        if (info.thumbnailLoaded)
        {
            return;
        }

        info.thumbnailData = extractThumbnail(info.filePath);
        info.hasThumbnail = !info.thumbnailData.empty();
        info.thumbnailLoaded = true;


        if (info.hasThumbnail)
        {
            // Decode the PNG data to get width and height
            std::vector<unsigned char> image;
            unsigned int width, height;
            unsigned int error = lodepng::decode(image, width, height, info.thumbnailData);

            if (error == 0)
            {
                info.thumbnailWidth = width;
                info.thumbnailHeight = height;
                createThumbnailTexture(info);
            }
            else
            {
                info.hasThumbnail = false;
                if (m_logger)
                {
                    m_logger->addEvent({fmt::format("Failed to decode thumbnail for {}: {}",
                                                    info.fileName,
                                                    lodepng_error_text(error)),
                                        events::Severity::Warning});
                }
            }
        }
    }

    void ThreemfThumbnailExtractor::createThumbnailTexture(ThumbnailInfo & info)
    {
        if (info.thumbnailTextureId != 0 || !info.hasThumbnail || info.thumbnailData.empty())
        {
            return;
        }

        // Decode the PNG data
        std::vector<unsigned char> decodedImage;
        unsigned int width, height;
        unsigned int error = lodepng::decode(decodedImage, width, height, info.thumbnailData);

        if (error != 0)
        {
            return;
        }

        // Create OpenGL texture
        GLuint textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Upload the image data to the texture
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA,
                     width,
                     height,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     decodedImage.data());

        // Unbind the texture
        glBindTexture(GL_TEXTURE_2D, 0);

        // Store the texture ID
        info.thumbnailTextureId = textureId;
    }

    void ThreemfThumbnailExtractor::releaseThumbnail(ThumbnailInfo & info)
    {
        if (info.thumbnailTextureId != 0)
        {
            glDeleteTextures(1, &info.thumbnailTextureId);
            info.thumbnailTextureId = 0;
        }

        info.thumbnailData.clear();
        info.hasThumbnail = false;
        info.thumbnailLoaded = false;
    }

    ThreemfThumbnailExtractor::ThumbnailInfo
    ThreemfThumbnailExtractor::createThumbnailInfo(const std::filesystem::path & filePath,
                                                   std::time_t timestamp)
    {
        ThumbnailInfo info;
        info.filePath = filePath;
        info.fileName = filePath.stem().string();
        info.timestamp = timestamp;

        // Get file size
        try
        {
            if (std::filesystem::exists(filePath))
            {
                info.fileInfo.fileSize = std::filesystem::file_size(filePath);
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->addEvent(
                  {fmt::format("Failed to get file size for {}: {}", filePath.string(), e.what()),
                   events::Severity::Warning});
            }
        }
        

        // Extract 3MF metadata if possible
        try
        {
            if (m_wrapper && std::filesystem::exists(filePath))
            {
                auto model = m_wrapper->CreateModel();
                auto reader = model->QueryReader("3mf");

                reader->SetStrictModeActive(false);
                reader->ReadFromFile(filePath.string());

                // Get the metadata group from the model
                auto metaDataGroup = model->GetMetaDataGroup();
                if (metaDataGroup)
                {
                    // Get number of metadata entries
                    Lib3MF_uint32 entryCount = metaDataGroup->GetMetaDataCount();

                    for (Lib3MF_uint32 i = 0; i < entryCount; i++)
                    {
                        try
                        {
                            // Get metadata entry
                            auto metaData = metaDataGroup->GetMetaData(i);
                            if (metaData)
                            {
                                // Extract metadata key and value
                                std::string key = metaData->GetName();
                                std::string value = metaData->GetValue();

                                // Get namespace if present to make key more specific
                                std::string nameSpace = metaData->GetNameSpace();
                                if (!nameSpace.empty())
                                {
                                    key = nameSpace + ":" + key;
                                }

                                // Add to fileInfo
                                info.fileInfo.addMetadata(key, value);
                            }
                        }
                        catch (...)
                        {
                            // Skip this metadata entry if there's an error
                        }
                    }
                }
            }
        }
        catch (const std::exception & e)
        {
            // it is not a problem if we cannot read the metadata
        }

        return info;
    }
}
