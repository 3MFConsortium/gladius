#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace gladius
{
    /**
     * @brief Configuration manager class for handling application settings
     *
     * Provides a centralized way to manage application configuration settings.
     * Settings are stored in a JSON file and can be accessed through this class.
     */
    class ConfigManager
    {
      public:
        /**
         * @brief Construct a new ConfigManager object
         */
        ConfigManager();

        /**
         * @brief Destroy the ConfigManager object
         */
        ~ConfigManager();

        /**
         * @brief Get a value from the configuration
         * @tparam T Type of the value to retrieve
         * @param section Section name in the configuration
         * @param key Key name within the section
         * @param defaultValue Default value to return if the key doesn't exist
         * @return The value from the configuration or the default value
         */
        template <typename T>
        T getValue(std::string const & section,
                   std::string const & key,
                   T const & defaultValue) const;

        /**
         * @brief Set a value in the configuration
         * @tparam T Type of the value to set
         * @param section Section name in the configuration
         * @param key Key name within the section
         * @param value Value to set
         */
        template <typename T>
        void setValue(std::string const & section, std::string const & key, T const & value);

        /**
         * @brief Save the current configuration to the file
         */
        void save();

        /**
         * @brief Reload the configuration from the file
         */
        void reload();

        /**
         * @brief Get the path to the configuration directory
         * @return Path to the configuration directory
         */
        std::filesystem::path const & getConfigDir() const
        {
            return m_configDir;
        }

      private:
        /**
         * @brief Initialize the configuration
         * Creates the configuration directory if it doesn't exist
         */
        void init();

        /**
         * @brief Load the configuration from the file
         */
        void load();

        std::filesystem::path m_configDir;
        std::filesystem::path m_configFilePath;
        nlohmann::json m_config;
        mutable std::mutex m_mutex;

        // Disable copy and move
        ConfigManager(ConfigManager const &) = delete;
        ConfigManager & operator=(ConfigManager const &) = delete;
        ConfigManager(ConfigManager &&) = delete;
        ConfigManager & operator=(ConfigManager &&) = delete;
    };

    // Template implementations

    template <typename T>
    T ConfigManager::getValue(std::string const & section,
                              std::string const & key,
                              T const & defaultValue) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_config.contains(section) && m_config[section].contains(key))
        {
            try
            {
                return m_config[section][key].get<T>();
            }
            catch (...)
            {
                // In case of type mismatch, return default
                return defaultValue;
            }
        }

        return defaultValue;
    }

    template <typename T>
    void
    ConfigManager::setValue(std::string const & section, std::string const & key, T const & value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_config[section][key] = value;
    }

} // namespace gladius
