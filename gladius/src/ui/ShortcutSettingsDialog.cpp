#include "ShortcutSettingsDialog.h"

#include <algorithm>
#include <unordered_set>
#include <vector>
#include <imgui.h>
#include <imgui_stdlib.h>

namespace gladius::ui
{
    ShortcutSettingsDialog::ShortcutSettingsDialog(std::shared_ptr<ShortcutManager> shortcutManager)
        : m_shortcutManager(std::move(shortcutManager))
    {
    }

    void ShortcutSettingsDialog::show()
    {
        m_visible = true;
    }

    void ShortcutSettingsDialog::hide()
    {
        m_visible = false;
        m_isCapturingInput = false;
    }

    bool ShortcutSettingsDialog::isVisible() const
    {
        return m_visible;
    }

    void ShortcutSettingsDialog::render()
    {
        if (!m_visible)
        {
            return;
        }

        constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_AlwaysAutoResize;
        
        if (!ImGui::Begin("Keyboard Shortcuts", &m_visible, windowFlags))
        {
            ImGui::End();
            return;
        }        // Search filter
        ImGui::Text("Filter:");
        ImGui::SameLine();

        ImGui::InputText("##ShortcutSearch", &m_searchFilter);

        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            m_searchFilter.clear();
        }

        ImGui::Separator();

        // Reset all shortcuts button
        if (ImGui::Button("Reset All to Defaults"))
        {
            m_shortcutManager->resetAllShortcutsToDefault();
        }

        ImGui::Separator();

        // If we're capturing input, display a message
        if (m_isCapturingInput)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 0, 255));
            ImGui::TextWrapped("Press a key combination (with modifiers if desired)...");
            ImGui::TextWrapped("Press Escape to cancel");
            ImGui::PopStyleColor();

            // Cancel capture if Escape is pressed
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                m_isCapturingInput = false;
            }

            // Detect key press
            ImGuiIO& io = ImGui::GetIO();
            bool keyPressed = false;
            ImGuiKey pressedKey = ImGuiKey_None;

            // Check for any key press except for modifiers and Escape (already handled)
            for (int i = ImGuiKey_Tab; i < ImGuiKey_COUNT; i++)
            {
                ImGuiKey key = static_cast<ImGuiKey>(i);
                if (key != ImGuiKey_LeftCtrl && key != ImGuiKey_RightCtrl &&
                    key != ImGuiKey_LeftShift && key != ImGuiKey_RightShift &&
                    key != ImGuiKey_LeftAlt && key != ImGuiKey_RightAlt &&
                    key != ImGuiKey_Escape)
                {
                    if (ImGui::IsKeyPressed(key, false))
                    {
                        pressedKey = key;
                        keyPressed = true;
                        break;
                    }
                }
            }

            if (keyPressed)
            {
                // Create shortcut from current modifiers and pressed key
                ShortcutCombo combo(pressedKey, io.KeyCtrl, io.KeyAlt, io.KeyShift);
                
                // Apply the new shortcut
                if (!combo.isEmpty())
                {
                    m_shortcutManager->setShortcut(m_capturingForActionId, combo);
                }
                
                m_isCapturingInput = false;
            }
        }
        else
        {
            // Get all contexts from registered actions
            std::unordered_set<ShortcutContext> contexts;
            for (auto const& action : m_shortcutManager->getActions())
            {
                contexts.insert(action->getContext());
            }

            // Render each context section
            if (contexts.contains(ShortcutContext::Global))
            {
                renderContextSection(ShortcutContext::Global);
            }
            
            if (contexts.contains(ShortcutContext::RenderWindow))
            {
                renderContextSection(ShortcutContext::RenderWindow);
            }
            
            if (contexts.contains(ShortcutContext::ModelEditor))
            {
                renderContextSection(ShortcutContext::ModelEditor);
            }
            
            if (contexts.contains(ShortcutContext::SlicePreview))
            {
                renderContextSection(ShortcutContext::SlicePreview);
            }
        }

        ImGui::End();
    }

    void ShortcutSettingsDialog::renderContextSection(ShortcutContext context)
    {
        // Get actions for this context
        std::vector<std::shared_ptr<ShortcutAction>> contextActions;
        for (auto const& action : m_shortcutManager->getActions())
        {
            if (action->getContext() == context)
            {
                // Apply search filter if any
                if (!m_searchFilter.empty())
                {
                    std::string actionName = action->getName();
                    std::string actionDesc = action->getDescription();
                    
                    // Convert to lowercase for case-insensitive search
                    std::transform(actionName.begin(), actionName.end(), actionName.begin(), 
                                   [](unsigned char c) { return std::tolower(c); });
                    std::transform(actionDesc.begin(), actionDesc.end(), actionDesc.begin(), 
                                   [](unsigned char c) { return std::tolower(c); });
                    
                    std::string filter = m_searchFilter;
                    std::transform(filter.begin(), filter.end(), filter.begin(), 
                                   [](unsigned char c) { return std::tolower(c); });
                    
                    if (actionName.find(filter) == std::string::npos && 
                        actionDesc.find(filter) == std::string::npos)
                    {
                        continue;
                    }
                }
                
                contextActions.push_back(action);
            }
        }
        
        // Skip empty sections
        if (contextActions.empty())
        {
            return;
        }
        
        // Create collapsible header for this context
        if (ImGui::CollapsingHeader(contextToString(context).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            
            // Table to align the shortcuts and their names
            if (ImGui::BeginTable("ShortcutTable", 4, ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                
                // List all actions in this context
                for (auto const& action : contextActions)
                {
                    ImGui::TableNextRow();
                    
                    // Action name and description
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(action->getName().c_str());
                    ImGui::TextDisabled("%s", action->getDescription().c_str());
                    
                    // Current shortcut
                    ImGui::TableNextColumn();
                    ShortcutCombo shortcut = m_shortcutManager->getShortcut(action->getId());
                    ImGui::TextUnformatted(shortcut.toString().c_str());
                    
                    // Edit button
                    ImGui::TableNextColumn();
                    std::string buttonId = "Edit##" + action->getId();
                    if (ImGui::Button(buttonId.c_str()))
                    {
                        m_isCapturingInput = true;
                        m_capturingForActionId = action->getId();
                    }
                    
                    // Reset button
                    ImGui::TableNextColumn();
                    buttonId = "Reset##" + action->getId();
                    if (ImGui::Button(buttonId.c_str()))
                    {
                        m_shortcutManager->resetShortcutToDefault(action->getId());
                    }
                }
                
                ImGui::EndTable();
            }
            
            ImGui::Unindent();
        }
    }

    void ShortcutSettingsDialog::setShortcutManager(std::shared_ptr<ShortcutManager> shortcutManager)
    {
        m_shortcutManager = std::move(shortcutManager);
    }

} // namespace gladius::ui
