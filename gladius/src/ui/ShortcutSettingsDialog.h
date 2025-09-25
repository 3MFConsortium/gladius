#pragma once

#include "ShortcutManager.h"
#include <memory>
#include <string>

namespace gladius::ui
{
    /**
     * @brief Dialog for configuring keyboard shortcuts
     *
     * Allows users to view and customize keyboard shortcuts for various actions
     * in the application. Shortcuts can be reset to defaults individually or all at once.
     */
    class ShortcutSettingsDialog
    {
      public:
        /**
         * @brief Construct a new ShortcutSettingsDialog
         * @param shortcutManager The shortcut manager to configure
         */
        explicit ShortcutSettingsDialog(std::shared_ptr<ShortcutManager> shortcutManager = nullptr);

        /**
         * @brief Set the shortcut manager
         * @param shortcutManager The new shortcut manager
         */
        void setShortcutManager(std::shared_ptr<ShortcutManager> shortcutManager);

        /**
         * @brief Destroy the ShortcutSettingsDialog
         */
        ~ShortcutSettingsDialog() = default;

        /**
         * @brief Show the shortcut settings dialog
         */
        void show();

        /**
         * @brief Hide the shortcut settings dialog
         */
        void hide();

        /**
         * @brief Check if the dialog is currently visible
         * @return true if the dialog is visible
         */
        bool isVisible() const;

        /**
         * @brief Render the dialog
         *
         * This should be called every frame if the dialog is visible.
         */
        void render();

      private:
        /**
         * @brief Render the shortcut editor for a specific context
         * @param context The context to render shortcuts for
         */
        void renderContextSection(ShortcutContext context);

        std::shared_ptr<ShortcutManager> m_shortcutManager;
        bool m_visible = false;
        std::string m_searchFilter;

        // For capturing key input
        bool m_isCapturingInput = false;
        std::string m_capturingForActionId;
        ShortcutCombo m_pendingShortcut;

        // Disable copy and move
        ShortcutSettingsDialog(ShortcutSettingsDialog const &) = delete;
        ShortcutSettingsDialog & operator=(ShortcutSettingsDialog const &) = delete;
        ShortcutSettingsDialog(ShortcutSettingsDialog &&) = delete;
        ShortcutSettingsDialog & operator=(ShortcutSettingsDialog &&) = delete;
    };

} // namespace gladius::ui
