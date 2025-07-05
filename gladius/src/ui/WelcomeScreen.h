#pragma once

#include "BackupManager.h"
#include "ThreemfThumbnailExtractor.h"
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

// Forward declare ImVec2 from imgui
struct ImVec2;

namespace gladius::ui
{
    /// Tab identifiers for the welcome screen
    enum class WelcomeTab
    {
        RecentFiles,
        RestoreBackup,
        Examples
    };

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
    std::string formatTimeForHuman(std::time_t const & timestamp);

    /**
     * @brief Class responsible for rendering the welcome screen
     *
     * This class displays a welcome screen with options for creating
     * a new project, opening an existing project, and showing recent files.
     */
    class WelcomeScreen
    {
      public:
        /**
         * @brief Construct a new WelcomeScreen object
         */
        WelcomeScreen() = default;

        /**
         * @brief Sets the callback function for opening a file
         *
         * @param callback The function to call when a file should be opened
         */
        void setOpenFileCallback(std::function<void(const std::filesystem::path &)> callback);

        /**
         * @brief Sets the callback function for creating a new model
         *
         * @param callback The function to call when a new model should be created
         */
        void setNewModelCallback(std::function<void()> callback);

        /**
         * @brief Sets the list of recent files
         *
         * @param recentFiles Vector of pairs containing the file path and last modification
         * timestamp
         */
        void setRecentFiles(
          const std::vector<std::pair<std::filesystem::path, std::time_t>> & recentFiles);

        /**
         * @brief Sets the logger for the welcome screen
         *
         * @param logger The event logger
         */
        void setLogger(events::SharedLogger logger);

        /**
         * @brief Renders the welcome screen
         *
         * @return true if the screen was rendered and is still active
         * @return false if the screen was closed or not rendered
         */
        bool render();

        /**
         * @brief Checks if the welcome screen is currently visible
         *
         * @return true if visible
         * @return false if hidden
         */
        bool isVisible() const;

        /**
         * @brief Shows the welcome screen
         */
        void show();

        /**
         * @brief Hides the welcome screen
         */
        void hide();

        /**
         * @brief Sets the backup manager for accessing backup files
         *
         * @param backupManager Pointer to the backup manager
         */
        void setBackupManager(const BackupManager * backupManager);

        /**
         * @brief Sets the callback function for restoring a backup file
         *
         * @param callback The function to call when a backup should be restored
         */
        void setRestoreBackupCallback(std::function<void(const std::filesystem::path &)> callback);

        /**
         * @brief Sets the examples directory path
         *
         * @param examplesPath The path to the directory containing example files
         */
        void setExamplesDirectory(const std::filesystem::path & examplesPath);

      private:
        /// Callback for when a file should be opened
        std::function<void(const std::filesystem::path &)> m_openFileCallback;

        /// Callback for when a new model should be created
        std::function<void()> m_newModelCallback;

        /// List of recent files with their modification timestamps
        std::vector<std::pair<std::filesystem::path, std::time_t>> m_recentFiles;

        /// Flag indicating whether the welcome screen is visible
        bool m_isVisible = true;

        /// Thumbnail size for the recent files grid
        float m_thumbnailSize = 150.0f;

        /// Number of columns in the recent files grid
        int m_columns = 3;

        /// Thumbnail extractor for recent files
        std::unique_ptr<ThreemfThumbnailExtractor> m_thumbnailExtractor;

        /// Logger for error reporting
        events::SharedLogger m_logger;

        /// List of thumbnail info for recent files
        std::vector<ThreemfThumbnailExtractor::ThumbnailInfo> m_thumbnailInfos;

        /// Flag to indicate if thumbnails need to be regenerated
        bool m_needsRefresh = true;

        /// Callback for when a backup should be restored
        std::function<void(const std::filesystem::path &)> m_restoreBackupCallback;

        /// Pointer to backup manager for accessing backup files
        const BackupManager * m_backupManager = nullptr;

        /// Currently active tab
        WelcomeTab m_activeTab = WelcomeTab::RecentFiles;

        /// Flag to indicate if backup tab should be selected by default
        bool m_preferBackupTab = false;

        /// Path to the examples directory
        std::filesystem::path m_examplesDirectory;

        /// List of example files with their modification timestamps
        std::vector<std::pair<std::filesystem::path, std::time_t>> m_exampleFiles;

        /// List of thumbnail info for example files
        std::vector<ThreemfThumbnailExtractor::ThumbnailInfo> m_exampleThumbnailInfos;

        /// Flag to indicate if example thumbnails need to be regenerated
        bool m_examplesNeedRefresh = true;

        /// Update the thumbnail info based on recent files list
        void updateThumbnailInfos();

        /// Render the recent files tab
        void renderRecentFilesTab(float availableWidth);

        /// Render the restore backup tab
        void renderRestoreBackupTab(float availableWidth);

        /// Update active tab based on available backups
        void updateActiveTab();

        /// Scan the examples directory for .3mf files
        void scanExamplesDirectory();

        /// Update the thumbnail info for example files
        void updateExampleThumbnailInfos();

        /// Render the examples tab
        void renderExamplesTab(float availableWidth);

        /// Helper method to render a simple file list without thumbnails
        void renderSimpleFileList(
          const std::vector<std::pair<std::filesystem::path, std::time_t>> & fileList,
          bool showTimestamp);

        /// Helper method to render a thumbnail grid
        void
        renderThumbnailGrid(std::vector<ThreemfThumbnailExtractor::ThumbnailInfo> & thumbnailInfos,
                            float availableWidth,
                            const char * placeholderIcon,
                            bool showTimestamp);

        /// Helper method to render a single thumbnail item
        void renderThumbnailItem(ThreemfThumbnailExtractor::ThumbnailInfo & info,
                                 float cellWidth,
                                 float cellHeight,
                                 const ImVec2 & itemPos,
                                 const char * placeholderIcon,
                                 bool showTimestamp);

        /// Helper method to render file name with truncation
        void renderFileName(const std::string & fileName,
                            float cellWidth,
                            const ImVec2 & itemPos,
                            float textY);

        /// Helper method to format and display file size
        std::string formatFileSize(size_t fileSize);

        /// Helper method to render file tooltip with common information
        void renderFileTooltip(const ThreemfThumbnailExtractor::ThumbnailInfo & info,
                               bool showTimestamp);
    };
}
