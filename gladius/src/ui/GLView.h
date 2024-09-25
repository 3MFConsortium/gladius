#pragma once

#include "../gpgpu.h"

#include <filesystem>
#include <functional>

 
#include "imgui.h"

struct GLFWwindow;

namespace gladius
{
    using ViewCallBack = std::function<void()>;
    using RequestCloseCallBack = std::function<void()>;
    using FileDropCallBack = std::function<void(const std::filesystem::path&)>;

    struct WindowSettings
    {
        int width = 1280;
        int height = 720;
        int x = 50;
        int y = 50;
    };

    class GLView
    {
    public:
        GLView();

        ~GLView();

        void startMainLoop();

        void render();

        void setRenderCallback(const ViewCallBack& func);

        void setFileDropCallback(const FileDropCallBack& func);

        void applyFullscreenMode();

        [[nodiscard]] size_t getWidth() const;
        [[nodiscard]] size_t getHeight() const;

        void storeWindowSettings();

        void addViewCallBack(const ViewCallBack& func);
        void clearViewCallback();
        void setRequestCloseCallBack(const ViewCallBack& func);
        bool isViewSettingsVisible();
        void setViewSettingsVisible(bool visible);

        [[nodiscard]] bool isFullScreen() const;
        void setFullScreen(bool enableFullscreen);
        void startAnimationMode();
        void stopAnimationMode();

        // [[nodiscard]]
        // HWND getNativeWindowHandle();

    private:
        void init();
        void setGladiusTheme(ImGuiIO& io);
        void initImgUI();

        void displayUI();

        static void noOp()
        {
        }

        static void noOpFileDrop(const std::filesystem::path &){}

        void handleDropCallback(GLFWwindow *, int count, const char ** paths);
        ViewCallBack m_render = noOp;
        RequestCloseCallBack m_close = noOp;

        FileDropCallBack m_fileDrop = noOpFileDrop;

        bool m_fullScreen = false;
        bool m_glFullScreen = false;
        WindowSettings m_windowSettings;

        std::vector<ViewCallBack> m_viewCallBacks;
        bool m_show_demo_window{false};
        bool m_showViewSettings{false};

        std::filesystem::path m_gladiusImgUiFilename{};
        GLFWwindow* m_window{nullptr};
        bool m_isAnimationRunning = false;
        bool m_stateCloseRequested = false;
    };
}
