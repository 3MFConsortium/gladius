#pragma once

#include "ui/MainWindow.h"
#include "ConfigManager.h"

namespace gladius
{
    class Application
    {
      public:
        Application();
        Application(int argc, char ** argv);
        Application(std::filesystem ::path const & filename);
        
        /**
         * @brief Get the ConfigManager instance
         * @return Reference to the ConfigManager
         */
        ConfigManager& getConfigManager() { return m_configManager; }
        
        /**
         * @brief Get the ConfigManager instance (const version)
         * @return Const reference to the ConfigManager
         */
        ConfigManager const& getConfigManager() const { return m_configManager; }
        
      private:
        ConfigManager m_configManager;
        ui::MainWindow m_mainWindow;
    };
}
