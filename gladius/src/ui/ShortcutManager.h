#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "../ConfigManager.h"
#include "imgui.h"

namespace gladius::ui
{
    /**
     * @brief Represents a keyboard shortcut combination
     * 
     * Stores a key combination consisting of a main key and optional modifier keys
     * (Ctrl, Alt, Shift).
     */
    class ShortcutCombo
    {
    public:
        /**
         * @brief Default constructor
         */
        ShortcutCombo() = default;

        /**
         * @brief Construct a new ShortcutCombo from key and modifiers
         * @param key The main key (ImGuiKey)
         * @param ctrl Whether Ctrl is required
         * @param alt Whether Alt is required
         * @param shift Whether Shift is required
         */
        ShortcutCombo(ImGuiKey key, bool ctrl = false, bool alt = false, bool shift = false);

        /**
         * @brief Parse a keyboard shortcut from string format
         * @param comboStr String representation (e.g., "Ctrl+Shift+S")
         * @return ShortcutCombo object parsed from string
         */
        static ShortcutCombo fromString(std::string const& comboStr);

        /**
         * @brief Convert shortcut to string representation
         * @return String representation (e.g., "Ctrl+Shift+S")
         */
        std::string toString() const;

        /**
         * @brief Check if this shortcut is triggered by the current input state
         * @return true if the shortcut combo is pressed
         */
        bool isPressed() const;

        /**
         * @brief Check if this shortcut combination is empty
         * @return true if no keys are assigned
         */
        bool isEmpty() const;

        /**
         * @brief Get the main key of the shortcut
         * @return The main key
         */
        ImGuiKey getKey() const { return m_key; }

        /**
         * @brief Get the ctrl modifier state
         * @return true if Ctrl is required
         */
        bool getCtrl() const { return m_ctrl; }

        /**
         * @brief Get the alt modifier state
         * @return true if Alt is required
         */
        bool getAlt() const { return m_alt; }

        /**
         * @brief Get the shift modifier state
         * @return true if Shift is required
         */
        bool getShift() const { return m_shift; }

    /**
     * @brief Returns wheel direction for wheel-based shortcuts
     * @return +1 for WheelUp, -1 for WheelDown, 0 if not a wheel combo
     */
    int getWheelDirection() const { return m_wheelDirection; }

        /**
         * @brief Check if two shortcut combinations are equal
         * @param other The other shortcut to compare with
         * @return true if the shortcuts are equal
         */
        bool operator==(ShortcutCombo const& other) const;

    private:
        ImGuiKey m_key = ImGuiKey_None;
        bool m_ctrl = false;
        bool m_alt = false;
        bool m_shift = false;
    // Wheel-based shortcut support: +1 = WheelUp, -1 = WheelDown, 0 = none
    int m_wheelDirection = 0;
    };

    /**
     * @brief Context in which a shortcut can be used
     */
    enum class ShortcutContext
    {
        Global,             ///< Global shortcuts, always active
        RenderWindow,       ///< Shortcuts active in the 3D render window
        ModelEditor,        ///< Shortcuts active in the model editor
        SlicePreview        ///< Shortcuts active in the slice preview
    };

    /**
     * @brief Convert a ShortcutContext to a string name
     * @param context The context to convert
     * @return String representation of the context
     */
    std::string contextToString(ShortcutContext context);

    /**
     * @brief Represents an action that can be triggered by a shortcut
     */
    class ShortcutAction
    {
    public:
        /**
         * @brief Callback type for shortcut actions
         */
        using ActionCallback = std::function<void()>;

        /**
         * @brief Construct a new ShortcutAction
         * @param id Unique identifier for the action
         * @param name User-friendly name of the action
         * @param description Description of what the action does
         * @param context Context in which the action is available
         * @param callback Function to call when the shortcut is triggered
         */
        ShortcutAction(std::string id, 
                      std::string name, 
                      std::string description, 
                      ShortcutContext context,
                      ActionCallback callback);

        /**
         * @brief Get the unique ID of the action
         * @return Action ID
         */
        std::string const& getId() const { return m_id; }

        /**
         * @brief Get the user-friendly name of the action
         * @return Action name
         */
        std::string const& getName() const { return m_name; }

        /**
         * @brief Get the description of the action
         * @return Action description
         */
        std::string const& getDescription() const { return m_description; }

        /**
         * @brief Get the context in which the action is available
         * @return Action context
         */
        ShortcutContext getContext() const { return m_context; }

        /**
         * @brief Execute the action callback
         */
        void execute() const;

    private:
        std::string m_id;
        std::string m_name;
        std::string m_description;
        ShortcutContext m_context;
        ActionCallback m_callback;
    };

    /**
     * @brief Manager for keyboard shortcuts throughout the application
     * 
     * Provides centralized registration and handling of keyboard shortcuts.
     * Shortcuts can be associated with specific contexts and are configurable
     * by the user.
     */
    class ShortcutManager
    {
    public:
        /**
         * @brief Construct a new ShortcutManager object
         * @param configManager Configuration manager to store shortcut settings
         */
        explicit ShortcutManager(std::shared_ptr<ConfigManager> configManager);

        /**
         * @brief Destroy the ShortcutManager object
         */
        ~ShortcutManager() = default;

        /**
         * @brief Register a new action with a default shortcut
         * @param id Unique identifier for the action
         * @param name User-friendly name of the action
         * @param description Description of what the action does
         * @param context Context in which the action is available
         * @param defaultShortcut Default key combination
         * @param callback Function to call when the shortcut is triggered
         * @return true if the action was registered successfully
         */
        bool registerAction(std::string const& id, 
                           std::string const& name, 
                           std::string const& description,
                           ShortcutContext context,
                           ShortcutCombo const& defaultShortcut,
                           ShortcutAction::ActionCallback callback);

        /**
         * @brief Process keyboard input and trigger actions if matching shortcuts are found
         * @param activeContext The currently active context to filter actions
         */
        void processInput(ShortcutContext activeContext);

        /**
         * @brief Get all registered actions
         * @return Vector of registered actions
         */
        std::vector<std::shared_ptr<ShortcutAction>> const& getActions() const { return m_actions; }

        /**
         * @brief Get the shortcut assigned to an action
         * @param actionId ID of the action
         * @return Assigned shortcut combo
         */
        ShortcutCombo getShortcut(std::string const& actionId) const;

        /**
         * @brief Set a new shortcut for an action
         * @param actionId ID of the action
         * @param combo New shortcut combo
         * @return true if the shortcut was updated successfully
         */
        bool setShortcut(std::string const& actionId, ShortcutCombo const& combo);

        /**
         * @brief Reset a shortcut to its default value
         * @param actionId ID of the action
         * @return true if the shortcut was reset successfully
         */
        bool resetShortcutToDefault(std::string const& actionId);

        /**
         * @brief Reset all shortcuts to their default values
         */
        void resetAllShortcutsToDefault();

        /**
         * @brief Save current shortcut configuration to the config manager
         */
        void saveShortcuts();

        /**
         * @brief Load shortcut configuration from the config manager
         */
        void loadShortcuts();

    private:
        std::shared_ptr<ConfigManager> m_configManager;
        std::vector<std::shared_ptr<ShortcutAction>> m_actions;
        std::unordered_map<std::string, ShortcutCombo> m_shortcuts;
        std::unordered_map<std::string, ShortcutCombo> m_defaultShortcuts;

        // Disable copy and move
        ShortcutManager(ShortcutManager const&) = delete;
        ShortcutManager& operator=(ShortcutManager const&) = delete;
        ShortcutManager(ShortcutManager&&) = delete;
        ShortcutManager& operator=(ShortcutManager&&) = delete;
    };

} // namespace gladius::ui
