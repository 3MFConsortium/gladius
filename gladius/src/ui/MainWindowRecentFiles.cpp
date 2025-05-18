// Implementation of recent files functionality

#include "MainWindow.h"
#include <algorithm>
#include <chrono>

namespace gladius::ui
{
    /**
     * @brief Add a file to the list of recently modified files
     * @param filePath Path to the file that has been modified
     */
    void MainWindow::addToRecentFiles(const std::filesystem::path& filePath)
    {
        // If ConfigManager is not available, we can't store recent files
        if (!m_configManager)
            return;
            
        // Get current time as Unix timestamp
        auto now = std::chrono::system_clock::now();
        std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
        
        // Get existing recent files list
        nlohmann::json recentFiles = m_configManager->getValue<nlohmann::json>("recentFiles", "files", nlohmann::json::array());
        
        // Check if this file is already in the list
        bool fileFound = false;
        std::string filePathStr = filePath.string();
        
        // Create updated list of recent files
        nlohmann::json updatedList = nlohmann::json::array();
        
        // Add the current file with updated timestamp
        nlohmann::json newEntry;
        newEntry["path"] = filePathStr;
        newEntry["timestamp"] = timestamp;
        updatedList.push_back(newEntry);
        
        // Add other existing files, skipping the current one (it's already added)
        size_t count = 1; // Start at 1 since we already added the current file
        const size_t MAX_RECENT_FILES = 10;
        
        for (auto& entry : recentFiles)
        {
            if (count >= MAX_RECENT_FILES)
                break;
                
            if (entry.is_object() && entry.contains("path") && entry["path"].is_string())
            {
                std::string path = entry["path"].get<std::string>();
                
                // Skip the current file (we already added it with a new timestamp)
                if (path != filePathStr)
                {
                    updatedList.push_back(entry);
                    count++;
                }
            }
        }
        
        // Store updated list back to config
        m_configManager->setValue("recentFiles", "files", updatedList);
        
        // Save the configuration to disk
        m_configManager->save();
    }
    
    /**
     * @brief Get the list of recently modified files
     * @param maxCount Maximum number of files to return
     * @return List of pairs containing file paths and timestamps
     */
    std::vector<std::pair<std::filesystem::path, std::time_t>> MainWindow::getRecentFiles(size_t maxCount) const
    {
        std::vector<std::pair<std::filesystem::path, std::time_t>> result;
        
        if (!m_configManager)
            return result;
            
        // Get recent files list from config
        nlohmann::json recentFiles = m_configManager->getValue<nlohmann::json>("recentFiles", "files", nlohmann::json::array());
        
        // Process each entry
        for (auto& entry : recentFiles)
        {
            if (result.size() >= maxCount)
                break;
                
            if (entry.is_object() && 
                entry.contains("path") && entry["path"].is_string() &&
                entry.contains("timestamp") && entry["timestamp"].is_number())
            {
                std::string path = entry["path"].get<std::string>();
                std::time_t timestamp = entry["timestamp"].get<std::time_t>();
                
                // Only add if the file still exists
                if (std::filesystem::exists(path))
                {
                    result.emplace_back(std::filesystem::path(path), timestamp);
                }
            }
        }
        
        return result;
    }
}
