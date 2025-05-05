#include "LibraryBrowser.h"
#include "../IconFontCppHeaders/IconsFontAwesome5.h"
#include "FileChooser.h"
#include "imgui.h"
#include <algorithm>
#include <fmt/format.h>

namespace gladius::ui
{
    LibraryBrowser::LibraryBrowser(events::SharedLogger logger)
        : m_logger(std::move(logger))
    {
    }

    LibraryBrowser::~LibraryBrowser() = default;

    void LibraryBrowser::setRootDirectory(const std::filesystem::path & directory)
    {
        if (m_rootDirectory != directory)
        {
            m_rootDirectory = directory;
            m_needsRefresh = true;
        }
    }

    void LibraryBrowser::setVisibility(bool visible)
    {
        m_visible = visible;
    }

    void LibraryBrowser::refreshDirectories()
    {
        m_needsRefresh = true;
    }

    void LibraryBrowser::scanSubfolders()
    {
        if (!m_needsRefresh)
        {
            return;
        }

        m_subfolders.clear();
        m_fileBrowsers.clear();

        // Add the root directory itself as the first "subfolder"
        m_subfolders.push_back(m_rootDirectory);

        if (!std::filesystem::exists(m_rootDirectory) ||
            !std::filesystem::is_directory(m_rootDirectory))
        {
            return;
        }

        try
        {
            for (const auto & entry : std::filesystem::directory_iterator(m_rootDirectory))
            {
                if (entry.is_directory())
                {
                    m_subfolders.push_back(entry.path());
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

        createFileBrowsers();
        m_needsRefresh = false;
    }

    void LibraryBrowser::createFileBrowsers()
    {
        for (const auto & subfolder : m_subfolders)
        {
            // Use folder name as key
            std::string folderName =
              subfolder == m_rootDirectory ? "Root" : subfolder.filename().string();

            // Create file browser for this subfolder if it doesn't exist
            if (m_fileBrowsers.find(folderName) == m_fileBrowsers.end())
            {
                auto browser = std::make_unique<ThreemfFileViewer>(m_logger);
                browser->setDirectory(subfolder);
                m_fileBrowsers[folderName] = std::move(browser);
            }
            else
            {
                // Update directory if it exists but might have changed
                m_fileBrowsers[folderName]->setDirectory(subfolder);
                m_fileBrowsers[folderName]->refreshDirectory();
            }
        }
    }

    void LibraryBrowser::render(SharedDocument doc)
    {
        if (!m_visible)
        {
            return;
        }

        // Scan the directories if needed
        scanSubfolders();

        // Configure window
        ImGuiWindowFlags windowFlags = 0;
        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("3MF Library Browser", &m_visible, windowFlags))
        {
            // Directory selection and refresh button
            ImGui::Text("Library Directory: %s", m_rootDirectory.string().c_str());
            ImGui::SameLine();

            // Button with folder icon for selecting directory
            if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_FOLDER)))
            {
                auto selectedDir = queryDirectory(m_rootDirectory);
                if (selectedDir)
                {
                    setRootDirectory(selectedDir.value());
                }
            }

            ImGui::SameLine();

            // Button with refresh icon
            if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_SYNC_ALT)))
            {
                refreshDirectories();
            }

            ImGui::Separator();

            // Skip tab bar if no subfolders or only the root
            if (m_subfolders.empty() ||
                (m_subfolders.size() == 1 && m_subfolders[0] == m_rootDirectory))
            {
                if (m_fileBrowsers.find("Root") != m_fileBrowsers.end())
                {
                    m_fileBrowsers["Root"]->render(doc);
                }
            }
            else
            {
                // Create tabs for each subfolder
                if (ImGui::BeginTabBar("DirectoryTabs", ImGuiTabBarFlags_None))
                {
                    for (const auto & [name, browser] : m_fileBrowsers)
                    {
                        if (ImGui::BeginTabItem(name.c_str()))
                        {
                            browser->render(doc);
                            ImGui::EndTabItem();
                        }
                    }
                    ImGui::EndTabBar();
                }
            }
        }
        ImGui::End();
    }
} // namespace gladius::ui
