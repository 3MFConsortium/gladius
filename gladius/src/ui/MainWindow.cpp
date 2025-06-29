#include "MainWindow.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fmt/format.h>
#include <iostream>

#ifdef _WIN32
#include <shellapi.h>
#endif

#include "../CliReader.h"
#include "../CliWriter.h"
#include "../EventLogger.h"
#include "../IconFontCppHeaders/IconsFontAwesome5.h"
#include "../TimeMeasurement.h"
#include "../io/MeshExporter.h"
#include "AboutDialog.h"
#include "FileChooser.h"
#include "FileSystemUtils.h"
#include "GLView.h"
#include "LibraryBrowser.h"
#include "Profiling.h"
#include "SvgWriter.h"
#include "compute/ComputeCore.h"
#include "exceptions.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "io/3mf/ImageStackCreator.h"
#include "io/3mf/Writer3mf.h"
#include "io/ImageStackExporter.h"
#include <nodes/ToCommandStreamVisitor.h>

namespace gladius::ui
{
    using namespace std;

    bool bigMenuItem(const char * label)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
        const bool result = ImGui::Button(label);
        ImGui::PopStyleColor();
        return result;
    }

    MainWindow::MainWindow()
        : m_logger(std::make_shared<events::Logger>())
        , m_shortcutSettingsDialog(nullptr) // Initialize with nullptr, we'll set it later
    {
        m_mainView.setRequestCloseCallBack([&]() { close(); });
    }

    void MainWindow::setup(std::shared_ptr<ComputeCore> core,
                           std::shared_ptr<Document> doc,
                           events::SharedLogger logger)
    {
        m_core = std::move(core);
        m_doc = std::move(doc);
        m_logger = std::move(logger);
        m_outline.setDocument(m_doc);

        // Set UI mode to true since we're using the MainWindow (UI interface)
        m_doc->setUiMode(true);

        m_modelEditor.setDocument(m_doc);
        // Set the library root directory
        m_modelEditor.setLibraryRootDirectory(getAppDir() / "library");

        using namespace gladius;

        // Initialize keyboard shortcuts
        initializeShortcuts();

        m_renderWindow.initialize(m_core.get(), &m_mainView, m_shortcutManager, m_configManager);
        LOG_LOCATION
        m_core->getPreviewRenderProgram()->setOnProgramSwapCallBack([&]()
                                                                    { onPreviewProgramSwap(); });

        m_core->getOptimzedRenderProgram()->setOnProgramSwapCallBack([&]()
                                                                     { onPreviewProgramSwap(); });

        m_dirty = true;

        m_renderCallback = [&]() { updateModel(); };

        m_mainView.setRenderCallback(m_renderCallback);

        m_mainView.clearViewCallback();
        m_mainView.addViewCallBack([&]() { render(); });

        m_mainView.setFileDropCallback([&](std::filesystem::path const & path) { open(path); });

        // Set up welcome screen callbacks
        m_welcomeScreen.setNewModelCallback(
          [this]()
          {
              newModel();
              m_welcomeScreen.hide();
          });

        m_welcomeScreen.setOpenFileCallback(
          [this](const std::filesystem::path & path)
          {
              if (path.empty())
              {
                  open();
              }
              else
              {
                  open(path);
              }
              m_welcomeScreen.hide();
          });

        // Set the logger for the welcome screen
        m_welcomeScreen.setLogger(m_logger);

        // Set backup manager for the welcome screen
        m_welcomeScreen.setBackupManager(&m_doc->getBackupManager());

        // Set backup restore callback
        m_welcomeScreen.setRestoreBackupCallback(
          [this](const std::filesystem::path & backupPath)
          {
              open(backupPath);
              m_welcomeScreen.hide();
          });

        // Set recent files
        m_welcomeScreen.setRecentFiles(getRecentFiles(100));

        nodeEditor();
        newModel();
        loadRenderSettings();
    }

    void MainWindow::dragParameter(const std::string & label,
                                   float * valuePtr,
                                   float minVal,
                                   float maxVal)
    {
        const bool changed = ImGui::DragFloat(label.c_str(), valuePtr, 0.001f, minVal, maxVal);
        m_contoursDirty = changed || m_contoursDirty;
        m_parameterDirty = changed || m_parameterDirty;
    }

    void MainWindow::renderSettingsDialog()
    {
        ImGui::Begin("Settings");

        if (ImGui::CollapsingHeader("Rendering"))
        {
            // Add save/load buttons for settings if ConfigManager is available
            if (m_configManager)
            {
                if (ImGui::Button("Save Settings"))
                {
                    saveRenderSettings();
                }
                ImGui::SameLine();
                if (ImGui::Button("Load Settings"))
                {
                    loadRenderSettings();
                    refreshModel();
                }
                ImGui::Separator();
            }

            ImGui::SliderFloat("Ray marching tolerance",
                               &m_core->getResourceContext()->getRenderingSettings().quality,
                               0.1f,
                               20.0f);

            bool enableSdfRendering =
              m_core->getPreviewRenderProgram()->isSdfVisualizationEnabled();
            if (ImGui::Checkbox("Show Distance field", &enableSdfRendering))
            {
                m_core->getPreviewRenderProgram()->setSdfVisualizationEnabled(enableSdfRendering);
                refreshModel();
            }
        }

        if (ImGui::CollapsingHeader("Keyboard Shortcuts"))
        {
            if (ImGui::Button("Configure Shortcuts"))
            {
                showShortcutSettings();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset to Defaults") && m_shortcutManager)
            {
                m_shortcutManager->resetAllShortcutsToDefault();
                m_logger->addEvent(
                  {"Keyboard shortcuts reset to defaults", events::Severity::Info});
            }

            ImGui::Text("Use Ctrl+K to open the keyboard shortcuts dialog");
            ImGui::Separator();

            // Show a few common shortcuts
            ImGui::Text("Common Shortcuts:");
            if (m_shortcutManager)
            {
                ImGui::Text("New: %s",
                            m_shortcutManager->getShortcut("file.new").toString().c_str());
                ImGui::Text("Open: %s",
                            m_shortcutManager->getShortcut("file.open").toString().c_str());
                ImGui::Text("Save: %s",
                            m_shortcutManager->getShortcut("file.save").toString().c_str());
                ImGui::Text("Save As: %s",
                            m_shortcutManager->getShortcut("file.saveAs").toString().c_str());
            }
        }

        auto z = m_core->getSliceHeight();
        ImGui::SliderFloat("Slice Position [mm]", &z, -20.f, 300.);

        m_core->setSliceHeight(z);
        bool tmp_m_dirty = m_dirty.load();

        ImGui::Checkbox("m_dirty", &tmp_m_dirty);
        ImGui::Checkbox("m_moving", &m_moving);

        if (ImGui::Button("Show Events"))
        {
            m_logView.show();
        }
        ImGui::End();
    }

    void MainWindow::setup()
    {
        ProfileFunction;
        m_initialized = true;

        auto context = std::make_shared<ComputeContext>(EnableGLOutput::enabled);

        if (!context->isValid())
        {
            throw std::runtime_error(
              "Failed to create OpenCL Context. Did you install proper GPU drivers?");
        }

        m_core =
          std::make_shared<ComputeCore>(context, RequiredCapabilities::OpenGLInterop, m_logger);
        m_doc = std::make_shared<Document>(m_core);

        // Load render settings if ConfigManager is available
        if (m_configManager)
        {
            loadRenderSettings();
        }

        setup(m_core, m_doc, m_logger);
    }

    void MainWindow::render()
    {
        ProfileFunction;
        m_uiScale = ImGui::GetIO().FontGlobalScale * 2.0f;

        // Check if welcome screen is visible first
        bool welcomeScreenVisible = m_welcomeScreen.isVisible();

        bool welcomeScreenHasbeenClosed = !welcomeScreenVisible && m_wasWelcomeScreenVisible;
        if (welcomeScreenHasbeenClosed)
        {
            m_overlayFadeoutActive = true;
            m_mainView.startAnimationMode();
        }
        m_wasWelcomeScreenVisible = welcomeScreenVisible;

        // Check for keyboard shortcuts
        ImGuiIO & io = ImGui::GetIO();
        processShortcuts(ShortcutContext::Global);

        // try to get the compute token
        auto computeToken = m_core->requestComputeToken();
        if (computeToken)
        {
            if (!m_core->getComputeContext()->isValid())
            {
                m_logger->addEvent({"Reinitializing compute context", events::Severity::Info});

                const auto context = std::make_shared<ComputeContext>(EnableGLOutput::enabled);

                if (!context->isValid())
                {
                    m_logger->addEvent(
                      {"Failed to create OpenCL Context. Did you install proper GPU drivers?",
                       events::Severity::FatalError});
                    throw std::runtime_error(
                      "Failed to create OpenCL Context. Did you install proper GPU drivers?");
                }

                m_core->setComputeContext(context);
            }
        }

        try
        {
            // If welcome screen is visible or fadeout is active, create a blocking overlay
            if (welcomeScreenVisible || (m_overlayFadeoutActive && m_overlayOpacity > 0.0f))
            {
                // Get the entire viewport size
                const ImVec2 viewportSize = ImGui::GetIO().DisplaySize;

                // Update overlay opacity if fadeout is active
                if (m_overlayFadeoutActive)
                {
                    // Reduce opacity based on frame time (smooth transition)
                    float const deltaTime = ImGui::GetIO().DeltaTime;
                    m_overlayOpacity -= deltaTime * 1.0f; // Adjust speed by changing multiplier

                    // Clamp to avoid negative values
                    if (m_overlayOpacity <= 0.0f)
                    {
                        m_overlayOpacity = 0.0f;

                        m_welcomeScreen.hide();
                        m_overlayFadeoutActive = false;
                    }

                    // Trigger animation mode to ensure continuous rendering during fadeout
                    m_mainView.startAnimationMode();
                }

                // Create a fullscreen, top-level modal overlay
                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(viewportSize);

                // Special flags to ensure it blocks all input and stays on top
                ImGuiWindowFlags overlayFlags =
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
                  ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus |
                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav;

                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

                // Begin a blocking overlay
                ImGui::Begin("##WelcomeScreenFullOverlay", nullptr, overlayFlags);

                // Draw a fully opaque rect over the entire screen
                ImDrawList * drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(
                  ImVec2(0, 0),
                  viewportSize,
                  ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, m_overlayOpacity)));

                ImGui::End();
                ImGui::PopStyleVar();
            }

            // Only render the normal UI if welcome screen is not visible and fadeout is complete
            if (!welcomeScreenVisible)
            {

                if (m_showStyleEditor)
                {
                    ImGui::Begin("Style Editor", &m_showStyleEditor);
                    ImGui::ShowStyleEditor();
                    ImGui::End();
                }

                if (m_mainView.isViewSettingsVisible())
                {
                    renderSettingsDialog();
                }

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                                    {20.f * m_uiScale, 12.f * m_uiScale});
                if (ImGui::BeginMainMenuBar())
                {
                    if (bigMenuItem(reinterpret_cast<const char *>(ICON_FA_BARS)))
                    {
                        m_showMainMenu = true;
                    }

                    if (m_modelEditor.isVisible())
                    {
                        if (ImGui::Button(
                              reinterpret_cast<const char *>(ICON_FA_PROJECT_DIAGRAM "\tGraph")))
                        {
                            m_modelEditor.setVisibility(false);
                        }
                    }
                    else
                    {
                        if (bigMenuItem(
                              reinterpret_cast<const char *>(ICON_FA_PROJECT_DIAGRAM "\tGraph")))
                        {
                            m_modelEditor.setVisibility(true);
                        }
                    }

                    if (m_renderWindow.isVisible())
                    {
                        if (ImGui::Button(
                              reinterpret_cast<const char *>(ICON_FA_DESKTOP "\tPreview")))
                        {
                            m_renderWindow.hide();
                        }
                    }
                    else
                    {
                        if (bigMenuItem(
                              reinterpret_cast<const char *>(ICON_FA_DESKTOP "\tPreview")))
                        {
                            m_renderWindow.show();
                        }
                    }

                    if (m_isSlicePreviewVisible)
                    {
                        if (ImGui::Button(
                              reinterpret_cast<const char *>(ICON_FA_LAYER_GROUP "\tSlice")))
                        {
                            m_sliceView.hide();
                        }
                    }
                    else
                    {
                        if (bigMenuItem(
                              reinterpret_cast<const char *>(ICON_FA_LAYER_GROUP "\tSlice")))
                        {
                            m_sliceView.show();
                        }
                    }

                    if (m_mainView.isFullScreen())
                    {
                        if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_EXPAND "")))
                        {
                            m_mainView.setFullScreen(false);
                        }
                    }
                    else
                    {
                        if (bigMenuItem(reinterpret_cast<const char *>(ICON_FA_EXPAND "")))
                        {
                            m_mainView.setFullScreen(true);
                        }
                    }

                    // Add keyboard shortcuts button
                    if (bigMenuItem(reinterpret_cast<const char *>(ICON_FA_KEYBOARD "\tShortcuts")))
                    {
                        showShortcutSettings();
                    }

                    if (m_currentAssemblyFileName)
                    {
                        if (m_fileChanged)
                        {
                            ImGui::TextUnformatted(
                              fmt::format("*{}", m_currentAssemblyFileName.value().string())
                                .c_str());
                        }
                        else
                        {
                            ImGui::TextUnformatted(
                              fmt::format("{}", m_currentAssemblyFileName.value().string())
                                .c_str());
                        }
                    }

                    ImGui::EndMainMenuBar();
                }
                ImGui::PopStyleVar();

                mainWindowDockingArea();
                sliceWindow();
                renderWindow();
                meshExportDialog();
                meshExportDialog3mf();
                cliExportDialog();
                mainMenu();
                showExitPopUp();
                showSaveBeforeFileOperationPopUp();

                if (m_shortcutSettingsDialog.isVisible())
                {
                    m_shortcutSettingsDialog.render();
                }
            }

            // Render welcome screen if it's visible
            if (welcomeScreenVisible)
            {
                // Now render the welcome screen on top of the overlay we created earlier
                m_welcomeScreen.render();
            }

            logViewer();
            m_about.render();
            m_renderWindow.updateCamera();

            // Render status bar if welcome screen is not visible
            if (!welcomeScreenVisible)
            {
                renderStatusBar();
            }

            // Library browser is now rendered by the ModelEditor
        }
        catch (OpenCLError & e)
        {
            m_logger->addEvent(
              {fmt::format("Unexpected exception: {}", e.what()), events::Severity::Error});
            m_logView.show();
        }
        catch (std::exception & e)
        {
            m_logger->addEvent(fmt::format("Unexpected exception: {}", e.what()));
            m_logView.show();
        }

        // Update event counts for status bar (removed automatic popup)
        m_lastEventCount = m_logger->getErrorCount();
        m_lastWarningCount = m_logger->getWarningCount();
    }

    void MainWindow::refreshModel()
    {
        if (m_doc->refreshModelIfNoCompilationIsRunning())
        {
            // Clear errors and warnings from events list when compilation is successful
            m_logger->clear();

            m_renderWindow.invalidateViewDuetoModelUpdate();
            m_modelEditor.markModelAsUpToDate();
        }
        m_renderWindow.invalidateView();
    }

    void MainWindow::nodeEditor()
    {
        m_mainView.addViewCallBack(
          [&]()
          {
              if (!m_modelEditor.isVisible())
              {
                  return;
              }

              // Process model editor shortcuts if visible and hovered
              if (m_modelEditor.isHovered())
              {
                  processShortcuts(ShortcutContext::ModelEditor);
              }

              const auto parameterModifiedByModelEditor = m_modelEditor.showAndEdit();
              m_parameterDirty = parameterModifiedByModelEditor || m_parameterDirty;
              m_dirty = m_parameterDirty || m_dirty;
              m_contoursDirty = m_parameterDirty || m_contoursDirty;
              bool const modelWasModified = m_modelEditor.modelWasModified();
              bool const compileRequested = m_modelEditor.isCompileRequested();

              if (modelWasModified || parameterModifiedByModelEditor)
              {
                  try
                  {
                      m_doc->getAssembly()->updateInputsAndOutputs();
                      m_doc->updateParameterRegistration();
                      markFileAsChanged();
                  }
                  catch (const std::exception & e)
                  {
                      m_logger->addEvent({fmt::format("Error updating model: {}", e.what()),
                                          events::Severity::Error});
                  }
              }

              if (compileRequested)
              {
                  refreshModel();
              }

              // Mark model as up to date after compilation check
              if (modelWasModified || parameterModifiedByModelEditor)
              {
                  m_modelEditor.markModelAsUpToDate();
              }
          });
    }

    void MainWindow::mainWindowDockingArea()
    {
        ImGuiWindowFlags window_flags =
          ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing |
          ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
          ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar;
#ifdef IMGUI_HAS_DOCK
        window_flags |= ImGuiWindowFlags_NoDocking;
#endif
        ImGui::BeginMainMenuBar();
        const auto menuBarHeight = ImGui::GetWindowHeight();
        ImGui::EndMainMenuBar();
        const auto & io = ImGui::GetIO();

        ImGui::SetNextWindowBgAlpha(0.0f);
        auto constexpr silderWidth = 1;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::Begin("MainWindowDockingArea", nullptr, window_flags);

        // Account for status bar height
        float const statusBarHeight = ImGui::GetFrameHeight();
        ImGui::SetWindowSize(ImVec2(io.DisplaySize.x - silderWidth,
                                    io.DisplaySize.y - menuBarHeight - statusBarHeight));

#ifdef IMGUI_HAS_DOCK
        const auto dockspaceID = ImGui::GetID("MainDockingSpace");
        ImGui::DockSpace(dockspaceID,
                         ImVec2(0.0f, 0.0f),
                         ImGuiDockNodeFlags_None |
                           ImGuiDockNodeFlags_PassthruCentralNode /*|ImGuiDockNodeFlags_NoResize*/);

#endif
        ImGui::SetWindowPos("MainWindowDockingArea", {0, menuBarHeight}, ImGuiCond_Always);
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void MainWindow::newModel()
    {
        if (m_fileChanged)
        {
            m_pendingFileOperation = PendingFileOperation::NewModel;
            m_pendingOpenFilename.reset();
            m_showSaveBeforeFileOperation = true;
            return;
        }

        m_doc->newFromTemplate();
        resetEditorState();
        m_renderWindow.centerView();
    }

    void MainWindow::renderWindow()
    {
        // Process render window shortcuts
        if (m_renderWindow.isVisible() && m_renderWindow.isHovered() && m_renderWindow.isFocused())
        {
            processShortcuts(ShortcutContext::RenderWindow);
        }

        m_renderWindow.renderWindow();
    }

    void MainWindow::mainMenu()
    {
        if (!m_showMainMenu)
        {
            return;
        }

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav;
#ifdef IMGUI_HAS_DOCK
        window_flags |= ImGuiWindowFlags_NoDocking;
#endif
        ImGui::BeginMainMenuBar();
        const auto menuBarHeight = ImGui::GetWindowHeight();
        ImGui::EndMainMenuBar();
        auto & io = ImGui::GetIO();
        const auto menuWidth = 400.f * m_uiScale;
        auto closeMenu = [&]()
        {
            m_showMainMenu = false;
            m_mainMenuPosX = -menuWidth;
        };

        ImGui::SetNextWindowBgAlpha(0.9f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {20 * m_uiScale, 20 * m_uiScale});

        ImGui::Begin("Menu", &m_showMainMenu, window_flags);

        ImGui::SetWindowSize(ImVec2(menuWidth, io.DisplaySize.y - menuBarHeight));
        if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_FILE "\tNew")))
        {
            closeMenu();
            newModel();
        }
        if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_FOLDER_OPEN "\tOpen")))
        {
            closeMenu();
            open();
        }

        if (m_showAuthoringTools)
        {
            if (ImGui::MenuItem(
                  reinterpret_cast<const char *>(ICON_FA_FOLDER_OPEN "\tImport functions")))
            {
                closeMenu();
                merge();
            }

            if (ImGui::MenuItem(
                  reinterpret_cast<const char *>(ICON_FA_FOLDER_OPEN "\tImport Image Stack")))
            {
                closeMenu();
                importImageStack();
            }
            if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_SAVE "\tSave As")))
            {
                closeMenu();
                saveAs();
            }

            if (ImGui::MenuItem(
                  reinterpret_cast<const char *>(ICON_FA_SAVE "\tSave Current Function As")))
            {
                closeMenu();
                saveCurrentFunction();
            }

            if (m_currentAssemblyFileName)
            {
                if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_SAVE "\tSave")))
                {
                    closeMenu();
                    save();
                }
            }
        }

        if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_SCHOOL "\tExamples")))
        {
            closeMenu();
            const auto filename = queryLoadFilename({{"*.3mf"}}, getAppDir() / "examples/");
            if (filename.has_value())
            {
                open(filename.value());
            }
        }

        CliWriter writer;

        ImGui::Separator();
        ImGui::TextUnformatted("Export");
        {
            if (ImGui::MenuItem(reinterpret_cast<const char *>("\t" ICON_FA_MINUS
                                                               "\tExport current layer as CLI")))
            {
                closeMenu();
                const auto filename = querySaveFilename({"*.cli"});
                if (filename.has_value())
                {
                    writer.saveCurrentLayer(filename.value(), *m_core);
#ifdef WIN32
                    ShellExecuteW(
                      nullptr, L"open", writer.getFilename().c_str(), nullptr, nullptr, SW_SHOW);
#endif
                }
            }

            if (ImGui::MenuItem(reinterpret_cast<const char *>("\t" ICON_FA_ALIGN_JUSTIFY
                                                               "\tSliced Geometry as CLI")))
            {
                closeMenu();

                QueriedFilename filename;
                if (m_currentAssemblyFileName.has_value())
                {
                    auto suggestedFilename = m_currentAssemblyFileName.value();
                    suggestedFilename.replace_extension("cli");
                    filename = querySaveFilename({"*.cli"}, suggestedFilename);
                }
                else
                {
                    filename = querySaveFilename({"*.cli"}, "part.cli");
                }
                if (filename.has_value())
                {
                    filename->replace_extension(".cli");
                    m_cliExportDialog.beginExport(filename.value(), *m_core);
                }
            }

            if (ImGui::MenuItem(reinterpret_cast<const char *>("\t" ICON_FA_MINUS
                                                               "\tExport current layer as SVG")))
            {
                closeMenu();
                const auto filename = querySaveFilename({"*.svg"});

                if (filename.has_value())
                {
                    SvgWriter svgWriter;
                    svgWriter.saveCurrentLayer(filename.value(), *m_core);
#ifdef WIN32
                    ShellExecuteW(
                      nullptr, L"open", writer.getFilename().c_str(), nullptr, nullptr, SW_SHOW);
#endif
                }
            }

            if (ImGui::MenuItem(reinterpret_cast<const char *>("\t" ICON_FA_FILE_CODE "\tOpenVDB")))
            {
                closeMenu();
                const auto filename = querySaveFilename({"*.vdb"});
                if (filename.has_value())
                {
                    vdb::MeshExporter exporter;
                    exporter.setQualityLevel(1);
                    exporter.beginExport(filename.value(), *m_core);
                    while (exporter.advanceExport(*m_core))
                    {
                        std::cout << " Processing layer with z = " << m_core->getSliceHeight()
                                  << "\n";
                    }
                    exporter.finalizeExportVdb();
                }
            }

            if (ImGui::MenuItem(reinterpret_cast<const char *>("\t" ICON_FA_FILE_CODE "\tNanoVDB")))
            {
                closeMenu();
                const auto filename = querySaveFilename({"*.nvdb"});
                if (filename.has_value())
                {
                    vdb::MeshExporter exporter;
                    exporter.beginExport(filename.value(), *m_core);
                    while (exporter.advanceExport(*m_core))
                    {
                        std::cout << " Processing layer with z = " << m_core->getSliceHeight()
                                  << "\n";
                    }
                    exporter.finalizeExportNanoVdb();
                }
            }

            if (ImGui::MenuItem(reinterpret_cast<const char *>("\t" ICON_FA_FILE_CODE "\tSTL")))
            {
                closeMenu();
                QueriedFilename filename;
                if (m_currentAssemblyFileName.has_value())
                {
                    auto suggestedFilename = m_currentAssemblyFileName.value();
                    suggestedFilename.replace_extension("stl");
                    filename = querySaveFilename({"*.stl"}, suggestedFilename);
                }
                else
                {
                    filename = querySaveFilename({"*.stl"}, "part.stl");
                }
                if (filename.has_value())
                {
                    filename->replace_extension(".stl");

                    m_meshExporterDialog.beginExport(filename.value(), *m_core);
                }
            }

            if (ImGui::MenuItem(
                  reinterpret_cast<const char *>("\t" ICON_FA_FILE_CODE "\t3MF Mesh")))
            {
                closeMenu();
                QueriedFilename filename;
                if (m_currentAssemblyFileName.has_value())
                {
                    auto suggestedFilename = m_currentAssemblyFileName.value();
                    suggestedFilename.replace_extension("3mf");
                    filename = querySaveFilename({"*.3mf"}, suggestedFilename);
                }
                else
                {
                    filename = querySaveFilename({"*.3mf"}, "part.3mf");
                }
                if (filename.has_value())
                {
                    filename->replace_extension(".3mf");

                    m_meshExporterDialog3mf.beginExport(filename.value(), *m_core, m_doc.get());
                }
            }
        }

        ImGui::Separator();

        // Add Library Browser menu item
        if (ImGui::MenuItem(
              reinterpret_cast<const char *>(ICON_FA_FOLDER_OPEN "\tLibrary Browser")))
        {
            closeMenu();
            m_modelEditor.setLibraryRootDirectory(getAppDir() / "examples");
            m_modelEditor.setLibraryVisibility(true);
            m_isLibraryBrowserVisible = true;
        }

        if (m_showSettings)
        {
            if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_COG "\tSettings")))
            {
                closeMenu();
                m_mainView.setViewSettingsVisible(true);
            }

            if (ImGui::MenuItem(
                  reinterpret_cast<const char *>(ICON_FA_KEYBOARD "\tKeyboard Shortcuts")))
            {
                closeMenu();
                showShortcutSettings();
            }

            if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_LIST "\tShow Log")))
            {
                closeMenu();
                m_logView.show();
            }
        }

        if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_QUESTION "\tAbout Gladius")))
        {
            closeMenu();
            m_about.show();
        }

        ImGui::Separator();
        if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_POWER_OFF "\tExit")))
        {
            closeMenu();
            close();
        }

        // Hide menu when anything else is clicked
        if (!ImGui::IsWindowFocused())
        {
            closeMenu();
        }

        ImGui::End();

        ImGui::PopStyleVar();

        // Animation
        const auto deltaTime = ImGui::GetIO().DeltaTime;
        m_mainMenuPosX -= m_mainMenuPosX * 20.f * deltaTime;
        m_mainMenuPosX = std::min(m_mainMenuPosX, 0.f);
        if (m_mainMenuPosX < 0.f)
        {
            m_mainView.startAnimationMode();
        }
        const auto window_pos = ImVec2(m_mainMenuPosX, menuBarHeight);
        ImGui::SetWindowPos("Menu", window_pos, ImGuiCond_Always);
    }

    void MainWindow::sliceWindow()
    {
        updateContours();
        m_isSlicePreviewVisible = m_sliceView.render(*m_core, m_mainView);

        // Process slice window shortcuts if visible and hovered
        if (m_isSlicePreviewVisible && m_sliceView.isHovered())
        {
            processShortcuts(ShortcutContext::SlicePreview);
        }
    }

    void MainWindow::meshExportDialog()
    {
        if (m_meshExporterDialog.isVisible())
        {
            m_mainView.startAnimationMode();
            m_renderWindow.invalidateView();
        }
        m_meshExporterDialog.render(*m_core);
    }

    void MainWindow::meshExportDialog3mf()
    {
        if (m_meshExporterDialog3mf.isVisible())
        {
            m_mainView.startAnimationMode();
            m_renderWindow.invalidateView();
        }
        m_meshExporterDialog3mf.render(*m_core);
    }

    void MainWindow::cliExportDialog()
    {
        if (m_cliExportDialog.isVisible())
        {
            m_mainView.startAnimationMode();
            m_renderWindow.invalidateView();
        }
        m_cliExportDialog.render(*m_core);
    }

    void MainWindow::import()
    {
        const auto filename = queryLoadFilename({{"*.3mf"}});
        if (filename.has_value())
        {
            throw std::runtime_error("Import not implemented");
            resetEditorState();
        }
    }

    void MainWindow::updateContours()
    {
        if (!m_contoursDirty || !m_isSlicePreviewVisible)
        {
            return;
        }
        m_core->requestContourUpdate({});
        m_contoursDirty = false;
    }

    void MainWindow::markFileAsChanged()
    {
        m_fileChanged = true;
    }

    void MainWindow::open()
    {
        if (m_fileChanged)
        {
            m_pendingFileOperation = PendingFileOperation::OpenFile;
            m_pendingOpenFilename.reset();
            m_showSaveBeforeFileOperation = true;
            return;
        }

        const auto filename = queryLoadFilename({{"*.3mf"}});
        if (filename.has_value())
        {
            open(filename.value());
        }
    }

    void MainWindow::merge()
    {
        const auto filename = queryLoadFilename({{"*.3mf"}});
        if (filename.has_value())
        {
            m_doc->merge(filename.value());
        }
    }

    void MainWindow::resetEditorState()
    {
        m_modelEditor.resetEditorContext();
        m_modelEditor.setDocument(m_doc);
        m_modelEditor.invalidatePrimitiveData();
        m_renderWindow.invalidateView();
        m_dirty = true;
        m_renderCallback();
        m_modelEditor.triggerNodePositionUpdate();
        m_fileChanged = false;
        m_modelEditor.resetUndo();
    }

    void MainWindow::open(const std::filesystem::path & filename)
    {
        if (m_fileChanged)
        {
            m_pendingFileOperation = PendingFileOperation::OpenFile;
            m_pendingOpenFilename = filename;
            m_showSaveBeforeFileOperation = true;
            return;
        }

        m_currentAssemblyFileName = filename;
        m_welcomeScreen.hide();
        m_doc->loadNonBlocking(filename);
        resetEditorState();
        m_renderWindow.centerView();

        // Add to recent files list
        addToRecentFiles(filename);
    }

    void MainWindow::startMainLoop()
    {
        m_mainView.startMainLoop();
    }

    void MainWindow::save()
    {
        if (m_currentAssemblyFileName->empty())
        {
            saveAs();
            return;
        }
        bool writeThumbnail = m_core->isRendererReady();
        m_doc->saveAs(m_currentAssemblyFileName.value(), writeThumbnail);
        m_renderWindow.invalidateViewDuetoModelUpdate();
        m_fileChanged = false;

        // Add to recent files list
        addToRecentFiles(m_currentAssemblyFileName.value());
    }

    void MainWindow::saveAs()
    {
        auto filename =
          querySaveFilename({"*.3mf"}, m_currentAssemblyFileName.value_or(std::filesystem::path{}));
        if (filename.has_value())
        {
            filename->replace_extension(".3mf");
            m_doc->saveAs(filename.value());
            m_renderWindow.invalidateViewDuetoModelUpdate();
            m_fileChanged = false;
            m_currentAssemblyFileName = filename;

            // Add to recent files list
            addToRecentFiles(filename.value());
        }
    }

    void MainWindow::saveCurrentFunction()
    {
        auto function = m_modelEditor.currentModel();
        if (!function)
        {
            return;
        }

        auto filename =
          querySaveFilename({"*.3mf"}, m_currentAssemblyFileName.value_or(std::filesystem::path{}));
        if (filename.has_value())
        {
            filename->replace_extension(".3mf");

            gladius::io::saveFunctionTo3mfFile(filename.value(), *function);
        }
    }

    void MainWindow::importImageStack()
    {
        // query directory
        const auto directory = queryDirectory();
        if (!directory.has_value())
        {
            return;
        }

        io::ImageStackCreator creator;
        creator.importDirectoryAsFunctionFromImage3D(m_doc->get3mfModel(), directory.value());
    }

    void MainWindow::onPreviewProgramSwap()
    {
        m_parameterDirty = true;
        m_contoursDirty = true;
        m_dirty = true;
        m_moving = true;
        m_doc->updateParameter();
        m_renderWindow.invalidateViewDuetoModelUpdate();
        m_renderWindow.updateCamera();
    }

    void MainWindow::close()
    {
        saveRenderSettings();
        if (m_fileChanged)
        {
            m_showSaveBeforeExit = true;
            return;
        }
        exit(EXIT_SUCCESS);
    }

    void MainWindow::showExitPopUp()
    {
        if (!m_showSaveBeforeExit)
        {
            return;
        }

        auto constexpr windowTitle = "Do you want to save before leaving Gladius?";
        if (!ImGui::IsPopupOpen(windowTitle))
        {
            ImGui::OpenPopup(windowTitle);
        }
        const ImGuiWindowFlags windowFlags =
          ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::BeginPopupModal(windowTitle, nullptr, windowFlags))
        {
            ImGui::NewLine();
            ImGui::NewLine();

            if (m_currentAssemblyFileName)
            {
                ImGui::TextUnformatted(
                  fmt::format("{} \nhas changed. \nDo you want to save before leaving?",
                              m_currentAssemblyFileName.value().string())
                    .c_str());

                ImGui::NewLine();
                ImGui::NewLine();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.f, 0.f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 0.f, 0.f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.f, 0.f, 1.f));
                if (ImGui::Button(
                      reinterpret_cast<const char *>(ICON_FA_POWER_OFF "\tLeave without saving")))
                {
                    std::exit(EXIT_SUCCESS);
                }
                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                if (ImGui::Button("Continue working"))
                {
                    m_showSaveBeforeExit = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_SAVE "\tSave")))
                {
                    save();
                    m_showSaveBeforeExit = false;
                    std::exit(EXIT_SUCCESS);
                }
                ImGui::SameLine();
                if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_SAVE "\tSave As")))
                {
                    saveAs();
                    m_showSaveBeforeExit = false;
                    std::exit(EXIT_SUCCESS);
                }
            }
            else
            {
                ImGui::TextUnformatted("The current assembly has not been saved yet. \nDo you want "
                                       "to save before leaving?");

                ImGui::NewLine();
                ImGui::NewLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.f, 0.f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 0.f, 0.f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.f, 0.f, 1.f));
                if (ImGui::Button(
                      reinterpret_cast<const char *>(ICON_FA_POWER_OFF "\tLeave without saving")))
                {
                    std::exit(EXIT_SUCCESS);
                }
                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                if (ImGui::Button("Continue working"))
                {
                    m_showSaveBeforeExit = false;
                }
                ImGui::SameLine();
                if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_SAVE "\tSave As")))
                {
                    saveAs();
                    m_showSaveBeforeExit = false;
                    std::exit(EXIT_SUCCESS);
                }
            }
            ImGui::EndPopup();
        }
    }

    void MainWindow::renderStatusBar()
    {
        if (!m_logger)
        {
            return;
        }

        // Create status bar at the bottom of the main window
        ImGuiViewport const * viewport = ImGui::GetMainViewport();
        ImVec2 const statusBarPos =
          ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - ImGui::GetFrameHeight());
        ImVec2 const statusBarSize = ImVec2(viewport->Size.x, ImGui::GetFrameHeight());

        ImGui::SetNextWindowPos(statusBarPos);
        ImGui::SetNextWindowSize(statusBarSize);

        ImGuiWindowFlags const statusBarFlags =
          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
          ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
          ImGuiWindowFlags_NoNavInputs;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));

        if (ImGui::Begin("##StatusBar", nullptr, statusBarFlags))
        {
            size_t const errorCount = m_logger->getErrorCount();
            size_t const warningCount = m_logger->getWarningCount();

            // Error indicator - clickable to show log view
            if (errorCount > 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 0.2f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.3f, 0.3f, 0.4f));
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.2f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 0.4f));
            }

            std::string const errorText = fmt::format("{} {} {}",
                                                      ICON_FA_EXCLAMATION_TRIANGLE,
                                                      errorCount,
                                                      errorCount == 1 ? "Error" : "Errors");

            if (ImGui::Button(errorText.c_str()))
            {
                m_logView.show();
            }
            ImGui::PopStyleColor(4);

            ImGui::SameLine();

            // Warning indicator - clickable to show log view
            if (warningCount > 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.3f, 0.2f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.7f, 0.3f, 0.4f));
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.2f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 0.4f));
            }

            std::string const warningText = fmt::format("{} {} {}",
                                                        ICON_FA_EXCLAMATION_CIRCLE,
                                                        warningCount,
                                                        warningCount == 1 ? "Warning" : "Warnings");

            if (ImGui::Button(warningText.c_str()))
            {
                m_logView.show();
            }
            ImGui::PopStyleColor(4);
        }
        ImGui::End();

        ImGui::PopStyleVar(3);
    }

    void MainWindow::showSaveBeforeFileOperationPopUp()
    {
        if (!m_showSaveBeforeFileOperation)
        {
            return;
        }

        auto constexpr windowTitle = "Do you want to save before continuing?";
        if (!ImGui::IsPopupOpen(windowTitle))
        {
            ImGui::OpenPopup(windowTitle);
        }
        const ImGuiWindowFlags windowFlags =
          ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::BeginPopupModal(windowTitle, nullptr, windowFlags))
        {
            ImGui::NewLine();
            ImGui::NewLine();

            if (m_currentAssemblyFileName)
            {
                const char * operationText =
                  (m_pendingFileOperation == PendingFileOperation::NewModel)
                    ? "creating a new model"
                    : "opening a file";

                ImGui::TextUnformatted(
                  fmt::format("{} \nhas changed. \nDo you want to save before {}?",
                              m_currentAssemblyFileName.value().string(),
                              operationText)
                    .c_str());

                ImGui::NewLine();
                ImGui::NewLine();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.f, 0.f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 0.f, 0.f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.f, 0.f, 1.f));
                if (ImGui::Button(
                      reinterpret_cast<const char *>(ICON_FA_TIMES "\tContinue without saving")))
                {
                    // Proceed with the pending operation without saving
                    if (m_pendingFileOperation == PendingFileOperation::NewModel)
                    {
                        m_doc->newFromTemplate();
                        resetEditorState();
                        m_modelFileName.clear();
                        m_currentAssemblyFileName.reset();
                        m_renderWindow.centerView();
                    }
                    else if (m_pendingFileOperation == PendingFileOperation::OpenFile)
                    {
                        if (m_pendingOpenFilename.has_value())
                        {
                            // Direct file open (e.g., from recent files)
                            m_currentAssemblyFileName = m_pendingOpenFilename.value();
                            m_welcomeScreen.hide();
                            m_doc->loadNonBlocking(m_pendingOpenFilename.value());
                            resetEditorState();
                            m_renderWindow.centerView();
                            addToRecentFiles(m_pendingOpenFilename.value());
                        }
                        else
                        {
                            // Open file dialog
                            const auto filename = queryLoadFilename({{"*.3mf"}});
                            if (filename.has_value())
                            {
                                m_currentAssemblyFileName = filename.value();
                                m_welcomeScreen.hide();
                                m_doc->loadNonBlocking(filename.value());
                                resetEditorState();
                                m_renderWindow.centerView();
                                addToRecentFiles(filename.value());
                            }
                        }
                    }

                    m_showSaveBeforeFileOperation = false;
                    m_pendingFileOperation = PendingFileOperation::None;
                    m_pendingOpenFilename.reset();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    m_showSaveBeforeFileOperation = false;
                    m_pendingFileOperation = PendingFileOperation::None;
                    m_pendingOpenFilename.reset();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_SAVE "\tSave")))
                {
                    save();

                    // After saving, proceed with the pending operation
                    if (m_pendingFileOperation == PendingFileOperation::NewModel)
                    {
                        m_doc->newFromTemplate();
                        resetEditorState();
                        m_renderWindow.centerView();
                    }
                    else if (m_pendingFileOperation == PendingFileOperation::OpenFile)
                    {
                        if (m_pendingOpenFilename.has_value())
                        {
                            // Direct file open (e.g., from recent files)
                            m_currentAssemblyFileName = m_pendingOpenFilename.value();
                            m_welcomeScreen.hide();
                            m_doc->loadNonBlocking(m_pendingOpenFilename.value());
                            resetEditorState();
                            m_renderWindow.centerView();
                            addToRecentFiles(m_pendingOpenFilename.value());
                        }
                        else
                        {
                            // Open file dialog
                            const auto filename = queryLoadFilename({{"*.3mf"}});
                            if (filename.has_value())
                            {
                                m_currentAssemblyFileName = filename.value();
                                m_welcomeScreen.hide();
                                m_doc->loadNonBlocking(filename.value());
                                resetEditorState();
                                m_renderWindow.centerView();
                                addToRecentFiles(filename.value());
                            }
                        }
                    }

                    m_showSaveBeforeFileOperation = false;
                    m_pendingFileOperation = PendingFileOperation::None;
                    m_pendingOpenFilename.reset();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_SAVE "\tSave As")))
                {
                    saveAs();

                    // After saving, proceed with the pending operation
                    if (m_pendingFileOperation == PendingFileOperation::NewModel)
                    {
                        m_doc->newFromTemplate();
                        resetEditorState();
                        m_renderWindow.centerView();
                    }
                    else if (m_pendingFileOperation == PendingFileOperation::OpenFile)
                    {
                        if (m_pendingOpenFilename.has_value())
                        {
                            // Direct file open (e.g., from recent files)
                            m_currentAssemblyFileName = m_pendingOpenFilename.value();
                            m_welcomeScreen.hide();
                            m_doc->loadNonBlocking(m_pendingOpenFilename.value());
                            resetEditorState();
                            m_renderWindow.centerView();
                            addToRecentFiles(m_pendingOpenFilename.value());
                        }
                        else
                        {
                            // Open file dialog
                            const auto filename = queryLoadFilename({{"*.3mf"}});
                            if (filename.has_value())
                            {
                                m_currentAssemblyFileName = filename.value();
                                m_welcomeScreen.hide();
                                m_doc->loadNonBlocking(filename.value());
                                resetEditorState();
                                m_renderWindow.centerView();
                                addToRecentFiles(filename.value());
                            }
                        }
                    }

                    m_showSaveBeforeFileOperation = false;
                    m_pendingFileOperation = PendingFileOperation::None;
                    m_pendingOpenFilename.reset();
                    ImGui::CloseCurrentPopup();
                }
            }
            else
            {
                const char * operationText =
                  (m_pendingFileOperation == PendingFileOperation::NewModel)
                    ? "creating a new model"
                    : "opening a file";

                ImGui::TextUnformatted(
                  fmt::format(
                    "The current assembly has not been saved yet. \nDo you want to save before {}?",
                    operationText)
                    .c_str());

                ImGui::NewLine();
                ImGui::NewLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.f, 0.f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 0.f, 0.f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.f, 0.f, 1.f));
                if (ImGui::Button(
                      reinterpret_cast<const char *>(ICON_FA_TIMES "\tContinue without saving")))
                {
                    // Proceed with the pending operation without saving
                    if (m_pendingFileOperation == PendingFileOperation::NewModel)
                    {
                        m_doc->newFromTemplate();
                        resetEditorState();
                        m_renderWindow.centerView();
                    }
                    else if (m_pendingFileOperation == PendingFileOperation::OpenFile)
                    {
                        if (m_pendingOpenFilename.has_value())
                        {
                            // Direct file open (e.g., from recent files)
                            m_currentAssemblyFileName = m_pendingOpenFilename.value();
                            m_welcomeScreen.hide();
                            m_doc->loadNonBlocking(m_pendingOpenFilename.value());
                            resetEditorState();
                            m_renderWindow.centerView();
                            addToRecentFiles(m_pendingOpenFilename.value());
                        }
                        else
                        {
                            // Open file dialog
                            const auto filename = queryLoadFilename({{"*.3mf"}});
                            if (filename.has_value())
                            {
                                m_currentAssemblyFileName = filename.value();
                                m_welcomeScreen.hide();
                                m_doc->loadNonBlocking(filename.value());
                                resetEditorState();
                                m_renderWindow.centerView();
                                addToRecentFiles(filename.value());
                            }
                        }
                    }

                    m_showSaveBeforeFileOperation = false;
                    m_pendingFileOperation = PendingFileOperation::None;
                    m_pendingOpenFilename.reset();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    m_showSaveBeforeFileOperation = false;
                    m_pendingFileOperation = PendingFileOperation::None;
                    m_pendingOpenFilename.reset();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_SAVE "\tSave As")))
                {
                    saveAs();

                    // After saving, proceed with the pending operation
                    if (m_pendingFileOperation == PendingFileOperation::NewModel)
                    {
                        m_doc->newFromTemplate();
                        resetEditorState();
                        m_renderWindow.centerView();
                    }
                    else if (m_pendingFileOperation == PendingFileOperation::OpenFile)
                    {
                        if (m_pendingOpenFilename.has_value())
                        {
                            // Direct file open (e.g., from recent files)
                            m_currentAssemblyFileName = m_pendingOpenFilename.value();
                            m_welcomeScreen.hide();
                            m_doc->loadNonBlocking(m_pendingOpenFilename.value());
                            resetEditorState();
                            m_renderWindow.centerView();
                            addToRecentFiles(m_pendingOpenFilename.value());
                        }
                        else
                        {
                            // Open file dialog
                            const auto filename = queryLoadFilename({{"*.3mf"}});
                            if (filename.has_value())
                            {
                                m_currentAssemblyFileName = filename.value();
                                m_welcomeScreen.hide();
                                m_doc->loadNonBlocking(filename.value());
                                resetEditorState();
                                m_renderWindow.centerView();
                                addToRecentFiles(filename.value());
                            }
                        }
                    }

                    m_showSaveBeforeFileOperation = false;
                    m_pendingFileOperation = PendingFileOperation::None;
                    m_pendingOpenFilename.reset();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
    }

    void MainWindow::logViewer()
    {
        if (!m_logger)
        {
            return;
        }
        m_logView.render(*m_logger);
    }

    void MainWindow::updateModel()
    {
        auto const timeSinceLastUpdate = std::chrono::steady_clock::now() - m_lastUpateTime;

        if (timeSinceLastUpdate <
            std::chrono::milliseconds(static_cast<int>(ImGui::GetIO().DeltaTime * 5000.f)))
        {
            return;
        }

        m_lastUpateTime = std::chrono::steady_clock::now();

        if (!(m_dirty || m_contoursDirty) || m_renderWindow.isRenderingInProgress() ||
            !m_core->isRendererReady())
        {
            return;
        }

        if (m_modelEditor.primitveDataNeedsUpdate())
        {
            m_doc->invalidatePrimitiveData();
            m_modelEditor.markPrimitiveDataAsUpToDate();
        }

        if (m_modelEditor.isCompileRequested() && m_core->isRendererReady())
        {
            refreshModel();
        }

        if (m_core->getSlicerProgram()->isCompilationInProgress() ||
            m_modelEditor.isCompileRequested() || !m_core->isRendererReady())
        {
            return;
        }

        if (m_parameterDirty)
        {
            m_doc->updateParameter();
            m_renderWindow.invalidateViewDuetoModelUpdate();
            m_parameterDirty = false;
        }
        updateContours();
    }

    /**
     * @brief Add a file to the list of recently modified files
     * @param filePath Path to the file that has been modified
     */
    void MainWindow::addToRecentFiles(const std::filesystem::path & filePath)
    {
        // If ConfigManager is not available, we can't store recent files
        if (!m_configManager)
            return;

        // Get current time as Unix timestamp
        auto now = std::chrono::system_clock::now();
        std::time_t timestamp = std::chrono::system_clock::to_time_t(now);

        // Get existing recent files list
        nlohmann::json recentFiles = m_configManager->getValue<nlohmann::json>(
          "recentFiles", "files", nlohmann::json::array());

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
        const size_t MAX_RECENT_FILES = 100;

        for (auto & entry : recentFiles)
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
    std::vector<std::pair<std::filesystem::path, std::time_t>>
    MainWindow::getRecentFiles(size_t maxCount) const
    {
        std::vector<std::pair<std::filesystem::path, std::time_t>> result;

        if (!m_configManager)
            return result;

        // Get recent files list from config
        nlohmann::json recentFiles = m_configManager->getValue<nlohmann::json>(
          "recentFiles", "files", nlohmann::json::array());
        // print size of recentFiles
        std::cout << "Size of recentFiles: " << recentFiles.size() << std::endl;
        // Process each entry
        for (auto & entry : recentFiles)
        {
            if (result.size() >= maxCount)
                break;

            if (entry.is_object() && entry.contains("path") && entry["path"].is_string() &&
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

    void MainWindow::initializeShortcuts()
    {
        if (!m_configManager)
        {
            return;
        }

        // Create shortcut manager
        m_shortcutManager = std::make_shared<ShortcutManager>(
          std::shared_ptr<ConfigManager>(m_configManager, [](ConfigManager *) {}));
        m_shortcutSettingsDialog.setShortcutManager(m_shortcutManager);

        // Register global shortcuts
        m_shortcutManager->registerAction("file.new",
                                          "New",
                                          "Create a new model",
                                          ShortcutContext::Global,
                                          ShortcutCombo(ImGuiKey_N, true), // Ctrl+N
                                          [this]() { newModel(); });

        m_shortcutManager->registerAction("file.open",
                                          "Open",
                                          "Open an existing model",
                                          ShortcutContext::Global,
                                          ShortcutCombo(ImGuiKey_O, true), // Ctrl+O
                                          [this]() { open(); });

        m_shortcutManager->registerAction("file.save",
                                          "Save",
                                          "Save the current model",
                                          ShortcutContext::Global,
                                          ShortcutCombo(ImGuiKey_S, true), // Ctrl+S
                                          [this]() { save(); });

        m_shortcutManager->registerAction(
          "file.saveAs",
          "Save As",
          "Save the current model with a new name",
          ShortcutContext::Global,
          ShortcutCombo(ImGuiKey_S, true, false, true), // Ctrl+Shift+S
          [this]() { saveAs(); });

        m_shortcutManager->registerAction(
          "edit.library",
          "Toggle Library Browser",
          "Show or hide the library browser",
          ShortcutContext::Global,
          ShortcutCombo(ImGuiKey_B, true), // Ctrl+B
          [this]()
          {
              m_modelEditor.setLibraryRootDirectory(getAppDir() / "examples");
              m_modelEditor.toggleLibraryVisibility();
              m_isLibraryBrowserVisible = m_modelEditor.isLibraryVisible();
          });

        m_shortcutManager->registerAction("view.resetView",
                                          "Reset View",
                                          "Reset the camera view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_R), // R
                                          [this]() { m_renderWindow.centerView(); });

        // Model editor shortcuts
        m_shortcutManager->registerAction("edit.undo",
                                          "Undo",
                                          "Undo the last action",
                                          ShortcutContext::ModelEditor,
                                          ShortcutCombo(ImGuiKey_Z, true), // Ctrl+Z
                                          [this]()
                                          {
                                              // Handle this in the UI code since undo/redo are
                                              // private
                                              if (ImGui::GetIO().KeyCtrl &&
                                                  ImGui::IsKeyPressed(ImGuiKey_Z, false))
                                              {
                                                  ed::NavigateToContent();
                                              }
                                          });

        m_shortcutManager->registerAction("edit.redo",
                                          "Redo",
                                          "Redo the last undone action",
                                          ShortcutContext::ModelEditor,
                                          ShortcutCombo(ImGuiKey_Y, true), // Ctrl+Y
                                          [this]()
                                          {
                                              // Handle this in the UI code since undo/redo are
                                              // private
                                              if (ImGui::GetIO().KeyCtrl &&
                                                  ImGui::IsKeyPressed(ImGuiKey_Y, false))
                                              {
                                                  ed::NavigateToContent();
                                              }
                                          });

        m_shortcutManager->registerAction("edit.compile",
                                          "Compile Model",
                                          "Compile the current model",
                                          ShortcutContext::ModelEditor,
                                          ShortcutCombo(ImGuiKey_F5), // F5
                                          [this]()
                                          {
                                              // The model editor will handle compilation based on
                                              // the F5 key press We'll let the native ImGui input
                                              // handling take care of this
                                          });

        // Add shortcut for showing settings dialog
        m_shortcutManager->registerAction("view.shortcuts",
                                          "Keyboard Shortcuts",
                                          "Show keyboard shortcuts dialog",
                                          ShortcutContext::Global,
                                          ShortcutCombo(ImGuiKey_K, true), // Ctrl+K
                                          [this]() { showShortcutSettings(); });

        // Model editor shortcuts - Compile implicit function
        m_shortcutManager->registerAction("model.compileImplicit",
                                          "Compile Implicit Function",
                                          "Manually compile the implicit function",
                                          ShortcutContext::ModelEditor,
                                          ShortcutCombo(ImGuiKey_F7), // F7
                                          [this]() { m_modelEditor.requestManualCompile(); });

        // Standard CAD view shortcuts for RenderWindow
        // Based on industry standards (Blender, 3ds Max, Maya, AutoCAD, SolidWorks)

        // Basic view controls
        m_shortcutManager->registerAction(
          "camera.centerView",
          "Center View",
          "Center the camera view on the model",
          ShortcutContext::RenderWindow,
          ShortcutCombo(ImGuiKey_Period), // . (standard in many CAD apps)
          [this]() { m_renderWindow.centerView(); });

        m_shortcutManager->registerAction(
          "camera.togglePermanentCentering",
          "Toggle Permanent Centering",
          "Toggle automatic view centering when model or camera changes",
          ShortcutContext::RenderWindow,
          ShortcutCombo(ImGuiKey_Period, true), // Ctrl+. for permanent centering
          [this]() { m_renderWindow.togglePermanentCentering(); });

        m_shortcutManager->registerAction("camera.frameAll",
                                          "Frame All",
                                          "Frame all objects in view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Home), // Home key
                                          [this]() { m_renderWindow.frameAll(); });

        // Orthographic views (Numpad standard)
        m_shortcutManager->registerAction("camera.frontView",
                                          "Front View",
                                          "Set camera to front view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Keypad1), // Numpad 1
                                          [this]() { m_renderWindow.setFrontView(); });

        m_shortcutManager->registerAction("camera.backView",
                                          "Back View",
                                          "Set camera to back view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Keypad1, true), // Ctrl+Numpad 1
                                          [this]() { m_renderWindow.setBackView(); });

        m_shortcutManager->registerAction("camera.rightView",
                                          "Right View",
                                          "Set camera to right view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Keypad3), // Numpad 3
                                          [this]() { m_renderWindow.setRightView(); });

        m_shortcutManager->registerAction("camera.leftView",
                                          "Left View",
                                          "Set camera to left view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Keypad3, true), // Ctrl+Numpad 3
                                          [this]() { m_renderWindow.setLeftView(); });

        m_shortcutManager->registerAction("camera.topView",
                                          "Top View",
                                          "Set camera to top view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Keypad7), // Numpad 7
                                          [this]() { m_renderWindow.setTopView(); });

        m_shortcutManager->registerAction("camera.bottomView",
                                          "Bottom View",
                                          "Set camera to bottom view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Keypad7, true), // Ctrl+Numpad 7
                                          [this]() { m_renderWindow.setBottomView(); });

        // Isometric and perspective views
        m_shortcutManager->registerAction("camera.isoView",
                                          "Isometric View",
                                          "Set camera to isometric view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Keypad0), // Numpad 0
                                          [this]() { m_renderWindow.setIsometricView(); });

        m_shortcutManager->registerAction("camera.perspectiveToggle",
                                          "Toggle Perspective/Orthographic",
                                          "Toggle between perspective and orthographic projection",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Keypad5), // Numpad 5
                                          [this]() { m_renderWindow.togglePerspective(); });

        // Alternative view shortcuts for keyboards without numpad
        m_shortcutManager->registerAction("camera.frontViewAlt",
                                          "Front View (Alt)",
                                          "Set camera to front view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_1), // 1
                                          [this]() { m_renderWindow.setFrontView(); });

        m_shortcutManager->registerAction("camera.rightViewAlt",
                                          "Right View (Alt)",
                                          "Set camera to right view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_3), // 3
                                          [this]() { m_renderWindow.setRightView(); });

        m_shortcutManager->registerAction("camera.topViewAlt",
                                          "Top View (Alt)",
                                          "Set camera to top view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_7), // 7
                                          [this]() { m_renderWindow.setTopView(); });

        m_shortcutManager->registerAction("camera.backViewAlt",
                                          "Back View (Alt)",
                                          "Set camera to back view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_1, true), // Ctrl+1
                                          [this]() { m_renderWindow.setBackView(); });

        m_shortcutManager->registerAction("camera.leftViewAlt",
                                          "Left View (Alt)",
                                          "Set camera to left view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_3, true), // Ctrl+3
                                          [this]() { m_renderWindow.setLeftView(); });

        m_shortcutManager->registerAction("camera.bottomViewAlt",
                                          "Bottom View (Alt)",
                                          "Set camera to bottom view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_7, true), // Ctrl+7
                                          [this]() { m_renderWindow.setBottomView(); });

        // Camera movement shortcuts
        m_shortcutManager->registerAction("camera.panLeft",
                                          "Pan Left",
                                          "Pan camera to the left",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Keypad4), // Numpad 4
                                          [this]() { m_renderWindow.panLeft(); });

        m_shortcutManager->registerAction("camera.panRight",
                                          "Pan Right",
                                          "Pan camera to the right",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Keypad6), // Numpad 6
                                          [this]() { m_renderWindow.panRight(); });

        m_shortcutManager->registerAction("camera.panUp",
                                          "Pan Up",
                                          "Pan camera up",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Keypad8), // Numpad 8
                                          [this]() { m_renderWindow.panUp(); });

        m_shortcutManager->registerAction("camera.panDown",
                                          "Pan Down",
                                          "Pan camera down",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Keypad2), // Numpad 2
                                          [this]() { m_renderWindow.panDown(); });

        // Rotation shortcuts
        m_shortcutManager->registerAction(
          "camera.rotateLeft",
          "Rotate Left",
          "Rotate camera to the left",
          ShortcutContext::RenderWindow,
          ShortcutCombo(ImGuiKey_Keypad4, false, true), // Shift+Numpad 4
          [this]() { m_renderWindow.rotateLeft(); });

        m_shortcutManager->registerAction(
          "camera.rotateRight",
          "Rotate Right",
          "Rotate camera to the right",
          ShortcutContext::RenderWindow,
          ShortcutCombo(ImGuiKey_Keypad6, false, true), // Shift+Numpad 6
          [this]() { m_renderWindow.rotateRight(); });

        m_shortcutManager->registerAction(
          "camera.rotateUp",
          "Rotate Up",
          "Rotate camera up",
          ShortcutContext::RenderWindow,
          ShortcutCombo(ImGuiKey_Keypad8, false, true), // Shift+Numpad 8
          [this]() { m_renderWindow.rotateUp(); });

        m_shortcutManager->registerAction(
          "camera.rotateDown",
          "Rotate Down",
          "Rotate camera down",
          ShortcutContext::RenderWindow,
          ShortcutCombo(ImGuiKey_Keypad2, false, true), // Shift+Numpad 2
          [this]() { m_renderWindow.rotateDown(); });

        // Zoom shortcuts for RenderWindow (CAD standard)
        m_shortcutManager->registerAction("camera.zoomIn",
                                          "Zoom In",
                                          "Zoom in the camera view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_KeypadAdd), // Numpad +
                                          [this]() { m_renderWindow.zoomIn(); });

        m_shortcutManager->registerAction("camera.zoomOut",
                                          "Zoom Out",
                                          "Zoom out the camera view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_KeypadSubtract), // Numpad -
                                          [this]() { m_renderWindow.zoomOut(); });

        m_shortcutManager->registerAction("camera.zoomInAlt",
                                          "Zoom In (Alt)",
                                          "Zoom in the camera view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Equal, true), // Ctrl+=
                                          [this]() { m_renderWindow.zoomIn(); });

        m_shortcutManager->registerAction("camera.zoomOutAlt",
                                          "Zoom Out (Alt)",
                                          "Zoom out the camera view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Minus, true), // Ctrl+-
                                          [this]() { m_renderWindow.zoomOut(); });

        m_shortcutManager->registerAction("camera.zoomExtents",
                                          "Zoom Extents",
                                          "Zoom to fit all objects in view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_KeypadMultiply), // Numpad *
                                          [this]() { m_renderWindow.zoomExtents(); });

        m_shortcutManager->registerAction("camera.zoomSelected",
                                          "Zoom Selected",
                                          "Zoom to fit selected objects",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_KeypadDivide), // Numpad /
                                          [this]() { m_renderWindow.zoomSelected(); });

        m_shortcutManager->registerAction("camera.resetZoom",
                                          "Reset Zoom",
                                          "Reset the camera zoom level",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_0, true), // Ctrl+0
                                          [this]() { m_renderWindow.resetZoom(); });

        // CAD-specific view shortcuts
        m_shortcutManager->registerAction(
          "camera.previousView",
          "Previous View",
          "Go to previous view",
          ShortcutContext::RenderWindow,
          ShortcutCombo(ImGuiKey_LeftArrow, false, true), // Shift+Left Arrow
          [this]() { m_renderWindow.previousView(); });

        m_shortcutManager->registerAction(
          "camera.nextView",
          "Next View",
          "Go to next view",
          ShortcutContext::RenderWindow,
          ShortcutCombo(ImGuiKey_RightArrow, false, true), // Shift+Right Arrow
          [this]() { m_renderWindow.nextView(); });

        m_shortcutManager->registerAction("camera.saveView",
                                          "Save View",
                                          "Save current view",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_V, true), // Ctrl+V
                                          [this]() { m_renderWindow.saveCurrentView(); });

        m_shortcutManager->registerAction(
          "camera.restoreView",
          "Restore View",
          "Restore saved view",
          ShortcutContext::RenderWindow,
          ShortcutCombo(ImGuiKey_V, true, false, true), // Ctrl+Shift+V
          [this]() { m_renderWindow.restoreSavedView(); });

        // Fly/Walk mode shortcuts
        m_shortcutManager->registerAction("camera.flyMode",
                                          "Toggle Fly Mode",
                                          "Toggle fly/walk camera mode",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_F), // F
                                          [this]() { m_renderWindow.toggleFlyMode(); });

        // Additional professional CAD shortcuts
        m_shortcutManager->registerAction("camera.orbitMode",
                                          "Orbit Mode",
                                          "Enter orbit camera mode",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_O), // O
                                          [this]() { m_renderWindow.setOrbitMode(); });

        m_shortcutManager->registerAction("camera.panMode",
                                          "Pan Mode",
                                          "Enter pan camera mode",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_P), // P
                                          [this]() { m_renderWindow.setPanMode(); });

        m_shortcutManager->registerAction("camera.zoomMode",
                                          "Zoom Mode",
                                          "Enter zoom camera mode",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_Z), // Z
                                          [this]() { m_renderWindow.setZoomMode(); });

        m_shortcutManager->registerAction("camera.resetOrientation",
                                          "Reset Orientation",
                                          "Reset camera orientation only",
                                          ShortcutContext::RenderWindow,
                                          ShortcutCombo(ImGuiKey_R, true), // Ctrl+R
                                          [this]() { m_renderWindow.resetOrientation(); });

        // Model editor node shortcuts
        m_shortcutManager->registerAction("model.autoLayout",
                                          "Auto Layout",
                                          "Automatically arrange nodes in the editor",
                                          ShortcutContext::ModelEditor,
                                          ShortcutCombo(ImGuiKey_L, true), // Ctrl+L
                                          [this]()
                                          {
                                              m_modelEditor.autoLayoutNodes(
                                                200.0f); // Use default distance
                                          });

        m_shortcutManager->registerAction("model.createNode",
                                          "Create Node",
                                          "Open the create node menu",
                                          ShortcutContext::ModelEditor,
                                          ShortcutCombo(ImGuiKey_G, true), // Ctrl+G
                                          [this]() { m_modelEditor.showCreateNodePopup(); });

        // Slice preview shortcuts
        m_shortcutManager->registerAction("sliceview.zoomin",
                                          "Zoom In",
                                          "Zoom in slice view",
                                          ShortcutContext::SlicePreview,
                                          ShortcutCombo(ImGuiKey_Equal, true), // Ctrl+=
                                          [this]()
                                          {
                                              if (m_isSlicePreviewVisible)
                                              {
                                                  m_sliceView.zoomIn();
                                              }
                                          });

        m_shortcutManager->registerAction("sliceview.zoomout",
                                          "Zoom Out",
                                          "Zoom out slice view",
                                          ShortcutContext::SlicePreview,
                                          ShortcutCombo(ImGuiKey_Minus, true), // Ctrl+-
                                          [this]()
                                          {
                                              if (m_isSlicePreviewVisible)
                                              {
                                                  m_sliceView.zoomOut();
                                              }
                                          });

        m_shortcutManager->registerAction("sliceview.reset",
                                          "Reset View",
                                          "Reset the slice view",
                                          ShortcutContext::SlicePreview,
                                          ShortcutCombo(ImGuiKey_R), // R
                                          [this]()
                                          {
                                              if (m_isSlicePreviewVisible)
                                              {
                                                  m_sliceView.resetView();
                                              }
                                          });
    }

    void MainWindow::processShortcuts(ShortcutContext activeContext)
    {
        if (m_shortcutManager)
        {
            m_shortcutManager->processInput(activeContext);
        }
    }

    void MainWindow::showShortcutSettings()
    {
        m_shortcutSettingsDialog.show();
    }

    void MainWindow::saveRenderSettings()
    {
        if (!m_configManager)
        {
            return;
        }

        // Get current rendering settings
        auto & renderSettings = m_core->getResourceContext()->getRenderingSettings();

        // Create JSON object for render settings
        nlohmann::json renderJson;
        renderJson["quality"] = renderSettings.quality;
        renderJson["sdfVisEnabled"] =
          m_core->getPreviewRenderProgram()->isSdfVisualizationEnabled();

        // Save to config
        m_configManager->setValue("rendering", "settings", renderJson);

        // Save shortcuts too
        if (m_shortcutManager)
        {
            m_shortcutManager->saveShortcuts();
        }

        // Write to disk
        m_configManager->save();

        // Log success
        m_logger->addEvent({"Rendering settings saved", events::Severity::Info});
    }

    void MainWindow::loadRenderSettings()
    {
        if (!m_configManager)
        {
            return;
        }

        // Get render settings from config (or use default empty object if not found)
        nlohmann::json renderJson = m_configManager->getValue<nlohmann::json>(
          "rendering", "settings", nlohmann::json::object());

        // Skip if there are no saved settings
        if (renderJson.empty())
        {
            return;
        }

        // Get current rendering settings to update
        auto & renderSettings = m_core->getResourceContext()->getRenderingSettings();

        // Update settings from config
        if (renderJson.contains("quality"))
        {
            renderSettings.quality = renderJson["quality"].get<float>();
        }

        if (renderJson.contains("sdfVisEnabled"))
        {
            m_core->getPreviewRenderProgram()->setSdfVisualizationEnabled(
              renderJson["sdfVisEnabled"].get<bool>());
        }

        // Load shortcuts too (this happens automatically when m_shortcutManager is created)

        // Log success
        m_logger->addEvent({"Rendering settings loaded", events::Severity::Info});
    }
}
