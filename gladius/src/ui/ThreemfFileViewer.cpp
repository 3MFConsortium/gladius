#include "ThreemfFileViewer.h"
#include "../IconFontCppHeaders/IconsFontAwesome5.h"
#include "FileChooser.h"
#include "Widgets.h"
#include "imgui.h"
#include "io/3mf/Importer3mf.h"
#include <algorithm>
#include <cstdint>
#include <fmt/format.h>
#include <lodepng.h>

namespace gladius::ui
{
    ThreemfFileViewer::ThreemfFileViewer(events::SharedLogger logger)
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

    ThreemfFileViewer::~ThreemfFileViewer()
    {
        // Clean up OpenGL textures
        for (auto & fileInfo : m_files)
        {
            if (fileInfo.thumbnailTextureId != 0)
            {
                glDeleteTextures(1, &fileInfo.thumbnailTextureId);
                fileInfo.thumbnailTextureId = 0;
            }
        }
    }

    void ThreemfFileViewer::setDirectory(const std::filesystem::path & directory)
    {
        if (m_directory != directory)
        {
            m_directory = directory;
            m_needsRefresh = true;
        }
    }

    // setVisibility removed - no longer needed as this is a pure widget

    void ThreemfFileViewer::refreshDirectory()
    {
        m_needsRefresh = true;
    }

    void ThreemfFileViewer::scanDirectory()
    {
        if (!m_needsRefresh)
        {
            return;
        }

        // Clean up existing textures
        for (auto & fileInfo : m_files)
        {
            if (fileInfo.thumbnailTextureId != 0)
            {
                glDeleteTextures(1, &fileInfo.thumbnailTextureId);
                fileInfo.thumbnailTextureId = 0;
            }
        }

        m_files.clear();

        if (!std::filesystem::exists(m_directory) || !std::filesystem::is_directory(m_directory))
        {
            return;
        }

        try
        {
            for (const auto & entry : std::filesystem::directory_iterator(m_directory))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".3mf")
                {
                    ThreemfFileInfo fileInfo;
                    fileInfo.filePath = entry.path();
                    fileInfo.fileName = entry.path().stem().string();
                    m_files.push_back(fileInfo);
                }
            }
        }
        catch (const std::filesystem::filesystem_error & e)
        {
            if (m_logger)
            {
                m_logger->addEvent({e.what(), events::Severity::Error});
            }
        }

        m_needsRefresh = false;
    }

    std::vector<unsigned char>
    ThreemfFileViewer::extractThumbnail(const std::filesystem::path & filePath)
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

    void ThreemfFileViewer::loadThumbnail(ThreemfFileInfo & fileInfo)
    {
        if (fileInfo.thumbnailLoaded)
        {
            return;
        }

        fileInfo.thumbnailData = extractThumbnail(fileInfo.filePath);
        fileInfo.hasThumbnail = !fileInfo.thumbnailData.empty();
        fileInfo.thumbnailLoaded = true;

        if (fileInfo.hasThumbnail)
        {
            // Decode the PNG data to get width and height
            std::vector<unsigned char> image;
            unsigned int width, height;
            unsigned int error = lodepng::decode(image, width, height, fileInfo.thumbnailData);

            if (error == 0)
            {
                fileInfo.thumbnailWidth = width;
                fileInfo.thumbnailHeight = height;
                createThumbnailTexture(fileInfo);
            }
            else
            {
                fileInfo.hasThumbnail = false;
                if (m_logger)
                {
                    m_logger->addEvent({fmt::format("Failed to decode thumbnail for {}: {}",
                                                    fileInfo.fileName,
                                                    lodepng_error_text(error)),
                                        events::Severity::Warning});
                }
            }
        }
    }

    void ThreemfFileViewer::createThumbnailTexture(ThreemfFileInfo & fileInfo)
    {
        if (fileInfo.thumbnailTextureId != 0 || !fileInfo.hasThumbnail ||
            fileInfo.thumbnailData.empty())
        {
            return;
        }

        // Decode the PNG data
        std::vector<unsigned char> decodedImage;
        unsigned int width, height;
        unsigned int error = lodepng::decode(decodedImage, width, height, fileInfo.thumbnailData);

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
        fileInfo.thumbnailTextureId = textureId;
    }

    void ThreemfFileViewer::render(SharedDocument doc)
    {
        // Scan the directory if needed
        scanDirectory();
            
        // Calculate the number of columns that fit into the window based on thumbnail size
        float const windowWidth = ImGui::GetContentRegionAvail().x;
        float const spacing = ImGui::GetStyle().ItemSpacing.x;
        float const effectiveItemWidth = m_thumbnailSize + spacing;

        // Calculate how many thumbnails fit in the window width
        m_columns = std::max(1, static_cast<int>(windowWidth / effectiveItemWidth));
            
        bool oneThumbnailLoaded = false;
        // Display files in a grid
        if (m_files.empty())
        {
            ImGui::TextUnformatted("No 3MF files found in the specified directory");
        }
        else
        {
            // No need for a child window as we're already in a tab or window
            float availableHeight = ImGui::GetContentRegionAvail().y;

                // Calculate item width based on available width and desired columns
                float windowWidth = ImGui::GetContentRegionAvail().x;
                float itemWidth =
                  (windowWidth - ImGui::GetStyle().ItemSpacing.x * (m_columns - 1)) / m_columns;

                // Define standard item size for consistent layout
                const float itemHeight = m_thumbnailSize + 40.0f; // Thumbnail + space for text

                // Loop through files and display them in a grid
                int fileIndex = 0;
                for (auto & fileInfo : m_files)
                {
                    // Start on a new line if not the first item and not at the start of a row
                    if (fileIndex > 0 && (fileIndex % m_columns) != 0)
                    {
                        ImGui::SameLine();
                    }

                    // Create a unique ID for the item
                    ImGui::PushID(fileIndex);

                    // Create a selectable group for the file
                    ImGui::BeginGroup();

                    // Process thumbnail if not already loaded
                    if (!fileInfo.thumbnailLoaded && !oneThumbnailLoaded)
                    {
                        oneThumbnailLoaded = true;
                        loadThumbnail(fileInfo);
                    }

                    // Calculate display dimensions while maintaining aspect ratio
                    float aspectRatio = 1.0f;
                    float thumbnailHeight = m_thumbnailSize;
                    float thumbnailWidth = m_thumbnailSize;

                    if (fileInfo.hasThumbnail && fileInfo.thumbnailWidth > 0 &&
                        fileInfo.thumbnailHeight > 0)
                    {
                        aspectRatio = static_cast<float>(fileInfo.thumbnailWidth) /
                                      static_cast<float>(fileInfo.thumbnailHeight);

                        // Calculate dimensions to fit within the standard size while preserving
                        // aspect ratio
                        if (aspectRatio > 1.0f) // Wider than tall
                        {
                            thumbnailHeight = m_thumbnailSize / aspectRatio;
                        }
                        else // Taller than wide or square
                        {
                            thumbnailWidth = m_thumbnailSize * aspectRatio;
                        }
                    }

                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

                    // Save the initial cursor position before the selectable
                    ImVec2 itemStartPos = ImGui::GetCursorPos();

                    // Create a selectable container with fixed height for consistent layout
                    bool isClicked = ImGui::Selectable("##selector",
                                                       false,
                                                       ImGuiSelectableFlags_AllowDoubleClick,
                                                       ImVec2(m_thumbnailSize, itemHeight));

                    // Save selectable height to properly position elements
                    ImVec2 selectableSize = ImGui::GetItemRectSize();

                    // Return cursor to beginning of the item so we can draw inside the selectable
                    // area
                    ImGui::SetCursorPos(itemStartPos);

                    // Handle thumbnail click event after setting up visual elements
                    if (isClicked && ImGui::IsMouseDoubleClicked(0))
                    {
                        // Double click to import all functions of the file
                        try
                        {
                            doc->merge(fileInfo.filePath);
                            
                            if (m_logger)
                            {
                                m_logger->addEvent(
                                  {fmt::format("Loaded file: {}", fileInfo.fileName),
                                   events::Severity::Info});
                            }
                        }
                        catch (const std::exception & e)
                        {
                            if (m_logger)
                            {
                                m_logger->addEvent({fmt::format("Failed to load file {}: {}",
                                                                fileInfo.fileName,
                                                                e.what()),
                                                    events::Severity::Error});
                            }
                        }
                    }

                    // Center the thumbnail horizontally and vertically within the item area
                    float centeringOffsetX = (m_thumbnailSize - thumbnailWidth) * 0.5f;
                    float centeringOffsetY = (m_thumbnailSize - thumbnailHeight) * 0.5f;

                    // Calculate proper thumbnail position (centered in the top portion of the
                    // selectable)
                    ImVec2 thumbnailPos =
                      ImVec2(itemStartPos.x + centeringOffsetX, itemStartPos.y + centeringOffsetY);

                    // Set cursor position to draw the thumbnail
                    ImGui::SetCursorPos(thumbnailPos);

                    // Display the thumbnail or placeholder
                    if (fileInfo.hasThumbnail && fileInfo.thumbnailTextureId != 0)
                    {
                        ImGui::Image(reinterpret_cast<void *>(
                                       static_cast<intptr_t>(fileInfo.thumbnailTextureId)),
                                     ImVec2(thumbnailWidth, thumbnailHeight));
                    }
                    else
                    {
                        // Draw a placeholder if no thumbnail is available
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                        ImGui::Button(reinterpret_cast<const char *>(ICON_FA_FILE_IMPORT),
                                      ImVec2(m_thumbnailSize, m_thumbnailSize));
                        ImGui::PopStyleColor();
                    }

                    ImGui::PopStyleVar();

                    // Position text at the bottom of the selectable area
                    float textHeight = ImGui::GetTextLineHeight();
                    float textY =
                      itemStartPos.y + selectableSize.y - textHeight - 5.0f; // 5px margin

                    // Add file name below the thumbnail
                    float textWidth = ImGui::CalcTextSize(fileInfo.fileName.c_str()).x;

                    if (textWidth > m_thumbnailSize)
                    {
                        // Truncate file name if it's too long
                        std::string truncatedName = fileInfo.fileName.substr(0, 20) + "...";
                        float posX =
                          itemStartPos.x +
                          (m_thumbnailSize - ImGui::CalcTextSize(truncatedName.c_str()).x) * 0.5f;

                        ImGui::SetCursorPos(ImVec2(posX, textY));
                        ImGui::TextUnformatted(truncatedName.c_str());
                    }
                    else
                    {
                        float posX = itemStartPos.x + (m_thumbnailSize - textWidth) * 0.5f;
                        ImGui::SetCursorPos(ImVec2(posX, textY));
                        ImGui::TextUnformatted(fileInfo.fileName.c_str());
                    }

                    ImGui::EndGroup();
                    ImGui::PopID();

                    fileIndex++;
                }
            }
    }

} // namespace gladius::ui
