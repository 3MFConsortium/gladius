#pragma once

// use 32 bit indices for imgui

#include <atomic>
#include <filesystem>

#include "../ConfigManager.h"
#include "../Document.h"
#include "AboutDialog.h"
#include "CliExportDialog.h"
#include "GLView.h"
#include "LogView.h"
#include "MeshExportDialog.h"
#include "ModelEditor.h"
#include "Outline.h"
#include "RenderWindow.h"
#include "SliceView.h"
#include "LibraryBrowser.h"

#include <chrono>

namespace ed = ax::NodeEditor;

namespace gladius::ui
{
    class MainWindow
    {
      public:
        MainWindow();

        /**
         * @brief Set the ConfigManager reference
         * @param configManager Reference to the ConfigManager
         */
        void setConfigManager(ConfigManager & configManager)
        {
            m_configManager = &configManager;
        }

        void setup(std::shared_ptr<ComputeCore> core,
                   std::shared_ptr<Document> doc,
                   events::SharedLogger logger);
        void dragParameter(const std::string & label, float * valuePtr, float minVal, float maxVal);
        void renderSettingsDialog();
        void open(const std::filesystem::path & filename);
        void startMainLoop();
        void setup();

      private:
        void render();
        void renderWelcomeScreen();
        void nodeEditor();
        void mainWindowDockingArea();

        void renderWindow();
        void mainMenu();
        void sliceWindow();
        void meshExportDialog();
        void cliExportDialog();
        void showExitPopUp();
        void logViewer();

        void refreshModel();

        void markFileAsChanged();
        void newModel();
        void import();
        void updateContours();
        void close();
        void open();
        void merge();
        void resetEditorState();
        void save();
        void updateModel();
        void saveAs();
        void saveCurrentFunction();
        void importImageStack();

        /**
         * @brief Save rendering settings to configuration
         */
        void saveRenderSettings();

        /**
         * @brief Load rendering settings from configuration
         */
        void loadRenderSettings();

        void onPreviewProgramSwap();

        GLView m_mainView;

        ModelEditor m_modelEditor;

        std::filesystem::path m_modelFileName;
        std::optional<std::filesystem::path> m_currentAssemblyFileName;
        std::shared_ptr<ComputeCore> m_core;
        bool m_fileChanged{false};
        std::atomic<bool> m_dirty{true};
        std::atomic<bool> m_parameterDirty{false};
        std::atomic<bool> m_contoursDirty{false};

        ViewCallBack m_renderCallback;

        bool m_showStyleEditor{false};

        bool m_showMainMenu{false};
        bool m_isSlicePreviewVisible{false};
        bool m_showSaveBeforeExit{false};

        float m_mainMenuPosX{-400.f}; // used for the move in animation

        bool m_moving = false;

        bool m_showAuthoringTools{true};
        MeshExportDialog m_meshExporterDialog;
        CliExportDialog m_cliExportDialog;
        SliceView m_sliceView;
        LogView m_logView;
        RenderWindow m_renderWindow;
        AboutDialog m_about;

        std::shared_ptr<Document> m_doc;
        events::SharedLogger m_logger;


        // Add member variables for LibraryBrowser
        LibraryBrowser m_libraryBrowser;
        bool m_isLibraryBrowserVisible = false;


        double m_maxTimeSliceOptimization_s = 60.f;

        bool m_initialized = false;

        bool m_showSettings = false;

        size_t m_lastEventCount{};

        std::chrono::time_point<std::chrono::steady_clock> m_lastUpateTime;

        Outline m_outline;

        float m_uiScale = 1.f;

        ConfigManager * m_configManager = nullptr; // Pointer to the Application's ConfigManager
    };
}
