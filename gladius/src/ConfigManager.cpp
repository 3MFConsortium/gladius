#include "ConfigManager.h"
#include "exceptions.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sago/platform_folders.h>

namespace gladius
{
    ConfigManager::ConfigManager()
        : m_config(nlohmann::json::object())
    {
        init();
        load();
    }

    ConfigManager::~ConfigManager()
    {
        try
        {
            save();
        }
        catch (std::exception const& e)
        {
            std::cerr << "Failed to save configuration: " << e.what() << std::endl;
        }
    }

    void ConfigManager::init()
    {
        m_configDir = sago::getConfigHome() / std::filesystem::path{"gladius"};
        m_configFilePath = m_configDir / "settings.json";

        if (!std::filesystem::is_directory(m_configDir))
        {
            std::error_code ec;
            std::filesystem::create_directory(m_configDir, ec);
            if (ec)
            {
                throw FileIOError("Failed to create configuration directory: " + m_configDir.string() +
                                  ", error: " + ec.message());
            }
        }
    }

    void ConfigManager::load()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!std::filesystem::exists(m_configFilePath))
        {
            // No config file exists yet, using default empty config
            m_config = nlohmann::json::object();
            return;
        }

        try
        {
            std::ifstream configFile(m_configFilePath);
            if (!configFile.is_open())
            {
                throw FileIOError("Failed to open configuration file: " + m_configFilePath.string());
            }

            configFile >> m_config;
        }
        catch (nlohmann::json::exception const& e)
        {
            std::cerr << "Error loading configuration file: " << e.what() << std::endl;
            // Use default empty config in case of parsing errors
            m_config = nlohmann::json::object();
        }
        catch (std::exception const& e)
        {
            std::cerr << "Error loading configuration file: " << e.what() << std::endl;
            // Use default empty config in case of other errors
            m_config = nlohmann::json::object();
        }
    }

    void ConfigManager::save()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        try
        {
            std::ofstream configFile(m_configFilePath);
            if (!configFile.is_open())
            {
                throw FileIOError("Failed to open configuration file for writing: " + m_configFilePath.string());
            }

            configFile << std::setw(4) << m_config << std::endl;
        }
        catch (std::exception const& e)
        {
            throw FileIOError("Failed to save configuration: " + std::string(e.what()));
        }
    }

    void ConfigManager::reload()
    {
        load();
    }

} // namespace gladius
