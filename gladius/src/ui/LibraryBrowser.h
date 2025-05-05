#pragma once

#include "../Document.h"
#include "ThreemfFileViewer.h"
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gladius::ui
{
    /**
     * @class LibraryBrowser
     * @brief Shows a tabbed library browser with 3MF files from multiple directories
     *
     * The LibraryBrowser shows each subfolder of the selected root directory as a tab,
     * with each tab showing the 3MF files in that directory using a ThreemfFileViewer.
     */
    class LibraryBrowser
    {
      public:
        /**
         * @brief Constructor
         * @param logger The logger to use for events
         */
        explicit LibraryBrowser(events::SharedLogger logger);

        /**
         * @brief Destructor
         */
        ~LibraryBrowser();

        /**
         * @brief Set the root directory containing subfolders to display
         * @param directory The root directory path
         */
        void setRootDirectory(const std::filesystem::path & directory);

        /**
         * @brief Show or hide the browser
         * @param visible Whether the browser should be visible
         */
        void setVisibility(bool visible);

        /**
         * @brief Check if the browser is visible
         * @return Whether the browser is visible
         */
        [[nodiscard]] bool isVisible() const
        {
            return m_visible;
        }

        /**
         * @brief Render the library browser UI
         * @param doc The current document (for loading selected files)
         */
        void render(SharedDocument doc);

        /**
         * @brief Force a refresh of all directories
         */
        void refreshDirectories();

      private:
        /**
         * @brief Scan for subfolders in the root directory
         */
        void scanSubfolders();

        /**
         * @brief Create ThreemfFileViewer instances for each subfolder
         */
        void createFileBrowsers();

        std::filesystem::path m_rootDirectory;           ///< Root directory to scan for subfolders
        std::vector<std::filesystem::path> m_subfolders; ///< Found subfolders
        bool m_visible = false;                          ///< Whether the browser is visible
        bool m_needsRefresh = true;    ///< Whether the directories need to be rescanned
        events::SharedLogger m_logger; ///< Logger for events
        std::unordered_map<std::string, std::unique_ptr<ThreemfFileViewer>>
          m_fileBrowsers; ///< File browsers for each subfolder
    };
}
