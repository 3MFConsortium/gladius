#include "ThreemfFileViewer.h"
#include "Widgets.h"
#include "FileChooser.h"
#include "imgui.h"
#include "io/3mf/Importer3mf.h"
#include "../IconFontCppHeaders/IconsFontAwesome5.h"
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
        catch (const std::exception& e)
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
        for (auto& fileInfo : m_files)
        {
            if (fileInfo.thumbnailTextureId != 0)
            {
                glDeleteTextures(1, &fileInfo.thumbnailTextureId);
                fileInfo.thumbnailTextureId = 0;
            }
        }
    }

    void ThreemfFileViewer::setDirectory(const std::filesystem::path& directory)
    {
        if (m_directory != directory)
        {
            m_directory = directory;
            m_needsRefresh = true;
        }
    }

    void ThreemfFileViewer::setVisibility(bool visible)
    {
        m_visible = visible;
    }

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
        for (auto& fileInfo : m_files)
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
            for (const auto& entry : std::filesystem::directory_iterator(m_directory))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".3mf")
                {
                    ThreemfFileInfo fileInfo;
                    fileInfo.filePath = entry.path();
                    fileInfo.fileName = entry.path().filename().string();
                    m_files.push_back(fileInfo);
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            if (m_logger)
            {
                m_logger->addEvent({e.what(), events::Severity::Error});
            }
        }

        m_needsRefresh = false;
    }

    std::vector<unsigned char> ThreemfFileViewer::extractThumbnail(const std::filesystem::path& filePath)
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
        catch (const std::exception& e)
        {
            if (m_logger)
            {
                m_logger->addEvent({fmt::format("Failed to extract thumbnail from {}: {}", 
                                   filePath.string(), e.what()), 
                                   events::Severity::Warning});
            }
        }
        
        return thumbnailData;
    }

    void ThreemfFileViewer::loadThumbnail(ThreemfFileInfo& fileInfo)
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
                                      fileInfo.fileName, lodepng_error_text(error)),
                                      events::Severity::Warning});
                }
            }
        }
    }

    void ThreemfFileViewer::createThumbnailTexture(ThreemfFileInfo& fileInfo)
    {
        if (fileInfo.thumbnailTextureId != 0 || !fileInfo.hasThumbnail || fileInfo.thumbnailData.empty())
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, decodedImage.data());
        
        // Unbind the texture
        glBindTexture(GL_TEXTURE_2D, 0);
        
        // Store the texture ID
        fileInfo.thumbnailTextureId = textureId;
    }

    void ThreemfFileViewer::render(SharedDocument doc)
    {
        if (!m_visible)
        {
            return;
        }

        // Scan the directory if needed
        scanDirectory();

        // Configure window
        ImGuiWindowFlags windowFlags = 0;
        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
        
        if (ImGui::Begin("3MF File Browser", &m_visible, windowFlags))
        {
            // Directory selection and refresh button
            ImGui::Text("Directory: %s", m_directory.string().c_str());
            ImGui::SameLine();
            if (ImGui::Button("Select Directory"))
            {
                auto selectedDir = queryDirectory(m_directory);
                if (selectedDir)
                {
                    setDirectory(selectedDir.value());
                }
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Refresh"))
            {
                refreshDirectory();
            }

            ImGui::Separator();

            // Settings
            if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::SliderFloat("Thumbnail Size", &m_thumbnailSize, 100.0f, 400.0f);
                ImGui::SliderInt("Columns", &m_columns, 1, 6);
            }

            ImGui::Separator();

            // Display files in a grid
            if (m_files.empty())
            {
                ImGui::TextUnformatted("No 3MF files found in the specified directory");
            }
            else
            {
                // Calculate the number of files and start the child window
                float childHeight = ImGui::GetContentRegionAvail().y;
                ImGui::BeginChild("FilesView", ImVec2(0, childHeight), false);
                
                // Calculate item width based on available width and desired columns
                float windowWidth = ImGui::GetContentRegionAvail().x;
                float itemWidth = (windowWidth - ImGui::GetStyle().ItemSpacing.x * (m_columns - 1)) / m_columns;
                
                // Loop through files and display them in a grid
                int fileIndex = 0;
                for (auto& fileInfo : m_files)
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
                    if (!fileInfo.thumbnailLoaded)
                    {
                        loadThumbnail(fileInfo);
                    }
                    
                    // Display the thumbnail or placeholder
                    float aspectRatio = 1.0f;
                    float thumbnailHeight = m_thumbnailSize;
                    
                    if (fileInfo.hasThumbnail && fileInfo.thumbnailWidth > 0 && fileInfo.thumbnailHeight > 0)
                    {
                        aspectRatio = static_cast<float>(fileInfo.thumbnailWidth) / static_cast<float>(fileInfo.thumbnailHeight);
                        thumbnailHeight = m_thumbnailSize / aspectRatio;
                    }
                    
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                    
                    // Create a selectable container for the file item
                    if (ImGui::Selectable("##selector", false, ImGuiSelectableFlags_AllowDoubleClick, 
                                         ImVec2(m_thumbnailSize, thumbnailHeight + 40)))
                    {
                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            // Double click to open the file
                            try
                            {
                                io::loadFrom3mfFile(fileInfo.filePath, *doc);
                                // Don't need to mark the file as saved
                                
                                if (m_logger)
                                {
                                    m_logger->addEvent({fmt::format("Loaded file: {}", fileInfo.fileName), 
                                                      events::Severity::Info});
                                }
                            }
                            catch (const std::exception& e)
                            {
                                if (m_logger)
                                {
                                    m_logger->addEvent({fmt::format("Failed to load file {}: {}", 
                                                     fileInfo.fileName, e.what()), 
                                                     events::Severity::Error});
                                }
                            }
                        }
                    }
                    
                    // Set the cursor position for the thumbnail or placeholder
                    ImGui::SetCursorPos(ImGui::GetCursorPos());
                    
                    // Display the thumbnail or placeholder
                    if (fileInfo.hasThumbnail && fileInfo.thumbnailTextureId != 0)
                    {
                        ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(fileInfo.thumbnailTextureId)), 
                                    ImVec2(m_thumbnailSize, thumbnailHeight));
                    }
                    else
                    {
                        // Draw a placeholder if no thumbnail is available
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                        ImGui::Button(reinterpret_cast<const char*>(ICON_FA_FILE_IMPORT), 
                                     ImVec2(m_thumbnailSize, m_thumbnailSize));
                        ImGui::PopStyleColor();
                    }
                    
                    ImGui::PopStyleVar();
                    
                    // Add file name below the thumbnail
                    float textWidth = ImGui::CalcTextSize(fileInfo.fileName.c_str()).x;
                    if (textWidth > m_thumbnailSize)
                    {
                        // Truncate file name if it's too long
                        std::string truncatedName = fileInfo.fileName.substr(0, 20) + "...";
                        float posX = (m_thumbnailSize - ImGui::CalcTextSize(truncatedName.c_str()).x) * 0.5f;
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + posX);
                        ImGui::TextUnformatted(truncatedName.c_str());
                    }
                    else
                    {
                        float posX = (m_thumbnailSize - textWidth) * 0.5f;
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + posX);
                        ImGui::TextUnformatted(fileInfo.fileName.c_str());
                    }
                    
                    ImGui::EndGroup();
                    ImGui::PopID();
                    
                    fileIndex++;
                }
                
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }

} // namespace gladius::ui
