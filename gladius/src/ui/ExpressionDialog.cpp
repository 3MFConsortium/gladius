#include "ExpressionDialog.h"
#include "../ExpressionParser.h"
#include "../FunctionArgument.h"

#include "imgui.h"
#include <algorithm>
#include <fmt/format.h>

namespace gladius::ui
{
    ExpressionDialog::ExpressionDialog()
        : m_parser(std::make_unique<ExpressionParser>())
    {
    }

    ExpressionDialog::~ExpressionDialog() = default;

    std::vector<FunctionArgument> const & ExpressionDialog::getFunctionArguments() const
    {
        return m_arguments;
    }

    void ExpressionDialog::setFunctionArguments(std::vector<FunctionArgument> const & arguments)
    {
        m_arguments = arguments;
        // Clear buffers when arguments change
        m_newArgumentNameBuffer[0] = '\0';
        m_selectedArgumentType = 0;
        m_argumentToRemove = -1;
    }

    void ExpressionDialog::show()
    {
        m_visible = true;
        m_needsValidation = true;
    }

    void ExpressionDialog::hide()
    {
        m_visible = false;
    }

    bool ExpressionDialog::isVisible() const
    {
        return m_visible;
    }

    void ExpressionDialog::render()
    {
        if (!m_visible)
        {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(600, 550), ImGuiCond_FirstUseEver);

        if (ImGui::Begin(
              "Mathematical Expression Function Creator", &m_visible, ImGuiWindowFlags_NoCollapse))
        {
            renderFunctionNameInput();
            ImGui::Separator();
            renderArgumentsSection();
            ImGui::Separator();
            renderExpressionInput();
            ImGui::Separator();
            renderValidationStatus();
            ImGui::Separator();
            renderVariablesList();
            ImGui::Separator();
            renderButtons();
        }
        ImGui::End();

        // If window was closed via X button, hide it
        if (!m_visible)
        {
            hide();
        }
    }

    void ExpressionDialog::setFunctionName(std::string const & functionName)
    {
        m_functionName = functionName;

        // Copy to ImGui buffer, ensuring null termination
        size_t copySize = std::min(functionName.length(), FUNCTION_NAME_BUFFER_SIZE - 1);
        std::copy(functionName.begin(), functionName.begin() + copySize, m_functionNameBuffer);
        m_functionNameBuffer[copySize] = '\0';
    }

    std::string const & ExpressionDialog::getFunctionName() const
    {
        return m_functionName;
    }

    void ExpressionDialog::setExpression(std::string const & expression)
    {
        m_expression = expression;

        // Copy to ImGui buffer, ensuring null termination
        size_t copySize = std::min(expression.length(), EXPRESSION_BUFFER_SIZE - 1);
        std::copy(expression.begin(), expression.begin() + copySize, m_expressionBuffer);
        m_expressionBuffer[copySize] = '\0';

        m_needsValidation = true;
    }

    std::string const & ExpressionDialog::getExpression() const
    {
        return m_expression;
    }

    void ExpressionDialog::setOnApplyCallback(OnApplyCallback callback)
    {
        m_onApplyCallback = std::move(callback);
    }

    void ExpressionDialog::setOnPreviewCallback(OnPreviewCallback callback)
    {
        m_onPreviewCallback = std::move(callback);
    }

    void ExpressionDialog::validateExpression()
    {
        if (!m_needsValidation && m_lastValidatedExpression == m_expression)
        {
            return;
        }

        m_lastValidatedExpression = m_expression;
        m_needsValidation = false;

        if (m_expression.empty())
        {
            m_isValid = false;
            return;
        }

        m_isValid = m_parser->parseExpression(m_expression);
    }

    void ExpressionDialog::renderArgumentsSection()
    {
        ImGui::Text("Function Arguments:");

        if (m_arguments.empty())
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                               "No arguments defined. This function will take no inputs.");
        }
        else
        {
            // Display existing arguments in a table
            if (ImGui::BeginTable(
                  "##ArgumentsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < m_arguments.size(); ++i)
                {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", m_arguments[i].name.c_str());

                    ImGui::TableSetColumnIndex(1);
                    std::string typeStr = ArgumentUtils::argumentTypeToString(m_arguments[i].type);
                    ImGui::Text("%s", typeStr.c_str());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::Button("Remove", ImVec2(70, 0)))
                    {
                        m_argumentToRemove = static_cast<int>(i);
                    }
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
        }

        ImGui::Spacing();

        // Add new argument section
        ImGui::Text("Add New Argument:");

        // Argument name input
        ImGui::Text("Name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText("##newArgName", m_newArgumentNameBuffer, ARGUMENT_NAME_BUFFER_SIZE))
        {
            // Input changed - validation will happen when adding
        }

        // Argument type selection
        ImGui::SameLine();
        ImGui::Text("Type:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        const char * typeItems[] = {"Scalar", "Vector"};
        ImGui::Combo("##argType", &m_selectedArgumentType, typeItems, IM_ARRAYSIZE(typeItems));

        // Add button
        ImGui::SameLine();

        // Validate input before enabling add button
        std::string newArgName = m_newArgumentNameBuffer;
        bool canAdd = !newArgName.empty() && ArgumentUtils::isValidArgumentName(newArgName) &&
                      std::find_if(m_arguments.begin(),
                                   m_arguments.end(),
                                   [&newArgName](FunctionArgument const & arg)
                                   { return arg.name == newArgName; }) == m_arguments.end();

        if (!canAdd)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Add Argument"))
        {
            ArgumentType type =
              (m_selectedArgumentType == 0) ? ArgumentType::Scalar : ArgumentType::Vector;
            m_arguments.emplace_back(newArgName, type);
            m_newArgumentNameBuffer[0] = '\0'; // Clear input
            m_needsValidation = true;          // Re-validate expression with new arguments
        }

        if (!canAdd)
        {
            ImGui::EndDisabled();
        }

        // Show validation message for argument name
        if (!newArgName.empty() && !canAdd)
        {
            if (!ArgumentUtils::isValidArgumentName(newArgName))
            {
                ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f),
                                   "Invalid name: use letters, numbers, underscore only");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "Argument name already exists");
            }
        }

        // Handle removal (do this after rendering to avoid iterator invalidation)
        if (m_argumentToRemove >= 0 && m_argumentToRemove < static_cast<int>(m_arguments.size()))
        {
            m_arguments.erase(m_arguments.begin() + m_argumentToRemove);
            m_argumentToRemove = -1;
            m_needsValidation = true; // Re-validate expression
        }

        // Show helpful information
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                           "Scalar arguments can be used as single values (e.g., 'radius')");
        ImGui::TextColored(
          ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
          "Vector arguments can be accessed as components (e.g., 'pos.x', 'pos.y', 'pos.z')");
    }

    void ExpressionDialog::renderFunctionNameInput()
    {
        ImGui::Text("Function Name:");
        ImGui::PushItemWidth(-1); // Full width

        if (ImGui::InputText("##functionName", m_functionNameBuffer, FUNCTION_NAME_BUFFER_SIZE))
        {
            m_functionName = m_functionNameBuffer;
        }

        ImGui::PopItemWidth();

        // Show naming guidelines
        ImGui::TextColored(
          ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
          "Name your function descriptively (e.g., 'Distance_To_Center', 'Wave_Pattern')");
    }

    void ExpressionDialog::renderExpressionInput()
    {
        ImGui::Text("Enter Mathematical Expression:");
        ImGui::PushItemWidth(-1); // Full width

        if (ImGui::InputTextMultiline(
              "##expression", m_expressionBuffer, EXPRESSION_BUFFER_SIZE, ImVec2(-1, 80)))
        {
            m_expression = m_expressionBuffer;
            m_needsValidation = true;
        }

        ImGui::PopItemWidth();

        // Show some example expressions
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                           "Examples: x + y, sin(x) * cos(y), sqrt(x*x + y*y)");
    }

    void ExpressionDialog::renderValidationStatus()
    {
        validateExpression();

        if (m_expression.empty())
        {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Enter an expression above");
            return;
        }

        if (m_isValid)
        {
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "✓ Expression is valid");

            // Show the parsed expression
            if (m_parser->hasValidExpression())
            {
                std::string parsedExpression = m_parser->getExpressionString();
                if (!parsedExpression.empty() && parsedExpression != m_expression)
                {
                    ImGui::Text("Parsed as: %s", parsedExpression.c_str());
                }
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), "✗ Invalid expression");

            std::string error = m_parser->getLastError();
            if (!error.empty())
            {
                ImGui::TextWrapped("Error: %s", error.c_str());
            }
        }
    }

    void ExpressionDialog::renderVariablesList()
    {
        if (!m_isValid || !m_parser->hasValidExpression())
        {
            return;
        }

        std::vector<std::string> variables = m_parser->getVariables();

        if (variables.empty())
        {
            ImGui::Text("No variables found (expression uses only constants)");
            return;
        }

        ImGui::Text("Variables found:");
        ImGui::Indent();

        for (std::string const & var : variables)
        {
            ImGui::BulletText("%s", var.c_str());
        }

        ImGui::Unindent();

        if (variables.size() > 3)
        {
            ImGui::TextColored(
              ImVec4(0.8f, 0.8f, 0.0f, 1.0f),
              "Note: Many variables detected. Consider using fewer for better performance.");
        }
    }

    void ExpressionDialog::renderButtons()
    {
        ImGui::Spacing();

        // Apply button - only enabled if expression is valid and function name is not empty
        bool canApply = m_isValid && m_parser->hasValidExpression() && !m_functionName.empty();

        if (!canApply)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Create Function", ImVec2(120, 0)))
        {
            if (m_onApplyCallback && canApply)
            {
                m_onApplyCallback(m_functionName, m_expression, m_arguments);
                hide(); // Close dialog after applying
            }
        }

        if (!canApply)
        {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        // Preview button - enabled if expression is valid
        bool canPreview = m_isValid && m_parser->hasValidExpression();

        if (!canPreview)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Preview", ImVec2(100, 0)))
        {
            if (m_onPreviewCallback && canPreview)
            {
                m_onPreviewCallback(m_expression);
            }
        }

        if (!canPreview)
        {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        // Cancel button - always enabled
        if (ImGui::Button("Cancel", ImVec2(100, 0)))
        {
            hide();
        }

        // Help text
        ImGui::Spacing();
        ImGui::TextColored(
          ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
          "Supported functions: sin, cos, tan, exp, log, sqrt, abs, pow, min, max, etc.");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                           "Variables (x, y, z) will become input nodes in the function graph.");
    }

} // namespace gladius::ui
