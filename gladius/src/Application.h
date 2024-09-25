#pragma once

#include "ui/MainWindow.h"

namespace gladius
{
    class Application
    {
      public:
        Application();
        Application(int argc, char ** argv);
        Application(std::filesystem ::path const & filename);
      private:
        ui::MainWindow m_mainWindow;
    };
}
