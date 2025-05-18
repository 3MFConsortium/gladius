#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include "ThreemfThumbnailExtractor.h"

namespace gladius::ui
{
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
    std::string formatTimeForHuman(std::time_t const& timestamp);

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
        void setOpenFileCallback(std::function<void(const std::filesystem::path&)> callback);
        
        /**
         * @brief Sets the callback function for creating a new model
         * 
         * @param callback The function to call when a new model should be created
         */
        void setNewModelCallback(std::function<void()> callback);
        
        /**
         * @brief Sets the callback function for showing the library browser
         * 
         * @param callback The function to call when the library browser should be displayed
         */
        void setShowLibraryCallback(std::function<void()> callback);
        
        /**
         * @brief Sets the list of recent files
         * 
         * @param recentFiles Vector of pairs containing the file path and last modification timestamp
         */
        void setRecentFiles(const std::vector<std::pair<std::filesystem::path, std::time_t>>& recentFiles);
        
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
        
    private:
        /// Callback for when a file should be opened
        std::function<void(const std::filesystem::path&)> m_openFileCallback;
        
        /// Callback for when a new model should be created
        std::function<void()> m_newModelCallback;
        
        /// Callback for when the library browser should be shown
        std::function<void()> m_showLibraryCallback;
        
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
        
        /// Update the thumbnail info based on recent files list
        void updateThumbnailInfos();
    };
}
