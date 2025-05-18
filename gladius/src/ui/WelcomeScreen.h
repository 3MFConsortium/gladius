#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace gladius::ui
{
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
    };
}
