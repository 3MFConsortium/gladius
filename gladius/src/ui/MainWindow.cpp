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
#include <nodes/ToCommandStreamVisitor.h>
#include "AboutDialog.h"
#include "FileChooser.h"
#include "FileSystemUtils.h"
#include "GLView.h"
#include "Profiling.h"
#include "SvgWriter.h"
#include "compute/ComputeCore.h"
#include "exceptions.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "io/3mf/ImageStackCreator.h"
#include "io/3mf/Writer3mf.h"
#include "io/ImageStackExporter.h"

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
        : m_threemfFileViewer(std::make_shared<events::Logger>())
    {
        m_logger = std::make_shared<events::Logger>();
        m_mainView.addViewCallBack([&]() { renderWelcomeScreen(); });
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
        
        // Set the logger for the ThreemfFileViewer
        m_threemfFileViewer = ThreemfFileViewer(m_logger);

        m_modelEditor.setDocument(m_doc);
        using namespace gladius;

        m_renderWindow.initialize(m_core.get(), &m_mainView);
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
                               &m_core->getResourceContext().getRenderingSettings().quality,
                               0.1f,
                               20.0f);

            dragParameter("Weight dist to neighbor",
                          &m_core->getResourceContext().getRenderingSettings().weightDistToNb,
                          0.f,
                          100.0f);

            dragParameter("Weight midpoint",
                          &m_core->getResourceContext().getRenderingSettings().weightMidPoint,
                          0.f,
                          100.0f);

            dragParameter("Normal offset",
                          &m_core->getResourceContext().getRenderingSettings().normalOffset,
                          0.f,
                          5.0f);

            bool enableSdfRendering =
              m_core->getPreviewRenderProgram()->isSdfVisualizationEnabled();
            if (ImGui::Checkbox("Show Distance field", &enableSdfRendering))
            {
                m_core->getPreviewRenderProgram()->setSdfVisualizationEnabled(enableSdfRendering);
                refreshModel();
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
        
        // Check for keyboard shortcuts
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_B, false)) {
            m_threemfFileViewer.setDirectory(getAppDir() / "examples");
            m_threemfFileViewer.setVisibility(!m_threemfFileViewer.isVisible());
        }
        if (!m_core->getComputeContext().isValid())
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

        try
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

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {20.f * m_uiScale, 12.f * m_uiScale});
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
                    if (ImGui::Button(reinterpret_cast<const char *>(ICON_FA_DESKTOP "\tPreview")))
                    {
                        m_renderWindow.hide();
                    }
                }
                else
                {
                    if (bigMenuItem(reinterpret_cast<const char *>(ICON_FA_DESKTOP "\tPreview")))
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
                    if (bigMenuItem(reinterpret_cast<const char *>(ICON_FA_LAYER_GROUP "\tSlice")))
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
                if (m_currentAssemblyFileName)
                {
                    if (m_fileChanged)
                    {
                        ImGui::TextUnformatted(
                          fmt::format("*{}", m_currentAssemblyFileName.value().string()).c_str());
                    }
                    else
                    {
                        ImGui::TextUnformatted(
                          fmt::format("{}", m_currentAssemblyFileName.value().string()).c_str());
                    }
                }

                ImGui::EndMainMenuBar();
            }
            ImGui::PopStyleVar();

            mainWindowDockingArea();
            sliceWindow();
            renderWindow();
            meshExportDialog();
            cliExportDialog();
            mainMenu();
            showExitPopUp();
            logViewer();
            m_about.render();
            m_renderWindow.updateCamera();
            m_threemfFileViewer.render(m_doc);
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

        auto const errorCount = m_logger->getErrorCount();
        if (errorCount > m_lastEventCount)
        {
            m_logView.show();
        }
        m_lastEventCount = errorCount;
    }

    void MainWindow::renderWelcomeScreen()
    {
        ImGui::TextUnformatted("Welcome to Gladius");
        logViewer();

        try
        {
            if (!m_initialized)
            {
                setup();
            }
        }
        catch (std::exception & e)
        {
            if (m_logger)
            {
                m_logger->addEvent({fmt::format("Unexpected exception: {}", e.what()),
                                    events::Severity::FatalError});
                m_logView.show();
            }
            else
            {
                std::cerr << fmt::format("Unexpected exception: {}", e.what()) << "\n";
            }
        }
    }

    void MainWindow::refreshModel()
    {
        if (m_doc->refreshModelIfNoCompilationIsRunning())
        {
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

              const auto parameterModifiedByModelEditor = m_modelEditor.showAndEdit();
              m_parameterDirty = parameterModifiedByModelEditor || m_parameterDirty;
              m_dirty = m_parameterDirty || m_dirty;
              m_contoursDirty = m_parameterDirty || m_contoursDirty;
              if (m_modelEditor.modelWasModified() || parameterModifiedByModelEditor)
              {
                  m_doc->getAssembly()->updateInputsAndOutputs();
                  markFileAsChanged();
              }

              if (m_modelEditor.isCompileRequested() && m_core->isRendererReady())
              {
                  refreshModel();
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
        ImGui::SetWindowSize(
          ImVec2(io.DisplaySize.x - silderWidth, io.DisplaySize.y - menuBarHeight));

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
        m_doc->newFromTemplate();
        resetEditorState();
        m_renderWindow.centerView();
    }

    void MainWindow::renderWindow()
    {
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
        const auto menuWidth = 400.f  * m_uiScale;
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
                  reinterpret_cast<const char *>("\t" ICON_FA_FILE_CODE "\tImage Stack")))
            {
                closeMenu();
                QueriedFilename filename;
                std::filesystem::path suggestedFilename =
                  m_currentAssemblyFileName.value_or("imagestack");
                suggestedFilename.replace_extension("imagestack.3mf");

                filename = querySaveFilename({"*.3mf"}, suggestedFilename);

                if (filename.has_value())
                {
                    io::ImageStackExporter exporter(m_logger);
                    exporter.beginExport(filename.value(), *m_core);

                    if (filename.has_value())
                    {
                        exporter.beginExport(filename.value(), *m_core);
                        while (exporter.advanceExport(*m_core))
                        {
                            std::cout << " Processing layer with z = " << m_core->getSliceHeight()
                                      << "\n";
                        }
                        exporter.finalize();
                    }
                }
            }
        }

        ImGui::Separator();
        
        // Add 3MF File Browser menu item
        if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_FOLDER_OPEN "\t3MF Browser")))
        {
            closeMenu();
            m_threemfFileViewer.setDirectory(getAppDir() / "examples");
            m_threemfFileViewer.setVisibility(true);
        }
        
        if (m_showSettings)
        {
            if (ImGui::MenuItem(reinterpret_cast<const char *>(ICON_FA_COG "\tSettings")))
            {
                closeMenu();
                m_mainView.setViewSettingsVisible(true);
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
        m_currentAssemblyFileName = filename;
        m_doc->loadNonBlocking(filename);
        resetEditorState();
        m_renderWindow.centerView();
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
}
