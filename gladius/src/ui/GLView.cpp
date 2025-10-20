#include "GLView.h"
#include "../IconFontCppHeaders/IconsFontAwesome5.h"
#include "../gpgpu.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>
#ifdef _WIN32
#include <imgui_impl_win32.h>
#endif
#include "imgui.h"
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include "Profiling.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fmt/format.h>
#include <iostream>
#include <sago/platform_folders.h>
#include <thread>

namespace gladius
{

    GLView::GLView() = default;

    void errorCallback(int error, const char * description)
    {
        // Validate description pointer to prevent potential buffer overruns
        if (description != nullptr)
        {
            std::cerr << "Error: " << error << " " << description << "\n";
        }
        else
        {
            std::cerr << "Error: " << error << " (no description)\n";
        }
    }

    GLView::~GLView()
    {
        // Only tear down if we actually initialized a window/context
        if (m_initialized)
        {
            // Explicitly save ImGui settings before destroying context
            if (!m_iniFileNameStorage.empty())
            {
                try
                {
                    ImGui::SaveIniSettingsToDisk(m_iniFileNameStorage.c_str());
                }
                catch (...)
                {
                    // Ignore save errors during destruction to prevent exceptions
                    std::cerr << "Warning: Failed to save ImGui settings during destruction\n";
                }
            }

            // Clear window user pointer to prevent dangling references
            if (m_window)
            {
                glfwSetWindowUserPointer(m_window, nullptr);
            }

            ImGui_ImplOpenGL2_Shutdown();
            ImGui_ImplGlfw_Shutdown();

            ImGui::DestroyContext();
            if (glfwGetCurrentContext())
            {
                // Terminate GLFW only if it was initialized in this process
                glfwTerminate();
            }
        }
    }

    void GLView::ensureInitialized()
    {
        // Lazily create the window and GL context if not already done
        if (!m_initialized)
        {
            init();
        }
    }

    void GLView::storeWindowSettings()
    {
        ImGuiIO & io = ImGui::GetIO();

        m_windowSettings.width = static_cast<int>(io.DisplaySize.x);
        m_windowSettings.height = static_cast<int>(io.DisplaySize.y);
        glfwGetWindowPos(m_window, &m_windowSettings.x, &m_windowSettings.y);

        // Explicitly save ImGui settings to ensure current window positions are stored
        if (!m_iniFileNameStorage.empty())
        {
            try
            {
                ImGui::SaveIniSettingsToDisk(m_iniFileNameStorage.c_str());
            }
            catch (const std::exception & ex)
            {
                std::cerr << "Warning: Failed to save ImGui settings: " << ex.what() << "\n";
            }
        }
    }

    void GLView::addViewCallBack(const ViewCallBack & func)
    {
        m_viewCallBacks.emplace_back(func);
    }

    void GLView::clearViewCallback()
    {
        m_viewCallBacks.clear();
    }

    void GLView::setRequestCloseCallBack(const ViewCallBack & func)
    {
        m_close = func;
    }

    void GLView::init()
    {
        if (m_initialized)
        {
            return;
        }
        if (!glfwInit())
        {
            std::cerr << "Initialization of OpenGL context failed\n";
            return;
        }

        glfwSetErrorCallback(errorCallback);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

        m_window = glfwCreateWindow(
          1920, 1080, "Gladius - Advanced Cheese Grater Creator", nullptr, nullptr);

        if (!m_window)
        {
            std::cerr << "Window creation failed\n";
            glfwTerminate();
            return;
        }

        static auto staticDropCallback = [](GLFWwindow * window, int count, const char ** paths)
        {
            // Get the GLView instance from the window user pointer
            GLView * view = static_cast<GLView *>(glfwGetWindowUserPointer(window));
            if (view)
            {
                view->handleDropCallback(window, count, paths);
            }
        };

        // Set the window user pointer to this instance
        glfwSetWindowUserPointer(m_window, this);
        glfwSetDropCallback(m_window, staticDropCallback);

        glfwMakeContextCurrent(m_window);
        if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
        {
            std::cerr << "Initializing glad failed\n";
            glfwDestroyWindow(m_window);
            m_window = nullptr;
            glfwTerminate();
            return;
        }
        glShadeModel(GL_FLAT);
        initImgUI();

        // determine hdpi scaling
        determineUiScale();
        static auto staticWindowSizeCallback = [](GLFWwindow * window, int width, int height)
        {
            // Get the GLView instance from the window user pointer
            GLView * view = static_cast<GLView *>(glfwGetWindowUserPointer(window));
            if (view)
            {
                view->determineUiScale();
            }
        };

        static auto staticContentScaleCallback = [](GLFWwindow * window, float xscale, float yscale)
        {
            // Get the GLView instance from the window user pointer
            GLView * view = static_cast<GLView *>(glfwGetWindowUserPointer(window));
            if (view)
            {
                view->determineUiScale();
            }
        };

        glfwSetWindowSizeCallback(m_window, staticWindowSizeCallback);
        glfwSetWindowContentScaleCallback(m_window, staticContentScaleCallback);

        applyFullscreenMode();
        m_initialized = true;
    }

    void GLView::setGladiusTheme(ImGuiIO & io)
    {
        ImGui::StyleColorsDark();
        ImGuiStyle & style = ImGui::GetStyle();
        style.AntiAliasedFill = true;
        style.AntiAliasedLines = true;
        style.FrameRounding = 12.0f;
        style.Alpha = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.ItemSpacing = {9.f, 7.f};
        style.FramePadding.x = 20;
        style.WindowPadding.x = 20;
        style.WindowBorderSize = 0;
        style.FrameBorderSize = 1;
        io.FontAllowUserScaling = false;

        ImVec4 * colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_FrameBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

        colors[ImGuiCol_TitleBgActive] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.97f, 0.97f, 0.97f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.f, 0.f, 1.0f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.1f, 0.1f, 1.0f);
        colors[ImGuiCol_Button] = ImVec4(0.94f, 0.94f, 0.94f, 0.30f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.8f, 0.8f, 0.8f, 0.70f);
        colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.97f, 0.97f, 0.97f, 0.31f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(1.00f, 0.00f, 0.00f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.f, 0.f, 1.0f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.75f, 0.10f, 0.10f, 0.78f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.75f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.97f, 0.97f, 0.97f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.99f, 0.99f, 0.99f, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.95f);
        colors[ImGuiCol_Tab] = ImVec4(0.25f, 0.25f, 0.26f, 0.86f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.71f, 0.00f, 0.00f, 0.80f);
        colors[ImGuiCol_TabActive] = ImVec4(1.00f, 0.01f, 0.01f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.16f, 0.16f, 0.17f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);
        colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.27f, 0.27f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(1.0f, 0.f, 0.f, 1.0f); // also used for progress bar
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
    }

    void GLView::initImgUI()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO & io = ImGui::GetIO();

        // Set up the UI configuration file path
        try
        {
            std::filesystem::path gladiusConfigDir =
              sago::getConfigHome() / std::filesystem::path{"gladius"};
            m_gladiusImgUiFilename = gladiusConfigDir / "ui.config";

            if (!std::filesystem::is_directory(gladiusConfigDir))
            {
                std::filesystem::create_directory(gladiusConfigDir);
            }

            // If our custom config doesn't exist yet but the default ImGui one does, copy it
            if (!std::filesystem::is_regular_file(m_gladiusImgUiFilename) && io.IniFilename &&
                std::filesystem::is_regular_file(io.IniFilename))
            {
                std::filesystem::copy_file(io.IniFilename, m_gladiusImgUiFilename);
            }
        }
        catch (const std::filesystem::filesystem_error & ex)
        {
            std::cerr << "Warning: Failed to set up UI config directory: " << ex.what() << "\n";
            // Fall back to default ImGui behavior
            m_gladiusImgUiFilename.clear();
        }

        // Set the ini filename storage before using it
        if (!m_gladiusImgUiFilename.empty())
        {
            try
            {
                m_iniFileNameStorage = m_gladiusImgUiFilename.string();
                io.IniFilename = m_iniFileNameStorage.c_str();

                // If our custom config exists, load it immediately
                if (std::filesystem::is_regular_file(m_gladiusImgUiFilename))
                {
                    ImGui::LoadIniSettingsFromDisk(m_iniFileNameStorage.c_str());
                }
            }
            catch (const std::exception & ex)
            {
                std::cerr << "Warning: Failed to set up ImGui config file: " << ex.what() << "\n";
                // Clear the filename storage to use ImGui defaults
                m_iniFileNameStorage.clear();
                io.IniFilename = nullptr;
            }
        }
        else
        {
            // Use ImGui defaults if config setup failed
            m_iniFileNameStorage.clear();
            io.IniFilename = nullptr;
        }
#ifdef IMGUI_HAS_DOCK
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.ConfigInputTrickleEventQueue = true;

        setGladiusTheme(io);

        auto const font_scaling_factor = 2.f;
        auto const font_size_in_pixels = 16.f;

        // Load main font with fallback
        if (!io.Fonts->AddFontFromFileTTF("misc/fonts/Roboto-Medium.ttf",
                                          font_size_in_pixels * font_scaling_factor))
        {
            std::cerr << "Warning: Could not load Roboto-Medium.ttf, using default font\n";
        }

        // merge in icons from Font Awesome
        static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphOffset.y += 4.f;

        if (!io.Fonts->AddFontFromFileTTF("misc/fonts/fa-solid-900.ttf",
                                          font_size_in_pixels * font_scaling_factor,
                                          &icons_config,
                                          icons_ranges))
        {
            std::cerr
              << "Warning: Could not load fa-solid-900.ttf, icons may not display correctly\n";
        }

        io.FontGlobalScale /= font_scaling_factor;

        // backup the style
        m_originalStyle = ImGui::GetStyle();

        // Setup Platform/Renderer bindings
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL2_Init();
    }

    auto GLView::isViewSettingsVisible() -> bool
    {
        return m_showViewSettings;
    }

    void GLView::setViewSettingsVisible(bool visible)
    {
        m_showViewSettings = visible;
    }

    void GLView::displayUI()
    {
        glDisable(GL_CULL_FACE);
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();

        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (m_showViewSettings)
        {
            ImGui::Begin("Settings", &m_showViewSettings);
            if (ImGui::CollapsingHeader("Misc"))
            {
                // Window mode selector
                static const char * items[] = {"Windowed",
                                               "Fullscreen (Current Display)",
                                               "Fullscreen (Span Same Height Displays)"};
                int modeIndex = static_cast<int>(m_windowSettings.fullscreenMode);
                if (ImGui::Combo("Window Mode", &modeIndex, items, IM_ARRAYSIZE(items)))
                {
                    setFullscreenMode(static_cast<FullscreenMode>(modeIndex));
                }
                ImGui::Checkbox("Demo Window", &m_show_demo_window);
                if (m_show_demo_window)
                {
                    ImGui::ShowDemoWindow(&m_show_demo_window);
                }
                ImGui::TextUnformatted(fmt::format("Application average {} ms/frame, {} FPS",
                                                   1000.f / ImGui::GetIO().Framerate,
                                                   ImGui::GetIO().Framerate)
                                         .c_str());
            }

            // zoom / dpi scaling
            ImGui::Text("UI Scaling");
            ImGui::Text("Base: %.2f  User: %.2f  Total: %.2f", m_baseScale, m_userScale, m_uiScale);
            if (ImGui::SliderFloat("User UI Scaling", &m_userScale, 0.25f, 5.0f))
            {
                recomputeTotalScale();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset"))
            {
                resetUserScale();
            }

            ImGui::End();
        }

        // Set all scales in style to the same value
        ImGui::GetIO().FontGlobalScale = m_uiScale * 0.5f;
        ImGui::GetStyle() = m_originalStyle;
        ImGuiStyle & style = ImGui::GetStyle();
        style.ScaleAllSizes(m_uiScale);

        for (auto view : m_viewCallBacks)
        {
            view();
        }

        // Rendering
        ImGui::Render();
        ImGuiIO & io = ImGui::GetIO();
        glViewport(0, 0, (GLsizei) io.DisplaySize.x, (GLsizei) io.DisplaySize.y);
        glUseProgram(0);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
#ifdef IMGUI_HAS_VIEWPORT
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow * backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
#endif
    }

    void GLView::determineUiScale()
    {
#ifdef _WIN32
        // Windows DPI scaling
        HWND hwnd = glfwGetWin32Window(m_window);
        if (hwnd)
        {
            m_baseScale = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
            recomputeTotalScale();
            return;
        }

        ImGui_ImplWin32_EnableDpiAwareness();
#endif

        // First, try to use GLFW's content scale detection (preferred method)
        float xscale, yscale;
        glfwGetWindowContentScale(m_window, &xscale, &yscale);

        // GLFW's content scale is more reliable as it considers system DPI settings
        if (xscale > 0.0f && yscale > 0.0f)
        {
            m_baseScale = (xscale + yscale) / 2.0f;
            recomputeTotalScale();
            return;
        }

        // Fallback to framebuffer vs window size ratio (legacy method)
        int width, height;
        glfwGetWindowSize(m_window, &width, &height);
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);

        if (width > 0 && height > 0 && fbWidth > 0 && fbHeight > 0)
        {
            float hdpiScalingX = static_cast<float>(fbWidth) / static_cast<float>(width);
            float hdpiScalingY = static_cast<float>(fbHeight) / static_cast<float>(height);
            m_baseScale = (hdpiScalingX + hdpiScalingY) / 2.0f;
        }
        else
        {
            // Final fallback
            m_baseScale = 1.0f;
        }

        recomputeTotalScale();
    }

    void GLView::recomputeTotalScale()
    {
        // Clamp user scale to a reasonable range
        if (m_userScale < 0.25f)
            m_userScale = 0.25f;
        if (m_userScale > 5.0f)
            m_userScale = 5.0f;
        m_uiScale = m_baseScale * m_userScale;
    }

    void GLView::handleDropCallback(GLFWwindow *, int count, const char ** paths)
    {
        // Validate parameters to prevent buffer overruns
        if (paths == nullptr || count <= 0)
        {
            return;
        }

        for (int i = 0; i < count; ++i)
        {
            if (paths[i] != nullptr)
            {
                try
                {
                    m_fileDrop(std::filesystem::path(paths[i]));
                }
                catch (const std::exception & ex)
                {
                    std::cerr << "Warning: Failed to process dropped file '" << paths[i]
                              << "': " << ex.what() << "\n";
                }
            }
        }
    }

    GLFWmonitor * findCurrentMonitor(GLFWwindow * window)
    {
        if (!window)
        {
            return nullptr;
        }

        int sizeMonitors;
        auto monitors = glfwGetMonitors(&sizeMonitors);
        if (sizeMonitors < 1 || !monitors)
        {
            return nullptr;
        }

        int windowX;
        int windowY;
        glfwGetWindowPos(window, &windowX, &windowY);

        int windowWidth;
        int windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);

        auto * bestmonitor = monitors[0];
        int maxOverlap = 0;
        for (int i = 0; i < sizeMonitors; ++i)
        {
            if (!monitors[i])
            {
                continue;
            }

            auto const mode = glfwGetVideoMode(monitors[i]);
            if (!mode)
            {
                continue;
            }

            int monitorX;
            int monitorY;
            glfwGetMonitorPos(monitors[i], &monitorX, &monitorY);

            auto overlap = std::max(0,
                                    std::min(windowX + windowWidth, monitorX + mode->width) -
                                      std::max(windowX, monitorX)) *
                           std::max(0,
                                    std::min(windowY + windowHeight, monitorY + mode->height) -
                                      std::max(windowY, monitorY));

            if (overlap > maxOverlap)
            {
                maxOverlap = overlap;
                bestmonitor = monitors[i];
            }
        }

        return bestmonitor;
    }

    // Helper: compute union rectangle across monitors that match the reference monitor height
    // and are horizontally aligned (same Y position). This avoids spanning vertically when
    // monitors are stacked.
    static bool computeSpanAcrossSameHeightMonitors(GLFWmonitor * reference,
                                                    int & outX,
                                                    int & outY,
                                                    int & outW,
                                                    int & outH)
    {
        if (!reference)
        {
            return false;
        }

        const GLFWvidmode * refMode = glfwGetVideoMode(reference);
        if (!refMode)
        {
            return false;
        }

        int count = 0;
        GLFWmonitor ** monitors = glfwGetMonitors(&count);
        if (!monitors || count <= 0)
        {
            return false;
        }

        // Initialize with reference monitor rect
        int refX = 0, refY = 0;
        glfwGetMonitorPos(reference, &refX, &refY);
        int minX = refX;
        int maxX = refX + refMode->width;

        for (int i = 0; i < count; ++i)
        {
            GLFWmonitor * m = monitors[i];
            if (!m)
                continue;
            const GLFWvidmode * mode = glfwGetVideoMode(m);
            if (!mode)
                continue;
            // Only consider monitors with exactly the same height
            if (mode->height != refMode->height)
                continue;
            int mx = 0, my = 0;
            glfwGetMonitorPos(m, &mx, &my);
            // Allow some tolerance in vertical alignment (e.g., WM rounding or fractional scaling)
            // Span only monitors whose Y differs at most by 100 px from the reference row
            int const deltaY = my - refY;
            if (deltaY < -100 || deltaY > 100)
                continue;
            minX = std::min(minX, mx);
            maxX = std::max(maxX, mx + mode->width);
        }

        outX = minX;
        outY = refY;
        outW = maxX - minX;
        outH = refMode->height;

        return true;
    }

    // Determine if spanning across multiple monitors is actually available.
    // We consider it available if the computed spanning rectangle is wider than the reference
    // monitor.
    static bool isSpanAcrossSameHeightAvailable(GLFWmonitor * reference)
    {
        if (!reference)
        {
            return false;
        }
        auto const * refMode = glfwGetVideoMode(reference);
        if (!refMode)
        {
            return false;
        }
        int x = 0, y = 0, w = 0, h = 0;
        if (!computeSpanAcrossSameHeightMonitors(reference, x, y, w, h))
        {
            return false;
        }
        return w > refMode->width;
    }

    void GLView::applyFullscreenMode()
    {
        if (!m_window || !m_initialized)
        {
            return;
        }

        const bool modeChanged = (m_windowSettings.fullscreenMode != m_appliedFullscreenMode);

        if (!modeChanged)
        {
            return;
        }

        // Find appropriate monitor
        auto * monitor = findCurrentMonitor(m_window);
        if (!monitor)
        {
            monitor = glfwGetPrimaryMonitor();
        }
        if (!monitor)
        {
            std::cerr << "Warning: No monitor available for fullscreen mode\n";
            return;
        }

        // If span mode is selected but not available, fall back to single-monitor fullscreen
        auto desiredMode = m_windowSettings.fullscreenMode;
        if (desiredMode == FullscreenMode::SpanAllSameHeight &&
            !isSpanAcrossSameHeightAvailable(monitor))
        {
            desiredMode = FullscreenMode::SingleMonitor;
            m_windowSettings.fullscreenMode = desiredMode;
        }

        // Helper lambda to switch to single-monitor fullscreen
        auto switchToSingleMonitor = [&]()
        {
            const GLFWvidmode * mode = glfwGetVideoMode(monitor);
            if (!mode)
            {
                std::cerr << "Warning: Could not get video mode for monitor\n";
                return false;
            }
            glfwSetWindowMonitor(
              m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            return true;
        };

        // Apply the desired mode
        switch (desiredMode)
        {
        case FullscreenMode::SingleMonitor:
            if (switchToSingleMonitor())
            {
                m_appliedFullscreenMode = FullscreenMode::SingleMonitor;
            }
            break;

        case FullscreenMode::SpanAllSameHeight:
        {
            int x = 0, y = 0, w = 0, h = 0;
            if (!computeSpanAcrossSameHeightMonitors(monitor, x, y, w, h))
            {
                // Spanning not possible; fall back to single-monitor fullscreen
                if (switchToSingleMonitor())
                {
                    m_windowSettings.fullscreenMode = FullscreenMode::SingleMonitor;
                    m_appliedFullscreenMode = FullscreenMode::SingleMonitor;
                }
            }
            else
            {
                // Borderless fullscreen window across all matching-height monitors
                glfwSetWindowMonitor(m_window, nullptr, x, y, w, h, GLFW_DONT_CARE);
                glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
                glfwSetWindowAttrib(m_window, GLFW_RESIZABLE, GLFW_FALSE);
                glfwSetWindowPos(m_window, x, y);
                glfwSetWindowSize(m_window, w, h);
                m_appliedFullscreenMode = FullscreenMode::SpanAllSameHeight;
            }
            break;
        }

        case FullscreenMode::Windowed:
        default:
            // Set window attributes before changing mode
            glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
            glfwSetWindowAttrib(m_window, GLFW_RESIZABLE, GLFW_TRUE);
            // Switch to windowed mode
            glfwSetWindowMonitor(m_window,
                                 nullptr,
                                 m_windowSettings.x,
                                 m_windowSettings.y,
                                 m_windowSettings.width,
                                 m_windowSettings.height,
                                 GLFW_DONT_CARE);
            // Force the window manager to recognize the changes (X11 workaround)
            glfwHideWindow(m_window);
            glfwShowWindow(m_window);
            m_appliedFullscreenMode = FullscreenMode::Windowed;
            break;
        }
    }

    auto GLView::getWidth() const -> size_t
    {
        return static_cast<size_t>(ImGui::GetIO().DisplaySize.x);
    }

    auto GLView::getHeight() const -> size_t
    {
        return static_cast<size_t>(ImGui::GetIO().DisplaySize.y);
    }

    void GLView::render()
    {
        if (!m_window)
        {
            return;
        }

        FrameMark;

        glfwMakeContextCurrent(m_window);
        // applyFullscreenMode is now only called when mode changes via setFullscreenMode()
        m_render();
        glFlush();
        glFinish();
        glPopMatrix();

        displayUI();

        // ImGui automatically saves settings periodically, but this ensures
        // we're using our custom file path if ImGui decides to save this frame
        ImGuiIO & io = ImGui::GetIO();
        if (io.WantSaveIniSettings && !m_iniFileNameStorage.empty())
        {
            io.IniFilename = m_iniFileNameStorage.c_str();
        }

        glfwSwapBuffers(m_window);
    }

    std::chrono::milliseconds getTimeStamp_ms()
    {
        auto duration = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    }

    void GLView::startMainLoop()
    {
        // Lazy init of window and ImGui when UI actually starts
        if (!m_initialized)
        {
            init();
        }

        auto lastAnimationTimePoint_ms = getTimeStamp_ms();
        auto lastFrame_ms = getTimeStamp_ms();
        auto constexpr minFrameDurationAnimation =
          std::chrono::milliseconds{static_cast<int>(1000. / 120.)};
        auto constexpr minFrameDurationStatic =
          std::chrono::milliseconds{static_cast<int>(1000. / 60.)};
        try
        {
            while (!m_stateCloseRequested)
            {
                if (glfwWindowShouldClose(m_window))
                {
                    // Save window settings only if we're in windowed mode
                    if (m_appliedFullscreenMode == FullscreenMode::Windowed)
                    {
                        storeWindowSettings();
                    }

                    glfwSetWindowShouldClose(m_window, GLFW_FALSE);
                    m_close();
                    m_stateCloseRequested = false;
                }

                auto const durationSinceLastFrame_ms = getTimeStamp_ms() - lastFrame_ms;
                auto const minFrameDuration =
                  m_isAnimationRunning ? minFrameDurationAnimation : minFrameDurationStatic;

                auto const delay_ms =
                  std::min(minFrameDuration, minFrameDuration - durationSinceLastFrame_ms);
                std::this_thread::sleep_for(delay_ms);

                if (m_isAnimationRunning)
                {
                    glfwPollEvents();
                    lastAnimationTimePoint_ms = getTimeStamp_ms();
                }
                else
                {
                    auto durationSinceAnimation = getTimeStamp_ms() - lastAnimationTimePoint_ms;
                    if (durationSinceAnimation < std::chrono::seconds{5})
                    {
                        glfwPollEvents();
                    }
                    else
                    {
                        glfwWaitEventsTimeout(5);
                        lastAnimationTimePoint_ms = getTimeStamp_ms();
                    }
                }

                glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
                glClear(GL_COLOR_BUFFER_BIT);
                glClear(GL_DEPTH_BUFFER_BIT);
                render();
                lastFrame_ms = getTimeStamp_ms();
            }
        }
        catch (const std::exception & e)
        {
            std::cerr << e.what() << '\n';
        }
    }

    void GLView::setRenderCallback(const ViewCallBack & func)
    {
        m_render = func;
    }

    void GLView::setFileDropCallback(const FileDropCallBack & func)
    {
        m_fileDrop = func;
    }

    auto GLView::isFullScreen() const -> bool
    {
        return m_windowSettings.fullscreenMode != FullscreenMode::Windowed;
    }

    void GLView::setFullScreen(bool enableFullscreen)
    {
        m_windowSettings.fullscreenMode =
          enableFullscreen ? FullscreenMode::SingleMonitor : FullscreenMode::Windowed;
        applyFullscreenMode();
    }

    void GLView::setFullscreenMode(FullscreenMode mode)
    {
        m_windowSettings.fullscreenMode = mode;
        applyFullscreenMode();
    }

    bool GLView::isSpanModeAvailable() const
    {
        if (!m_window || !m_initialized)
        {
            return false;
        }

        auto * monitor = findCurrentMonitor(m_window);
        if (!monitor)
        {
            monitor = glfwGetPrimaryMonitor();
        }

        return monitor && isSpanAcrossSameHeightAvailable(monitor);
    }

    void GLView::startAnimationMode()
    {
        m_isAnimationRunning = true;
    }

    void GLView::stopAnimationMode()
    {
        m_isAnimationRunning = false;
    }
}

// User scale controls
namespace gladius
{
    void GLView::setUserScale(float scale)
    {
        m_userScale = scale;
        recomputeTotalScale();
    }

    void GLView::adjustUserScale(float factor)
    {
        m_userScale *= factor;
        recomputeTotalScale();
    }

    void GLView::resetUserScale()
    {
        m_userScale = 1.0f;
        recomputeTotalScale();
    }
}
