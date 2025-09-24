#include "ShortcutManager.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <unordered_map>

namespace gladius::ui
{
    // Map from string representation to ImGuiKey
    static const std::unordered_map<std::string, ImGuiKey> s_keyNameToImGuiKey = {
      {"A", ImGuiKey_A},
      {"B", ImGuiKey_B},
      {"C", ImGuiKey_C},
      {"D", ImGuiKey_D},
      {"E", ImGuiKey_E},
      {"F", ImGuiKey_F},
      {"G", ImGuiKey_G},
      {"H", ImGuiKey_H},
      {"I", ImGuiKey_I},
      {"J", ImGuiKey_J},
      {"K", ImGuiKey_K},
      {"L", ImGuiKey_L},
      {"M", ImGuiKey_M},
      {"N", ImGuiKey_N},
      {"O", ImGuiKey_O},
      {"P", ImGuiKey_P},
      {"Q", ImGuiKey_Q},
      {"R", ImGuiKey_R},
      {"S", ImGuiKey_S},
      {"T", ImGuiKey_T},
      {"U", ImGuiKey_U},
      {"V", ImGuiKey_V},
      {"W", ImGuiKey_W},
      {"X", ImGuiKey_X},
      {"Y", ImGuiKey_Y},
      {"Z", ImGuiKey_Z},
      {"0", ImGuiKey_0},
      {"1", ImGuiKey_1},
      {"2", ImGuiKey_2},
      {"3", ImGuiKey_3},
      {"4", ImGuiKey_4},
      {"5", ImGuiKey_5},
      {"6", ImGuiKey_6},
      {"7", ImGuiKey_7},
      {"8", ImGuiKey_8},
      {"9", ImGuiKey_9},
      {"F1", ImGuiKey_F1},
      {"F2", ImGuiKey_F2},
      {"F3", ImGuiKey_F3},
      {"F4", ImGuiKey_F4},
      {"F5", ImGuiKey_F5},
      {"F6", ImGuiKey_F6},
      {"F7", ImGuiKey_F7},
      {"F8", ImGuiKey_F8},
      {"F9", ImGuiKey_F9},
      {"F10", ImGuiKey_F10},
      {"F11", ImGuiKey_F11},
      {"F12", ImGuiKey_F12},
      {"Space", ImGuiKey_Space},
      {"Escape", ImGuiKey_Escape},
      {"Enter", ImGuiKey_Enter},
      {"Tab", ImGuiKey_Tab},
      {"Backspace", ImGuiKey_Backspace},
      {"Insert", ImGuiKey_Insert},
      {"Delete", ImGuiKey_Delete},
      {"Home", ImGuiKey_Home},
      {"End", ImGuiKey_End},
      {"PageUp", ImGuiKey_PageUp},
      {"PageDown", ImGuiKey_PageDown},
      {"Left", ImGuiKey_LeftArrow},
      {"Right", ImGuiKey_RightArrow},
      {"Up", ImGuiKey_UpArrow},
      {"Down", ImGuiKey_DownArrow},
      {"+", ImGuiKey_KeypadAdd},
      {"-", ImGuiKey_KeypadSubtract},
      {"*", ImGuiKey_KeypadMultiply},
      {"/", ImGuiKey_KeypadDivide},
      {"=", ImGuiKey_Equal},
      {",", ImGuiKey_Comma},
      {".", ImGuiKey_Period}};

    // Map from ImGuiKey to string representation
    static std::unordered_map<ImGuiKey, std::string> s_imGuiKeyToKeyName;

    // Initialize the reverse map once
    static void initKeyMaps()
    {
        static bool s_initialized = false;
        if (!s_initialized)
        {
            for (auto const & [name, key] : s_keyNameToImGuiKey)
            {
                s_imGuiKeyToKeyName[key] = name;
            }
            s_initialized = true;
        }
    }

    // ShortcutCombo implementation
    ShortcutCombo::ShortcutCombo(ImGuiKey key, bool ctrl, bool alt, bool shift)
        : m_key(key)
        , m_ctrl(ctrl)
        , m_alt(alt)
        , m_shift(shift)
    {
    }

    ShortcutCombo ShortcutCombo::fromString(std::string const & comboStr)
    {
        initKeyMaps();

        ShortcutCombo combo;

        if (comboStr.empty())
        {
            return combo;
        }

        std::istringstream ss(comboStr);
        std::string token;

        while (std::getline(ss, token, '+'))
        {
            // Trim whitespace
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);

            if (token == "Ctrl")
            {
                combo.m_ctrl = true;
            }
            else if (token == "Alt")
            {
                combo.m_alt = true;
            }
            else if (token == "Shift")
            {
                combo.m_shift = true;
            }
            else if (token == "WheelUp")
            {
                combo.m_wheelDirection = +1;
            }
            else if (token == "WheelDown")
            {
                combo.m_wheelDirection = -1;
            }
            else if (s_keyNameToImGuiKey.contains(token))
            {
                combo.m_key = s_keyNameToImGuiKey.at(token);
            }
        }

        return combo;
    }

    std::string ShortcutCombo::toString() const
    {
        initKeyMaps();

        if (isEmpty())
        {
            return "None";
        }

        std::string result;

        if (m_ctrl)
        {
            result += "Ctrl+";
        }

        if (m_alt)
        {
            result += "Alt+";
        }

        if (m_shift)
        {
            result += "Shift+";
        }

        if (m_key != ImGuiKey_None && s_imGuiKeyToKeyName.contains(m_key))
        {
            result += s_imGuiKeyToKeyName[m_key];
        }
        else if (m_wheelDirection != 0)
        {
            result += (m_wheelDirection > 0 ? "WheelUp" : "WheelDown");
        }

        return result;
    }

    bool ShortcutCombo::isPressed() const
    {
        if (isEmpty())
        {
            return false;
        }

        ImGuiIO & io = ImGui::GetIO();

        // Wheel-based combos are evaluated via mouse wheel delta in processInput
        if (m_wheelDirection != 0)
        {
            bool ctrlMatches = (io.KeyCtrl == m_ctrl);
            bool altMatches = (io.KeyAlt == m_alt);
            bool shiftMatches = (io.KeyShift == m_shift);
            int const dir = (io.MouseWheel > 0.f ? +1 : (io.MouseWheel < 0.f ? -1 : 0));
            return ctrlMatches && altMatches && shiftMatches && (dir == m_wheelDirection);
        }

        // Check if main key is pressed
        bool keyPressed = (m_key != ImGuiKey_None) && ImGui::IsKeyPressed(m_key, false);

        // Check modifiers match
        bool ctrlMatches = (io.KeyCtrl == m_ctrl);
        bool altMatches = (io.KeyAlt == m_alt);
        bool shiftMatches = (io.KeyShift == m_shift);

        return keyPressed && ctrlMatches && altMatches && shiftMatches;
    }

    bool ShortcutCombo::isEmpty() const
    {
        return m_key == ImGuiKey_None && m_wheelDirection == 0;
    }

    bool ShortcutCombo::operator==(ShortcutCombo const & other) const
    {
        return m_key == other.m_key && m_ctrl == other.m_ctrl && m_alt == other.m_alt &&
               m_shift == other.m_shift;
    }

    // ShortcutAction implementation
    ShortcutAction::ShortcutAction(std::string id,
                                   std::string name,
                                   std::string description,
                                   ShortcutContext context,
                                   ActionCallback callback)
        : m_id(std::move(id))
        , m_name(std::move(name))
        , m_description(std::move(description))
        , m_context(context)
        , m_callback(std::move(callback))
    {
    }

    void ShortcutAction::execute() const
    {
        if (m_callback)
        {
            m_callback();
        }
    }

    std::string contextToString(ShortcutContext context)
    {
        switch (context)
        {
        case ShortcutContext::Global:
            return "Global";
        case ShortcutContext::RenderWindow:
            return "Render Window";
        case ShortcutContext::ModelEditor:
            return "Model Editor";
        case ShortcutContext::SlicePreview:
            return "Slice Preview";
        default:
            return "Unknown";
        }
    }

    // ShortcutManager implementation
    ShortcutManager::ShortcutManager(std::shared_ptr<ConfigManager> configManager)
        : m_configManager(std::move(configManager))
    {
        initKeyMaps();
        loadShortcuts();
    }

    bool ShortcutManager::registerAction(std::string const & id,
                                         std::string const & name,
                                         std::string const & description,
                                         ShortcutContext context,
                                         ShortcutCombo const & defaultShortcut,
                                         ShortcutAction::ActionCallback callback)
    {
        // Check if action with this ID already exists
        auto it = std::find_if(m_actions.begin(),
                               m_actions.end(),
                               [&id](auto const & action) { return action->getId() == id; });

        if (it != m_actions.end())
        {
            // Action already registered
            return false;
        }

        // Create and register the action
        auto action =
          std::make_shared<ShortcutAction>(id, name, description, context, std::move(callback));
        m_actions.push_back(action);

        // Store default shortcut
        m_defaultShortcuts[id] = defaultShortcut;

        // If no custom shortcut is set, use the default
        if (!m_shortcuts.contains(id))
        {
            m_shortcuts[id] = defaultShortcut;
        }

        return true;
    }

    void ShortcutManager::processInput(ShortcutContext activeContext)
    {
        for (auto const & action : m_actions)
        {
            // Only process actions that are in the global context or the active context
            if (action->getContext() != ShortcutContext::Global &&
                action->getContext() != activeContext)
            {
                continue;
            }

            // Get the shortcut for this action
            auto shortcut = getShortcut(action->getId());

            // Check if the shortcut is pressed
            if (!shortcut.isEmpty() && shortcut.isPressed())
            {
                // Execute the action
                action->execute();
            }
        }
    }

    ShortcutCombo ShortcutManager::getShortcut(std::string const & actionId) const
    {
        if (m_shortcuts.contains(actionId))
        {
            return m_shortcuts.at(actionId);
        }

        // If no custom shortcut is set, check if there's a default
        if (m_defaultShortcuts.contains(actionId))
        {
            return m_defaultShortcuts.at(actionId);
        }

        // No shortcut found
        return ShortcutCombo();
    }

    bool ShortcutManager::setShortcut(std::string const & actionId, ShortcutCombo const & combo)
    {
        // Check if action exists
        auto it =
          std::find_if(m_actions.begin(),
                       m_actions.end(),
                       [&actionId](auto const & action) { return action->getId() == actionId; });

        if (it == m_actions.end())
        {
            // Action not found
            return false;
        }

        // Update the shortcut
        m_shortcuts[actionId] = combo;

        // Save the updated shortcuts
        saveShortcuts();

        return true;
    }

    bool ShortcutManager::resetShortcutToDefault(std::string const & actionId)
    {
        // Check if action exists
        auto it =
          std::find_if(m_actions.begin(),
                       m_actions.end(),
                       [&actionId](auto const & action) { return action->getId() == actionId; });

        if (it == m_actions.end())
        {
            // Action not found
            return false;
        }

        // If there's a default shortcut, restore it
        if (m_defaultShortcuts.contains(actionId))
        {
            m_shortcuts[actionId] = m_defaultShortcuts[actionId];
            saveShortcuts();
            return true;
        }

        return false;
    }

    void ShortcutManager::resetAllShortcutsToDefault()
    {
        m_shortcuts = m_defaultShortcuts;
        saveShortcuts();
    }

    void ShortcutManager::saveShortcuts()
    {
        // Check if configuration manager is available
        if (!m_configManager)
        {
            return;
        }

        // Convert shortcuts to strings for storage
        nlohmann::json shortcutsJson = nlohmann::json::object();

        for (auto const & [id, combo] : m_shortcuts)
        {
            shortcutsJson[id] = combo.toString();
        }

        // Save to config
        m_configManager->setValue("shortcuts", "mappings", shortcutsJson);
        m_configManager->save();
    }

    void ShortcutManager::loadShortcuts()
    {
        // Check if configuration manager is available
        if (!m_configManager)
        {
            return;
        }

        // Clear existing shortcuts
        m_shortcuts.clear();

        // Load from config
        nlohmann::json shortcutsJson = m_configManager->getValue<nlohmann::json>(
          "shortcuts", "mappings", nlohmann::json::object());

        // Parse shortcuts from strings
        for (auto it = shortcutsJson.begin(); it != shortcutsJson.end(); ++it)
        {
            std::string id = it.key();
            std::string comboStr = it.value().get<std::string>();

            m_shortcuts[id] = ShortcutCombo::fromString(comboStr);
        }
    }

} // namespace gladius::ui
