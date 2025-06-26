#include "Application.h"
#include "ui/MainWindow.h"

#include <filesystem>
#include <iostream>

namespace gladius
{
    Application::Application()
        : m_configManager()
        , m_mainWindow()
    {
        m_mainWindow.setConfigManager(m_configManager);
        m_mainWindow.setup();
        m_mainWindow.startMainLoop();
    }

    Application::Application(int argc, char ** argv)
        : m_configManager()
        , m_mainWindow()
    {
        m_mainWindow.setConfigManager(m_configManager);
        m_mainWindow.setup();
        
        // the first argument is the executable name
        if (argc >= 2)
        {
            std::filesystem::path filename{argv[1]};

            // because this is a console application, we print to the console
            std::cout << "Opening file: " << filename << std::endl;
            if (std::filesystem::exists(filename))
            {
                m_mainWindow.open(filename);
            }
            else
            {
                std::cout << "File does not exist: " << filename << std::endl;
            }
        }
        else
        {
            std::cout << "No file specified" << std::endl;
        }
        m_mainWindow.startMainLoop();
    }
    
    Application::Application(std::filesystem::path const & filename)
        : m_configManager()
        , m_mainWindow()
    {
        m_mainWindow.setConfigManager(m_configManager);
        m_mainWindow.setup();

        
        if (std::filesystem::exists(filename))
        {
            m_mainWindow.open(filename);
        }
        else
        {
            std::cout << "File does not exist: " << filename << std::endl;
        }
        m_mainWindow.startMainLoop();
    }
}
