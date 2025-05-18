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
        m_recentFiles = recentFiles;
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
        const float windowWidth = std::min(800.0f, displaySize.x * 0.8f);
        const float windowHeight = std::min(600.0f, displaySize.y * 0.8f);
        
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
            const float buttonWidth = windowWidth * 0.3f;
            const float listWidth = windowWidth * 0.6f;
            
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
            else
            {
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
