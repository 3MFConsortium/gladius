#include "WelcomeScreen.h"
#include "imgui.h"
#include "IconsFontAwesome5.h"
#include <fmt/format.h>
#include <iostream>

namespace gladius::ui
{

    void WelcomeScreen::setOpenFileCallback(std::function<void(const std::filesystem::path&)> callback)
    {
        m_openFileCallback = std::move(callback);
    }
    
    void WelcomeScreen::setNewModelCallback(std::function<void()> callback)
    {
        m_newModelCallback = std::move(callback);
    }
    
    void WelcomeScreen::setShowLibraryCallback(std::function<void()> callback)
    {
        m_showLibraryCallback = std::move(callback);
    }
    
    void WelcomeScreen::setRecentFiles(const std::vector<std::pair<std::filesystem::path, std::time_t>>& recentFiles)
    {
        if (m_recentFiles != recentFiles)
        {
            m_recentFiles = recentFiles;
            m_needsRefresh = true;
            
            // Update thumbnails if the thumbnail extractor is available
            if (m_thumbnailExtractor)
            {
                updateThumbnailInfos();
            }
        }
    }
    
void WelcomeScreen::updateThumbnailInfos()
{
    if (!m_thumbnailExtractor)
    {
        return;
    }
    
    // Clear existing thumbnails if we're refreshing
    if (m_needsRefresh)
    {
        for (auto& info : m_thumbnailInfos)
        {
            m_thumbnailExtractor->releaseThumbnail(info);
        }
        m_thumbnailInfos.clear();
    }
    
    // Create new thumbnail infos for all recent files
    for (const auto& recentFile : m_recentFiles)
    {
        const auto& filePath = recentFile.first;
        const auto& timestamp = recentFile.second;
        
        // Check if we already have this file
        auto it = std::find_if(m_thumbnailInfos.begin(), m_thumbnailInfos.end(),
            [&](const auto& info) { return info.filePath == filePath; });
            
        if (it == m_thumbnailInfos.end())
        {
            // Create new thumbnail info
            auto info = m_thumbnailExtractor->createThumbnailInfo(filePath, timestamp);
            
            // Load the thumbnail right away
            m_thumbnailExtractor->loadThumbnail(info);
            
            m_thumbnailInfos.push_back(std::move(info));
        }
    }
    
    // Remove thumbnails for files that are no longer in the recent files list
    m_thumbnailInfos.erase(
        std::remove_if(m_thumbnailInfos.begin(), m_thumbnailInfos.end(),
            [this](const auto& info) {
                auto it = std::find_if(m_recentFiles.begin(), m_recentFiles.end(),
                    [&info](const auto& pair) { return pair.first == info.filePath; });
                if (it == m_recentFiles.end())
                {
                    // Release the thumbnail resources before removing
                    m_thumbnailExtractor->releaseThumbnail(const_cast<ThreemfThumbnailExtractor::ThumbnailInfo&>(info));
                    return true;
                }
                return false;
            }),
        m_thumbnailInfos.end());
    
    m_needsRefresh = false;
}
    
    void WelcomeScreen::setLogger(events::SharedLogger logger)
    {
        m_logger = std::move(logger);
        
        // Initialize the thumbnail extractor if it doesn't exist
        if (!m_thumbnailExtractor && m_logger)
        {
            m_thumbnailExtractor = std::make_unique<ThreemfThumbnailExtractor>(m_logger);
            
            // Force a refresh of thumbnails if we have any recent files
            if (!m_recentFiles.empty())
            {
                m_needsRefresh = true;
                updateThumbnailInfos();
            }
        }
    }
    
    bool WelcomeScreen::render()
    {
        if (!m_isVisible)
        {
            return false;
        }
        
        // Set up window styling
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | 
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        const float windowWidth = std::min(1024.0f, displaySize.x * 0.8f);
        const float windowHeight = std::min(768.0f, displaySize.y * 0.8f);

        // Center the window on screen
        ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f), 
                               ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        
        ImGui::SetNextWindowBgAlpha(0.9f);
        
        if (ImGui::Begin("Welcome to Gladius", &m_isVisible, windowFlags))
        {
            // Title section with larger font
            const float titleFontScale = 1.5f;
            ImGui::PushFont(ImGui::GetFont());
            ImGui::SetWindowFontScale(titleFontScale);
            
            const char* title = "Welcome to Gladius";
            ImVec2 titleSize = ImGui::CalcTextSize(title);
            ImGui::SetCursorPosX((windowWidth - titleSize.x) * 0.5f);
            ImGui::TextUnformatted(title);
            
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();
            
            ImGui::Separator();
            ImGui::Spacing();
            
            // Main content split: left side for buttons, right side for recent files
            const float buttonWidth = 200.0f;
            const float listWidth = windowWidth - buttonWidth - 40.0f; // 20px padding on each side
            
            // Left side - actions
            ImGui::BeginChild("ActionsPane", ImVec2(buttonWidth, 0), true);
            
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
            
            if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_FILE " New Project"), ImVec2(-1, 50)))
            {
                if (m_newModelCallback)
                {
                    m_newModelCallback();
                    m_isVisible = false;
                }
            }
            
            if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_FOLDER_OPEN " Open Existing"), ImVec2(-1, 50)))
            {
                if (m_openFileCallback)
                {
                    m_openFileCallback(std::filesystem::path()); // Empty path signals to show file dialog
                    m_isVisible = false;
                }
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_FOLDER_OPEN " Library Browser"), ImVec2(-1, 50)))
            {
                if (m_showLibraryCallback)
                {
                    m_showLibraryCallback();
                    m_isVisible = false;
                }
            }
            
            ImGui::PopStyleVar();
            ImGui::EndChild();
            
            ImGui::SameLine();
            
            // Right side - recent files
            ImGui::BeginChild("RecentFilesPane", ImVec2(listWidth, 0), true);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8));
            
            ImGui::TextUnformatted("Recent Files");
            ImGui::Separator();
            
            if (m_recentFiles.empty())
            {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No recent files found");
            }
            else if (!m_thumbnailExtractor)
            {
                // If we don't have a thumbnail extractor, show a simple list of files
                for (const auto& [filePath, timestamp] : m_recentFiles)
                {
                    // Format the timestamp
                    std::tm* timeInfo = std::localtime(&timestamp);
                    char timeStr[64];
                    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", timeInfo);
                    
                    // Use a button that looks like a selectable
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
                    
                    const std::string fileName = filePath.filename().string();
                    const std::string folderName = filePath.parent_path().filename().string();
                    const std::string displayText = fmt::format("{}\n{} | {}", 
                        fileName, folderName, timeStr);
                    
                    if (ImGui::Button(displayText.c_str(), ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 2.5f)))
                    {
                        if (m_openFileCallback)
                        {
                            m_openFileCallback(filePath);
                            m_isVisible = false;
                        }
                    }
                    
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(3);
                    ImGui::Separator();
                }
            }
            else
            {
                // Update thumbnail infos if needed
                if (m_needsRefresh && m_thumbnailExtractor)
                {
                    updateThumbnailInfos();
                }
                
                // Calculate grid layout based on available width
                float availWidth = ImGui::GetContentRegionAvail().x - 20.0f; // 20px padding
                float cellWidth = m_thumbnailSize + 20;
                float cellHeight = m_thumbnailSize + 60;

                // Calculate number of columns based on available width
                int columns = std::max(1, static_cast<int>(std::floor(availWidth / cellWidth)));
                m_columns = columns;
                
                // Adjust cellWidth to evenly distribute space
                cellWidth = (availWidth - (ImGui::GetStyle().ItemSpacing.x * (columns - 1))) / columns;

                // Start the thumbnail grid
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 10));
                
                // Process one file at a time to ensure thumbnails load incrementally
                bool oneThumbnailLoaded = false;
                
                int itemIdx = 0;
                for (auto& info : m_thumbnailInfos)
                {
                    // Load thumbnail if not already loaded and we haven't loaded one this frame
                    if (!info.thumbnailLoaded && m_thumbnailExtractor && !oneThumbnailLoaded)
                    {
                        m_thumbnailExtractor->loadThumbnail(info);
                        oneThumbnailLoaded = true;
                    }
                    
                    // Create a unique ID for the item
                    ImGui::PushID(itemIdx);
                    
                    // Position in grid
                    if (itemIdx % columns != 0)
                    {
                        ImGui::SameLine();
                    }
                    
                    // Begin a group for the whole item (thumbnail + text)
                    ImGui::BeginGroup();
                    
                    // Save cursor position for item
                    ImVec2 itemPos = ImGui::GetCursorPos();
                    
                    // Create a selectable area for the whole thumbnail
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.1f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
                    
                    if (ImGui::Button("##thumbnail", ImVec2(cellWidth, cellHeight)))
                    {
                        if (m_openFileCallback)
                        {
                            m_openFileCallback(info.filePath);
                            m_isVisible = false;
                        }
                    }
                    
                    // Add tooltip when hovering over the thumbnail
                    if (ImGui::IsItemHovered())
                    {
                        // Create tooltip content
                        ImGui::BeginTooltip();
                        
                        // File path and size
                        ImGui::TextUnformatted(fmt::format("Path: {}", info.filePath.string()).c_str());
                        
                        // Show file size in appropriate units
                        std::string sizeStr;
                        if (info.fileInfo.fileSize > 1024 * 1024) {
                            sizeStr = fmt::format("Size: {:.2f} MB", 
                                               static_cast<double>(info.fileInfo.fileSize) / (1024.0 * 1024.0));
                        } else if (info.fileInfo.fileSize > 1024) {
                            sizeStr = fmt::format("Size: {:.2f} KB", 
                                               static_cast<double>(info.fileInfo.fileSize) / 1024.0);
                        } else {
                            sizeStr = fmt::format("Size: {} bytes", info.fileInfo.fileSize);
                        }
                        ImGui::TextUnformatted(sizeStr.c_str());
                        
                        ImGui::Separator();
                        
                        // 3MF metadata
                        if (!info.fileInfo.metadata.empty())
                        {
                            ImGui::TextUnformatted("3MF Metadata:");
                            for (const auto& item : info.fileInfo.metadata)
                            {
                                ImGui::BulletText("%s: %s", item.key.c_str(), item.value.c_str());
                            }
                        }
                        else
                        {
                            ImGui::TextUnformatted("No metadata available");
                        }
                        
                        ImGui::EndTooltip();
                    }
                    
                    ImGui::PopStyleColor(3);
                    
                    // Draw content over the button
                    ImGui::SetItemAllowOverlap();
                    ImGui::SetCursorPos(itemPos);
                    
                    // Center the thumbnail in the cell
                    float thumbPosX = itemPos.x + (cellWidth - m_thumbnailSize) * 0.5f;
                    ImGui::SetCursorPos(ImVec2(thumbPosX, itemPos.y + 5.0f));
                    
                    // Draw the thumbnail or placeholder
                    if (info.hasThumbnail && info.thumbnailTextureId != 0)
                    {
                        // Calculate aspect ratio for proper display
                        float aspectRatio = 1.0f;
                        float displayWidth = m_thumbnailSize;
                        float displayHeight = m_thumbnailSize;
                        
                        if (info.thumbnailWidth > 0 && info.thumbnailHeight > 0)
                        {
                            aspectRatio = static_cast<float>(info.thumbnailWidth) / 
                                          static_cast<float>(info.thumbnailHeight);
                                          
                            if (aspectRatio > 1.0f) // Wider than tall
                            {
                                displayHeight = m_thumbnailSize / aspectRatio;
                            }
                            else // Taller than wide or square
                            {
                                displayWidth = m_thumbnailSize * aspectRatio;
                            }
                        }
                        
                        // Center the thumbnail based on its aspect ratio
                        float centerX = thumbPosX + (m_thumbnailSize - displayWidth) * 0.5f;
                        ImGui::SetCursorPos(ImVec2(centerX, ImGui::GetCursorPosY() + 
                                                  (m_thumbnailSize - displayHeight) * 0.5f));
                        
                        ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(info.thumbnailTextureId)), 
                                    ImVec2(displayWidth, displayHeight));
                    }
                    else
                    {
                        // Draw placeholder
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
                        
                        if (ImGui::Button(reinterpret_cast<const char*>(ICON_FA_FILE_ALT), 
                                         ImVec2(m_thumbnailSize, m_thumbnailSize)))
                        {
                            // Clicking the placeholder should also open the file
                            if (m_openFileCallback)
                            {
                                m_openFileCallback(info.filePath);
                                m_isVisible = false;
                            }
                        }
                        
                        // Add tooltip for placeholder as well
                        if (ImGui::IsItemHovered())
                        {
                            // Create tooltip content
                            ImGui::BeginTooltip();
                            
                            // File path and size
                            ImGui::TextUnformatted(fmt::format("Path: {}", info.filePath.string()).c_str());
                            
                            // Show file size in appropriate units
                            std::string sizeStr;
                            if (info.fileInfo.fileSize > 1024 * 1024) {
                                sizeStr = fmt::format("Size: {:.2f} MB", 
                                                   static_cast<double>(info.fileInfo.fileSize) / (1024.0 * 1024.0));
                            } else if (info.fileInfo.fileSize > 1024) {
                                sizeStr = fmt::format("Size: {:.2f} KB", 
                                                   static_cast<double>(info.fileInfo.fileSize) / 1024.0);
                            } else {
                                sizeStr = fmt::format("Size: {} bytes", info.fileInfo.fileSize);
                            }
                            ImGui::TextUnformatted(sizeStr.c_str());
                            
                            ImGui::Separator();
                            
                            // 3MF metadata
                            if (!info.fileInfo.metadata.empty())
                            {
                                ImGui::TextUnformatted("3MF Metadata:");
                                for (const auto& item : info.fileInfo.metadata)
                                {
                                    ImGui::BulletText("%s: %s", item.key.c_str(), item.value.c_str());
                                }
                            }
                            else
                            {
                                ImGui::TextUnformatted("No metadata available");
                            }
                            
                            ImGui::EndTooltip();
                        }
                        
                        ImGui::PopStyleColor(3);
                    }
                    
                    // File name and timestamp below thumbnail
                    const char* fileName = info.fileName.c_str();
                    ImVec2 textSize = ImGui::CalcTextSize(fileName);
                    
                    // Position text below thumbnail
                    float textY = itemPos.y + m_thumbnailSize + 15.0f;
                    ImGui::SetCursorPos(ImVec2(itemPos.x + (cellWidth - textSize.x) * 0.5f, textY));
                    
                    // Draw filename with ellipsis if too long
                    if (textSize.x > cellWidth - 10)
                    {
                        // Truncate filename if too long
                        std::string truncatedName = info.fileName;
                        if (truncatedName.length() > 15)
                        {
                            truncatedName = truncatedName.substr(0, 12) + "...";
                        }
                        ImGui::TextUnformatted(truncatedName.c_str());
                    }
                    else
                    {
                        ImGui::TextUnformatted(fileName);
                    }
                    
                    // Format timestamp
                    std::tm* timeInfo = std::localtime(&info.timestamp);
                    char timeStr[64];
                    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d", timeInfo);
                    
                    textSize = ImGui::CalcTextSize(timeStr);
                    ImGui::SetCursorPos(ImVec2(itemPos.x + (cellWidth - textSize.x) * 0.5f, 
                                              ImGui::GetCursorPosY() + 5.0f));
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", timeStr);
                    
                    ImGui::EndGroup();
                    ImGui::PopID();
                    
                    itemIdx++;
                }
                
                ImGui::PopStyleVar(2);
            }
            
            ImGui::PopStyleVar();
            ImGui::EndChild();
        }
        ImGui::End();
        
        return m_isVisible;
    }
    
    bool WelcomeScreen::isVisible() const
    {
        return m_isVisible;
    }
    
    void WelcomeScreen::show()
    {
        m_isVisible = true;
    }
    
    void WelcomeScreen::hide()
    {
        m_isVisible = false;
    }
}
