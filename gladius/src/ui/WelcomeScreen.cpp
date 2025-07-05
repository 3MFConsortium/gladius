#include "WelcomeScreen.h"
#include "BackupManager.h"
#include "EventLogger.h"
#include "IconsFontAwesome5.h"
#include "imgui.h"
#include <chrono>
#include <filesystem>
#include <fmt/format.h>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace gladius::ui
{
    /**
     * @brief Thread-safe cross-platform function to convert time_t to tm structure
     *
     * @param timer The time_t value to convert
     * @param result Pointer to tm structure to store the result
     * @return std::tm* Pointer to the result tm structure, or nullptr on error
     */
    std::tm * safeLocaltime(std::time_t const * timer, std::tm * result)
    {
#ifdef _WIN32
        // Use localtime_s on Windows (thread-safe)
        return (localtime_s(result, timer) == 0) ? result : nullptr;
#else
        // Use localtime_r on POSIX systems (thread-safe)
        return localtime_r(timer, result);
#endif
    }

    /**
     * @brief Formats a timestamp in a human-readable format
     *
     * @param timestamp The timestamp to format
     * @return std::string Formatted timestamp with context-aware formatting:
     *   - Today: "Today at HH:MM"
     *   - Yesterday: "Yesterday at HH:MM"
     *   - This week: "DayName at HH:MM"
     *   - This year: "Month Day at HH:MM"
     *   - Older: "YYYY-MM-DD HH:MM"
     */
    std::string formatTimeForHuman(std::time_t const & timestamp)
    {
        // Get current time
        std::time_t const now = std::time(nullptr);

        // Use thread-safe localtime alternatives
        std::tm nowInfo{};
        std::tm timeInfo{};

        if (safeLocaltime(&now, &nowInfo) == nullptr ||
            safeLocaltime(&timestamp, &timeInfo) == nullptr)
        {
            // Fallback to simple format if localtime fails
            return fmt::format("{}", timestamp);
        } // Calculate days difference by converting to struct tm and comparing
        int const daysDiff = static_cast<int>(
          (now - timestamp) / (60 * 60 * 24) +
          (nowInfo.tm_hour < timeInfo.tm_hour ||
               (nowInfo.tm_hour == timeInfo.tm_hour && nowInfo.tm_min < timeInfo.tm_min)
             ? 1
             : 0));

        // Format time part "HH:MM"
        char timePartStr[16];
        std::strftime(timePartStr, sizeof(timePartStr), "%H:%M", &timeInfo);

        if (daysDiff == 0)
        {
            // Today
            return fmt::format("Today at {}", timePartStr);
        }
        else if (daysDiff == 1)
        {
            // Yesterday
            return fmt::format("Yesterday at {}", timePartStr);
        }
        else if (daysDiff < 7)
        {
            // This week
            char dayNameStr[16];
            std::strftime(dayNameStr, sizeof(dayNameStr), "%A", &timeInfo);
            return fmt::format("{} at {}", dayNameStr, timePartStr);
        }
        else if (timeInfo.tm_year == nowInfo.tm_year)
        {
            // This year
            char monthDayStr[16];
            std::strftime(monthDayStr, sizeof(monthDayStr), "%b %d", &timeInfo);
            return fmt::format("{} at {}", monthDayStr, timePartStr);
        }
        else
        {
            // Older
            char dateStr[64];
            std::strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M", &timeInfo);
            return dateStr;
        }
    }

    void
    WelcomeScreen::setOpenFileCallback(std::function<void(const std::filesystem::path &)> callback)
    {
        m_openFileCallback = std::move(callback);
    }

    void WelcomeScreen::setNewModelCallback(std::function<void()> callback)
    {
        m_newModelCallback = std::move(callback);
    }

    void WelcomeScreen::setRecentFiles(
      const std::vector<std::pair<std::filesystem::path, std::time_t>> & recentFiles)
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

    void WelcomeScreen::setBackupManager(const BackupManager * backupManager)
    {
        m_backupManager = backupManager;
        updateActiveTab();
    }

    void WelcomeScreen::setRestoreBackupCallback(
      std::function<void(const std::filesystem::path &)> callback)
    {
        m_restoreBackupCallback = std::move(callback);
    }

    void WelcomeScreen::setExamplesDirectory(const std::filesystem::path & examplesPath)
    {
        if (m_examplesDirectory != examplesPath)
        {
            m_examplesDirectory = examplesPath;
            m_examplesNeedRefresh = true;
            scanExamplesDirectory();
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
            for (auto & info : m_thumbnailInfos)
            {
                m_thumbnailExtractor->releaseThumbnail(info);
            }
            m_thumbnailInfos.clear();
        }

        // Create new thumbnail infos for all recent files
        for (const auto & recentFile : m_recentFiles)
        {
            const auto & filePath = recentFile.first;
            const auto & timestamp = recentFile.second;

            // Check if we already have this file
            auto it = std::find_if(m_thumbnailInfos.begin(),
                                   m_thumbnailInfos.end(),
                                   [&](const auto & info) { return info.filePath == filePath; });

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
          std::remove_if(m_thumbnailInfos.begin(),
                         m_thumbnailInfos.end(),
                         [this](const auto & info)
                         {
                             auto it = std::find_if(m_recentFiles.begin(),
                                                    m_recentFiles.end(),
                                                    [&info](const auto & pair)
                                                    { return pair.first == info.filePath; });
                             if (it == m_recentFiles.end())
                             {
                                 // Release the thumbnail resources before removing
                                 m_thumbnailExtractor->releaseThumbnail(
                                   const_cast<ThreemfThumbnailExtractor::ThumbnailInfo &>(info));
                                 return true;
                             }
                             return false;
                         }),
          m_thumbnailInfos.end());

        m_needsRefresh = false;
    }

    void WelcomeScreen::scanExamplesDirectory()
    {
        m_exampleFiles.clear();

        if (m_examplesDirectory.empty() || !std::filesystem::exists(m_examplesDirectory))
        {
            return;
        }

        try
        {
            // Scan for .3mf files in the examples directory and subdirectories
            for (const auto & entry :
                 std::filesystem::recursive_directory_iterator(m_examplesDirectory))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".3mf")
                {
                    // Get file modification time
                    auto fileTime = std::filesystem::last_write_time(entry);
                    auto timeT = std::chrono::system_clock::to_time_t(
                      std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        fileTime - std::filesystem::file_time_type::clock::now() +
                        std::chrono::system_clock::now()));

                    m_exampleFiles.emplace_back(entry.path(), timeT);
                }
            }

            // Sort by filename for consistent display
            std::sort(m_exampleFiles.begin(),
                      m_exampleFiles.end(),
                      [](const auto & a, const auto & b)
                      { return a.first.filename().string() < b.first.filename().string(); });

            // Update thumbnails if the thumbnail extractor is available
            if (m_thumbnailExtractor)
            {
                updateExampleThumbnailInfos();
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->addEvent({fmt::format("Error scanning examples directory: {}", e.what()),
                                    events::Severity::Error});
            }
        }
    }

    void WelcomeScreen::updateExampleThumbnailInfos()
    {
        if (!m_thumbnailExtractor)
        {
            return;
        }

        // Clear existing thumbnails if we're refreshing
        if (m_examplesNeedRefresh)
        {
            for (auto & info : m_exampleThumbnailInfos)
            {
                m_thumbnailExtractor->releaseThumbnail(info);
            }
            m_exampleThumbnailInfos.clear();
        }

        // Create new thumbnail infos for all example files
        for (const auto & exampleFile : m_exampleFiles)
        {
            const auto & filePath = exampleFile.first;
            const auto & timestamp = exampleFile.second;

            // Check if we already have this file
            auto it = std::find_if(m_exampleThumbnailInfos.begin(),
                                   m_exampleThumbnailInfos.end(),
                                   [&](const auto & info) { return info.filePath == filePath; });

            if (it == m_exampleThumbnailInfos.end())
            {
                // Create new thumbnail info
                auto info = m_thumbnailExtractor->createThumbnailInfo(filePath, timestamp);

                // Load the thumbnail right away
                m_thumbnailExtractor->loadThumbnail(info);

                m_exampleThumbnailInfos.push_back(std::move(info));
            }
        }

        // Remove thumbnails for files that are no longer in the examples files list
        m_exampleThumbnailInfos.erase(
          std::remove_if(m_exampleThumbnailInfos.begin(),
                         m_exampleThumbnailInfos.end(),
                         [this](const auto & info)
                         {
                             auto it = std::find_if(m_exampleFiles.begin(),
                                                    m_exampleFiles.end(),
                                                    [&info](const auto & pair)
                                                    { return pair.first == info.filePath; });
                             if (it == m_exampleFiles.end())
                             {
                                 // Release the thumbnail resources before removing
                                 m_thumbnailExtractor->releaseThumbnail(
                                   const_cast<ThreemfThumbnailExtractor::ThumbnailInfo &>(info));
                                 return true;
                             }
                             return false;
                         }),
          m_exampleThumbnailInfos.end());

        m_examplesNeedRefresh = false;
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

            // Force a refresh of example thumbnails if we have any examples
            if (!m_exampleFiles.empty())
            {
                m_examplesNeedRefresh = true;
                updateExampleThumbnailInfos();
            }
        }
    }

    void WelcomeScreen::updateActiveTab()
    {
        if (m_backupManager && m_backupManager->hasPreviousSessionBackups())
        {
            m_activeTab = WelcomeTab::RestoreBackup;
            m_preferBackupTab = true;
        }
        else if (m_recentFiles.empty())
        {
            // If no recent files, default to Examples tab
            m_activeTab = WelcomeTab::Examples;
            m_preferBackupTab = false;
        }
        else
        {
            m_activeTab = WelcomeTab::RecentFiles;
            m_preferBackupTab = false;
        }
    }

    bool WelcomeScreen::render()
    {
        if (!m_isVisible)
        {
            return false;
        }

        // Update active tab based on backup availability
        updateActiveTab();

        // Set up window styling
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        const float windowWidth = std::min(1024.0f, displaySize.x * 0.8f);
        const float windowHeight = std::min(768.0f, displaySize.y * 0.8f);

        // Center the window on screen
        ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
        ImGui::SetNextWindowPos(
          ImVec2(displaySize.x * 0.5f, displaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        // Semi-transparent background
        ImGui::SetNextWindowBgAlpha(0.9f);

        if (ImGui::Begin("Welcome to Gladius", &m_isVisible, windowFlags))
        {
            // Title section with larger font
            const float titleFontScale = 1.5f;
            ImGui::PushFont(ImGui::GetFont());
            ImGui::SetWindowFontScale(titleFontScale);

            const char * title = "Welcome to Gladius";
            ImVec2 titleSize = ImGui::CalcTextSize(title);
            ImGui::SetCursorPosX((windowWidth - titleSize.x) * 0.5f);
            ImGui::TextUnformatted(title);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopFont();

            ImGui::Spacing();

            // Main content split: left side for buttons, right side for tabbed content
            const float buttonWidth = 200.0f;
            const float listWidth = windowWidth - buttonWidth - 40.0f; // 20px padding on each side

            // Left side - actions
            ImGui::BeginChild("ActionsPane", ImVec2(buttonWidth, 0), ImGuiChildFlags_None);

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

            if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_FILE " New Project"),
                              ImVec2(-1, 50)))
            {
                if (m_newModelCallback)
                {
                    m_newModelCallback();
                    m_isVisible = false;
                }
            }

            if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_FOLDER_OPEN " Open Existing"),
                              ImVec2(-1, 50)))
            {
                if (m_openFileCallback)
                {
                    m_openFileCallback(
                      std::filesystem::path()); // Empty path signals to show file dialog
                    m_isVisible = false;
                }
            }

            ImGui::PopStyleVar();
            ImGui::EndChild();

            ImGui::SameLine();

            // Right side - tabbed content
            ImGui::BeginChild("TabbedContentPane", ImVec2(listWidth, 0), true);

            // Tab bar
            if (ImGui::BeginTabBar("WelcomeTabBar", ImGuiTabBarFlags_None))
            {
                // Recent Files tab - only show if there are recent files
                if (!m_recentFiles.empty())
                {
                    bool recentFilesSelected = (m_activeTab == WelcomeTab::RecentFiles);
                    if (ImGui::BeginTabItem("Recent Files",
                                            nullptr,
                                            recentFilesSelected && !m_preferBackupTab
                                              ? ImGuiTabItemFlags_SetSelected
                                              : ImGuiTabItemFlags_None))
                    {
                        m_activeTab = WelcomeTab::RecentFiles;
                        renderRecentFilesTab(listWidth);
                        ImGui::EndTabItem();
                    }
                }

                // Examples tab
                bool examplesSelected = (m_activeTab == WelcomeTab::Examples);
                if (ImGui::BeginTabItem("Examples",
                                        nullptr,
                                        examplesSelected ? ImGuiTabItemFlags_SetSelected
                                                         : ImGuiTabItemFlags_None))
                {
                    m_activeTab = WelcomeTab::Examples;
                    renderExamplesTab(listWidth);
                    ImGui::EndTabItem();
                }

                // Restore Backup tab
                bool restoreBackupSelected = (m_activeTab == WelcomeTab::RestoreBackup);
                if (ImGui::BeginTabItem("Restore Backup",
                                        nullptr,
                                        restoreBackupSelected && m_preferBackupTab
                                          ? ImGuiTabItemFlags_SetSelected
                                          : ImGuiTabItemFlags_None))
                {
                    m_activeTab = WelcomeTab::RestoreBackup;
                    renderRestoreBackupTab(listWidth);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::EndChild();
        }
        ImGui::End();

        return m_isVisible;
    }

    void WelcomeScreen::renderRecentFilesTab(float availableWidth)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8));

        if (m_recentFiles.empty())
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No recent files found");
        }
        else if (!m_thumbnailExtractor)
        {
            // If we don't have a thumbnail extractor, show a simple list of files
            for (const auto & [filePath, timestamp] : m_recentFiles)
            {
                // Format the timestamp in a human-readable format
                std::string timeStr = formatTimeForHuman(timestamp);

                // Use a button that looks like a selectable
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                      ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                      ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

                const std::string fileName = filePath.filename().string();
                const std::string folderName = filePath.parent_path().filename().string();
                const std::string displayText =
                  fmt::format("{}\n{} | {}", fileName, folderName, timeStr);

                if (ImGui::Button(displayText.c_str(),
                                  ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 2.5f)))
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
            for (auto & info : m_thumbnailInfos)
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
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                      ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                      ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));

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
                    if (info.fileInfo.fileSize > 1024 * 1024)
                    {
                        sizeStr = fmt::format("Size: {:.2f} MB",
                                              static_cast<double>(info.fileInfo.fileSize) /
                                                (1024.0 * 1024.0));
                    }
                    else if (info.fileInfo.fileSize > 1024)
                    {
                        sizeStr = fmt::format("Size: {:.2f} KB",
                                              static_cast<double>(info.fileInfo.fileSize) / 1024.0);
                    }
                    else
                    {
                        sizeStr = fmt::format("Size: {} bytes", info.fileInfo.fileSize);
                    }
                    ImGui::TextUnformatted(sizeStr.c_str());

                    // Display the timestamp in human-readable format
                    std::string timeStr = formatTimeForHuman(info.timestamp);
                    ImGui::TextUnformatted(fmt::format("Last Opened: {}", timeStr).c_str());

                    ImGui::Separator();

                    // 3MF metadata
                    if (!info.fileInfo.metadata.empty())
                    {
                        ImGui::TextUnformatted("3MF Metadata:");
                        for (const auto & item : info.fileInfo.metadata)
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
                    ImGui::SetCursorPos(ImVec2(
                      centerX, ImGui::GetCursorPosY() + (m_thumbnailSize - displayHeight) * 0.5f));

                    ImGui::Image(
                      reinterpret_cast<void *>(static_cast<intptr_t>(info.thumbnailTextureId)),
                      ImVec2(displayWidth, displayHeight));
                }
                else
                {
                    // Draw placeholder
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));

                    if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_FILE_ALT),
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
                        ImGui::TextUnformatted(
                          fmt::format("Path: {}", info.filePath.string()).c_str());

                        // Show file size in appropriate units
                        std::string sizeStr;
                        if (info.fileInfo.fileSize > 1024 * 1024)
                        {
                            sizeStr = fmt::format("Size: {:.2f} MB",
                                                  static_cast<double>(info.fileInfo.fileSize) /
                                                    (1024.0 * 1024.0));
                        }
                        else if (info.fileInfo.fileSize > 1024)
                        {
                            sizeStr =
                              fmt::format("Size: {:.2f} KB",
                                          static_cast<double>(info.fileInfo.fileSize) / 1024.0);
                        }
                        else
                        {
                            sizeStr = fmt::format("Size: {} bytes", info.fileInfo.fileSize);
                        }
                        ImGui::TextUnformatted(sizeStr.c_str());

                        // Display the timestamp in human-readable format
                        std::string timeStr = formatTimeForHuman(info.timestamp);
                        ImGui::TextUnformatted(fmt::format("Opened: {}", timeStr).c_str());

                        ImGui::Separator();

                        // 3MF metadata
                        if (!info.fileInfo.metadata.empty())
                        {
                            ImGui::TextUnformatted("3MF Metadata:");
                            for (const auto & item : info.fileInfo.metadata)
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
                const char * fileName = info.fileName.c_str();
                ImVec2 textSize = ImGui::CalcTextSize(fileName);

                // Position text below thumbnail
                float textY = itemPos.y + m_thumbnailSize + 15.0f;

                // Draw filename with ellipsis if too long
                if (textSize.x > cellWidth - 10)
                {
                    // Truncate filename if too long
                    std::string truncatedName = info.fileName;
                    if (truncatedName.length() > 15)
                    {
                        truncatedName = truncatedName.substr(0, 12) + "...";
                    }
                    textSize = ImGui::CalcTextSize(truncatedName.c_str());
                    ImGui::SetCursorPos(ImVec2(itemPos.x + (cellWidth - textSize.x) * 0.5f, textY));
                    ImGui::TextUnformatted(truncatedName.c_str());
                }
                else
                {
                    ImGui::SetCursorPos(ImVec2(itemPos.x + (cellWidth - textSize.x) * 0.5f, textY));
                    ImGui::TextUnformatted(fileName);
                }

                // Format timestamp in a human-readable format
                std::string timeStr = formatTimeForHuman(info.timestamp);

                textSize = ImGui::CalcTextSize(timeStr.c_str());
                ImGui::SetCursorPos(
                  ImVec2(itemPos.x + (cellWidth - textSize.x) * 0.5f, ImGui::GetCursorPosY()));
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", timeStr.c_str());

                ImGui::EndGroup();
                ImGui::PopID();

                itemIdx++;
            }

            ImGui::PopStyleVar(2);
        }

        ImGui::PopStyleVar();
    }

    void WelcomeScreen::renderRestoreBackupTab(float availableWidth)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8));

        if (!m_backupManager)
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Backup manager not available");
            ImGui::PopStyleVar();
            return;
        }

        auto backups = m_backupManager->getAvailableBackups();

        if (backups.empty())
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No backup files found");
        }
        else
        {
            // Show backup files in a list
            for (const auto & backup : backups)
            {
                // Format the timestamp in a human-readable format
                auto timeT = std::chrono::system_clock::to_time_t(backup.timestamp);
                std::string timeStr = formatTimeForHuman(timeT);

                // Create display text with session information
                std::string sessionText =
                  backup.isFromPreviousSession ? "Previous Session" : "Current Session";
                std::string statusIcon =
                  backup.isFromPreviousSession ? ICON_FA_EXCLAMATION_TRIANGLE : ICON_FA_CLOCK;

                // Use a button that looks like a selectable
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      backup.isFromPreviousSession
                                        ? ImVec4(0.6f, 0.4f, 0.2f, 0.3f)
                                        : // Orange tint for previous session
                                        ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                      backup.isFromPreviousSession
                                        ? ImVec4(0.7f, 0.5f, 0.3f, 0.5f)
                                        : ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                      backup.isFromPreviousSession
                                        ? ImVec4(0.8f, 0.6f, 0.4f, 0.7f)
                                        : ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

                const std::string displayText = fmt::format("{} {}\n{} | {} | {}",
                                                            statusIcon,
                                                            backup.originalFileName,
                                                            sessionText,
                                                            timeStr,
                                                            backup.filePath.filename().string());

                if (ImGui::Button(displayText.c_str(),
                                  ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 2.5f)))
                {
                    if (m_restoreBackupCallback)
                    {
                        m_restoreBackupCallback(backup.filePath);
                        m_isVisible = false;
                    }
                }

                // Add tooltip with more details
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(
                      fmt::format("Backup Path: {}", backup.filePath.string()).c_str());
                    ImGui::TextUnformatted(
                      fmt::format("Original File: {}", backup.originalFileName).c_str());
                    ImGui::TextUnformatted(fmt::format("Created: {}", timeStr).c_str());
                    ImGui::TextUnformatted(fmt::format("Session: {}", sessionText).c_str());

                    // File size if available
                    try
                    {
                        if (std::filesystem::exists(backup.filePath))
                        {
                            auto fileSize = std::filesystem::file_size(backup.filePath);
                            if (fileSize > 1024 * 1024)
                            {
                                ImGui::TextUnformatted(
                                  fmt::format("Size: {:.2f} MB",
                                              static_cast<double>(fileSize) / (1024.0 * 1024.0))
                                    .c_str());
                            }
                            else if (fileSize > 1024)
                            {
                                ImGui::TextUnformatted(
                                  fmt::format("Size: {:.2f} KB",
                                              static_cast<double>(fileSize) / 1024.0)
                                    .c_str());
                            }
                            else
                            {
                                ImGui::TextUnformatted(
                                  fmt::format("Size: {} bytes", fileSize).c_str());
                            }
                        }
                    }
                    catch (const std::exception &)
                    {
                        // Ignore file size errors
                    }

                    ImGui::EndTooltip();
                }

                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
                ImGui::Separator();
            }
        }

        ImGui::PopStyleVar();
    }

    void WelcomeScreen::renderExamplesTab(float availableWidth)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8));

        if (m_exampleFiles.empty())
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No example files found");
            if (!m_examplesDirectory.empty())
            {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                                   "Looking in: %s",
                                   m_examplesDirectory.string().c_str());
            }
        }
        else if (!m_thumbnailExtractor)
        {
            // If we don't have a thumbnail extractor, show a simple list of files
            for (const auto & [filePath, timestamp] : m_exampleFiles)
            {
                // Use a button that looks like a selectable
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                      ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                      ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

                const std::string fileName = filePath.filename().string();
                const std::string folderName = filePath.parent_path().filename().string();
                const std::string displayText = fmt::format("{}\n{}", fileName, folderName);

                if (ImGui::Button(displayText.c_str(),
                                  ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 2.5f)))
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
            if (m_examplesNeedRefresh && m_thumbnailExtractor)
            {
                updateExampleThumbnailInfos();
            }

            // Calculate grid layout based on available width
            float availWidth = ImGui::GetContentRegionAvail().x - 20.0f; // 20px padding
            float cellWidth = m_thumbnailSize + 20;
            float cellHeight = m_thumbnailSize + 60;

            // Calculate number of columns based on available width
            int columns = std::max(1, static_cast<int>(std::floor(availWidth / cellWidth)));

            // Adjust cellWidth to evenly distribute space
            cellWidth = (availWidth - (ImGui::GetStyle().ItemSpacing.x * (columns - 1))) / columns;

            // Start the thumbnail grid
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 10));

            // Process one file at a time to ensure thumbnails load incrementally
            bool oneThumbnailLoaded = false;

            int itemIdx = 0;
            for (auto & info : m_exampleThumbnailInfos)
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
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                      ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                                      ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));

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
                    if (info.fileInfo.fileSize > 1024 * 1024)
                    {
                        sizeStr = fmt::format("Size: {:.2f} MB",
                                              static_cast<double>(info.fileInfo.fileSize) /
                                                (1024.0 * 1024.0));
                    }
                    else if (info.fileInfo.fileSize > 1024)
                    {
                        sizeStr = fmt::format("Size: {:.2f} KB",
                                              static_cast<double>(info.fileInfo.fileSize) / 1024.0);
                    }
                    else
                    {
                        sizeStr = fmt::format("Size: {} bytes", info.fileInfo.fileSize);
                    }
                    ImGui::TextUnformatted(sizeStr.c_str());

                    ImGui::Separator();

                    // 3MF metadata
                    if (!info.fileInfo.metadata.empty())
                    {
                        ImGui::TextUnformatted("3MF Metadata:");
                        for (const auto & item : info.fileInfo.metadata)
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
                    ImGui::SetCursorPos(ImVec2(
                      centerX, ImGui::GetCursorPosY() + (m_thumbnailSize - displayHeight) * 0.5f));

                    ImGui::Image(
                      reinterpret_cast<void *>(static_cast<intptr_t>(info.thumbnailTextureId)),
                      ImVec2(displayWidth, displayHeight));
                }
                else
                {
                    // Draw placeholder with example icon
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 0.5f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.7f, 0.6f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.6f, 0.8f, 0.7f));

                    if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_SCHOOL),
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
                        ImGui::TextUnformatted(
                          fmt::format("Path: {}", info.filePath.string()).c_str());

                        // Show file size in appropriate units
                        std::string sizeStr;
                        if (info.fileInfo.fileSize > 1024 * 1024)
                        {
                            sizeStr = fmt::format("Size: {:.2f} MB",
                                                  static_cast<double>(info.fileInfo.fileSize) /
                                                    (1024.0 * 1024.0));
                        }
                        else if (info.fileInfo.fileSize > 1024)
                        {
                            sizeStr =
                              fmt::format("Size: {:.2f} KB",
                                          static_cast<double>(info.fileInfo.fileSize) / 1024.0);
                        }
                        else
                        {
                            sizeStr = fmt::format("Size: {} bytes", info.fileInfo.fileSize);
                        }
                        ImGui::TextUnformatted(sizeStr.c_str());

                        ImGui::Separator();

                        // 3MF metadata
                        if (!info.fileInfo.metadata.empty())
                        {
                            ImGui::TextUnformatted("3MF Metadata:");
                            for (const auto & item : info.fileInfo.metadata)
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

                // File name below thumbnail
                const char * fileName = info.fileName.c_str();
                ImVec2 textSize = ImGui::CalcTextSize(fileName);

                // Position text below thumbnail
                float textY = itemPos.y + m_thumbnailSize + 15.0f;

                // Draw filename with ellipsis if too long
                if (textSize.x > cellWidth - 10)
                {
                    // Truncate filename if too long
                    std::string truncatedName = info.fileName;
                    if (truncatedName.length() > 15)
                    {
                        truncatedName = truncatedName.substr(0, 12) + "...";
                    }
                    textSize = ImGui::CalcTextSize(truncatedName.c_str());
                    ImGui::SetCursorPos(ImVec2(itemPos.x + (cellWidth - textSize.x) * 0.5f, textY));
                    ImGui::TextUnformatted(truncatedName.c_str());
                }
                else
                {
                    ImGui::SetCursorPos(ImVec2(itemPos.x + (cellWidth - textSize.x) * 0.5f, textY));
                    ImGui::TextUnformatted(fileName);
                }

                // Add "Example" label below filename
                const char * exampleLabel = "Example";
                textSize = ImGui::CalcTextSize(exampleLabel);
                ImGui::SetCursorPos(
                  ImVec2(itemPos.x + (cellWidth - textSize.x) * 0.5f, ImGui::GetCursorPosY()));
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", exampleLabel);

                ImGui::EndGroup();
                ImGui::PopID();

                itemIdx++;
            }

            ImGui::PopStyleVar(2);
        }

        ImGui::PopStyleVar();
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
