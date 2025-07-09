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
#include "MeshExportDialog3mf.h"
#include "ModelEditor.h"
#include "Outline.h"
#include "RenderWindow.h"
#include "SliceView.h"
#include "WelcomeScreen.h"

// includes for the shortcut system
#include "ShortcutManager.h"
#include "ShortcutSettingsDialog.h"

#include <chrono>

namespace ed = ax::NodeEditor;

namespace gladius::ui
{
    enum class PendingFileOperation
    {
        None,
        NewModel,
        OpenFile
    };

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

        /**
         * @brief Initialize the shortcut system
         * Registers standard keyboard shortcuts for the application
         */
        void initializeShortcuts();

        /**
         * @brief Process keyboard shortcuts based on the active context
         * @param activeContext The currently active context
         */
        void processShortcuts(ShortcutContext activeContext);

        /**
         * @brief Show the shortcut settings dialog
         */
        void showShortcutSettings();

        /**
         * @brief Show the welcome screen and reset overlay opacity
         */
        void showWelcomeScreen();

      private:
        void render();
        void nodeEditor();
        void mainWindowDockingArea();

        void renderWindow();
        void mainMenu();
        void sliceWindow();
        void meshExportDialog();
        void meshExportDialog3mf();
        void cliExportDialog();
        void showExitPopUp();
        void showSaveBeforeFileOperationPopUp();
        void logViewer();
        void renderStatusBar();

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

        /**
         * @brief Add a file to the list of recently modified files
         * @param filePath Path to the file that has been modified
         */
        void addToRecentFiles(const std::filesystem::path & filePath);

        /**
         * @brief Get the list of recently modified files
         * @param maxCount Maximum number of files to return
         * @return List of pairs containing file paths and timestamps
         */
        std::vector<std::pair<std::filesystem::path, std::time_t>>
        getRecentFiles(size_t maxCount = 100) const;

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
        bool m_showSaveBeforeFileOperation{false};
        PendingFileOperation m_pendingFileOperation{PendingFileOperation::None};
        std::optional<std::filesystem::path> m_pendingOpenFilename;

        float m_mainMenuPosX{-400.f}; // used for the move in animation

        bool m_moving = false;

        bool m_showAuthoringTools{true};
        MeshExportDialog m_meshExporterDialog;
        MeshExportDialog3mf m_meshExporterDialog3mf;
        CliExportDialog m_cliExportDialog;
        SliceView m_sliceView;
        LogView m_logView;
        RenderWindow m_renderWindow;
        AboutDialog m_about;
        WelcomeScreen m_welcomeScreen;

        std::shared_ptr<Document> m_doc;
        events::SharedLogger m_logger;

        // Flag to remember if library browser was visible
        bool m_isLibraryBrowserVisible = false;

        double m_maxTimeSliceOptimization_s = 60.f;

        bool m_initialized = false;

        bool m_showSettings = false;

        size_t m_lastEventCount{};

        size_t m_lastWarningCount{};

        std::chrono::time_point<std::chrono::steady_clock> m_lastUpateTime;

        Outline m_outline;

        float m_uiScale = 1.f;

        /// Opacity value for the welcome screen overlay (0.0-1.0)
        float m_overlayOpacity = 1.0f;

        /// Flag indicating if welcome screen overlay fadeout is in progress
        bool m_overlayFadeoutActive = false;

        ConfigManager * m_configManager = nullptr; // Pointer to the Application's ConfigManager

        bool m_wasWelcomeScreenVisible = false;

        // Shortcut system
        std::shared_ptr<ShortcutManager> m_shortcutManager;
        ShortcutSettingsDialog m_shortcutSettingsDialog;
    };
}
