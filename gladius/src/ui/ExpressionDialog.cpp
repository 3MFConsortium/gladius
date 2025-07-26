#include "ExpressionDialog.h"
#include "../ExpressionParser.h"
#include "../FunctionArgument.h"

#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <fmt/format.h>
#include <iostream>

namespace gladius::ui
{
    ExpressionDialog::ExpressionDialog()
        : m_parser(std::make_unique<ExpressionParser>())
        , m_output(FunctionOutput::defaultOutput())
    {
        // Initialize output name buffer
        std::strcpy(m_outputNameBuffer, m_output.name.c_str());
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

    FunctionOutput const & ExpressionDialog::getFunctionOutput() const
    {
        return m_output;
    }

    void ExpressionDialog::setFunctionOutput(FunctionOutput const & output)
    {
        m_output = output;
        // Update buffer
        size_t copySize = std::min(output.name.length(), OUTPUT_NAME_BUFFER_SIZE - 1);
        std::copy(output.name.begin(), output.name.begin() + copySize, m_outputNameBuffer);
        m_outputNameBuffer[copySize] = '\0';
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
            renderOutputSection();
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

        // Update output type based on expression
        if (m_isValid)
        {
            m_output.type = deduceOutputType();
        }

        std::cout << "Expression validation - expression: '" << m_expression
                  << "', isValid: " << m_isValid
                  << ", hasValidExpression: " << m_parser->hasValidExpression()
                  << ", deduced type: "
                  << (m_output.type == ArgumentType::Scalar ? "Scalar" : "Vector") << std::endl;
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

        // Get cursor position before input
        bool inputActive = ImGui::IsItemActive();

        if (ImGui::InputTextMultiline(
              "##expression", m_expressionBuffer, EXPRESSION_BUFFER_SIZE, ImVec2(-1, 80)))
        {
            m_expression = m_expressionBuffer;
            m_needsValidation = true;

            // Update autocomplete suggestions when text changes
            m_autocompleteSuggestions = getAutocompleteSuggestions();
            m_showAutocomplete = !m_autocompleteSuggestions.empty();
            m_selectedSuggestion = -1;
        }

        // Handle keyboard navigation for autocomplete
        if (ImGui::IsItemActive() && m_showAutocomplete)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                m_selectedSuggestion =
                  (m_selectedSuggestion + 1) % m_autocompleteSuggestions.size();
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                m_selectedSuggestion = m_selectedSuggestion > 0
                                         ? m_selectedSuggestion - 1
                                         : m_autocompleteSuggestions.size() - 1;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Tab) || ImGui::IsKeyPressed(ImGuiKey_Enter))
            {
                if (m_selectedSuggestion >= 0 &&
                    m_selectedSuggestion < m_autocompleteSuggestions.size())
                {
                    // Insert the selected suggestion
                    std::string suggestion = m_autocompleteSuggestions[m_selectedSuggestion];
                    m_expression += suggestion;
                    strncpy(m_expressionBuffer, m_expression.c_str(), EXPRESSION_BUFFER_SIZE - 1);
                    m_expressionBuffer[EXPRESSION_BUFFER_SIZE - 1] = '\0';
                    m_needsValidation = true;
                    m_showAutocomplete = false;
                }
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                m_showAutocomplete = false;
            }
        }

        ImGui::PopItemWidth();

        // Show autocomplete popup
        if (m_showAutocomplete)
        {
            renderAutocompleteSuggestions();
        }

        // Show some example expressions with syntax highlighting hints
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                           "Examples: x + y, sin(x) * cos(y), sqrt(x*x + y*y)");
        ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f),
                           "Vector components: pos.x, pos.y, pos.z");
        if (!m_arguments.empty())
        {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.6f, 1.0f), "Available arguments:");
            ImGui::SameLine();
            for (size_t i = 0; i < m_arguments.size(); ++i)
            {
                if (i > 0)
                    ImGui::SameLine();
                ImVec4 color = m_arguments[i].type == ArgumentType::Vector
                                 ? ImVec4(0.6f, 0.9f, 0.6f, 1.0f)
                                 : ImVec4(0.9f, 0.6f, 0.9f, 1.0f);
                ImGui::TextColored(color, "%s", m_arguments[i].name.c_str());
                if (i < m_arguments.size() - 1)
                    ImGui::SameLine();
            }
        }
    }

    void ExpressionDialog::renderAutocompleteSuggestions()
    {
        if (m_autocompleteSuggestions.empty())
        {
            return;
        }

        ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y));
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetItemRectSize().x, 0));

        if (ImGui::Begin("##autocomplete",
                         nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoMove | ImGuiWindowFlags_Tooltip))
        {
            for (int i = 0; i < m_autocompleteSuggestions.size(); ++i)
            {
                bool isSelected = (i == m_selectedSuggestion);

                if (isSelected)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                }

                if (ImGui::Selectable(m_autocompleteSuggestions[i].c_str(), isSelected))
                {
                    // Insert the selected suggestion
                    m_expression += m_autocompleteSuggestions[i];
                    strncpy(m_expressionBuffer, m_expression.c_str(), EXPRESSION_BUFFER_SIZE - 1);
                    m_expressionBuffer[EXPRESSION_BUFFER_SIZE - 1] = '\0';
                    m_needsValidation = true;
                    m_showAutocomplete = false;
                }

                if (isSelected)
                {
                    ImGui::PopStyleColor();
                }
            }
        }
        ImGui::End();
    }

    std::vector<std::string> ExpressionDialog::getAutocompleteSuggestions() const
    {
        std::vector<std::string> suggestions;

        if (m_expression.empty())
        {
            return suggestions;
        }

        // Get the last word/token from the expression
        std::string lastToken;
        for (int i = m_expression.length() - 1; i >= 0; --i)
        {
            char c = m_expression[i];
            if (std::isalnum(c) || c == '_')
            {
                lastToken = c + lastToken;
            }
            else
            {
                break;
            }
        }

        if (lastToken.empty())
        {
            return suggestions;
        }

        // Function suggestions
        std::vector<std::string> functions = {
          "sin(",  "cos(",   "tan(",   "asin(",  "acos(", "atan(", "atan2(", "sinh(", "cosh(",
          "tanh(", "exp(",   "log(",   "log10(", "log2(", "sqrt(", "abs(",   "sign(", "floor(",
          "ceil(", "round(", "fract(", "pow(",   "min(",  "max(",  "fmod("};

        for (const auto & func : functions)
        {
            std::string funcName = func.substr(0, func.find('('));
            if (funcName.find(lastToken) == 0) // starts with lastToken
            {
                suggestions.push_back(func.substr(lastToken.length()));
            }
        }

        // Argument suggestions (if we have vector arguments, suggest component access)
        for (const auto & arg : m_arguments)
        {
            if (arg.type == ArgumentType::Vector)
            {
                if (arg.name.find(lastToken) == 0)
                {
                    suggestions.push_back(".x");
                    suggestions.push_back(".y");
                    suggestions.push_back(".z");
                }
                else if ((arg.name + ".").find(lastToken) == 0)
                {
                    std::string remaining = (arg.name + ".").substr(lastToken.length());
                    if (remaining == "x" || remaining == "y" || remaining == "z")
                    {
                        suggestions.push_back(remaining);
                    }
                }
            }
            else if (arg.name.find(lastToken) == 0) // Scalar argument
            {
                suggestions.push_back(arg.name.substr(lastToken.length()));
            }
        }

        // Common mathematical constants
        std::vector<std::string> constants = {"pi", "e"};
        for (const auto & constant : constants)
        {
            if (constant.find(lastToken) == 0)
            {
                suggestions.push_back(constant.substr(lastToken.length()));
            }
        }

        return suggestions;
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
            std::cout << "Create Function button clicked!" << std::endl;
            std::cout << "canApply: " << canApply << std::endl;
            std::cout << "m_isValid: " << m_isValid << std::endl;
            std::cout << "hasValidExpression: " << m_parser->hasValidExpression() << std::endl;
            std::cout << "functionName empty: " << m_functionName.empty() << std::endl;
            std::cout << "m_onApplyCallback set: " << (m_onApplyCallback ? "true" : "false")
                      << std::endl;

            if (m_onApplyCallback && canApply)
            {
                // Update output name from buffer
                m_output.name = std::string(m_outputNameBuffer);

                std::cout << "Calling apply callback with: " << m_functionName << ", "
                          << m_expression << ", output: " << m_output.name << " ("
                          << (m_output.type == ArgumentType::Scalar ? "Scalar" : "Vector") << ")"
                          << std::endl;
                m_onApplyCallback(m_functionName, m_expression, m_arguments, m_output);
                hide(); // Close dialog after applying
            }
            else
            {
                std::cout << "Apply callback not called - callback: "
                          << (m_onApplyCallback ? "set" : "not set") << ", canApply: " << canApply
                          << std::endl;
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

    void ExpressionDialog::renderOutputSection()
    {
        ImGui::Text("Function Output:");

        // Output name input
        ImGui::PushItemWidth(200);
        if (ImGui::InputText("Output Name", m_outputNameBuffer, OUTPUT_NAME_BUFFER_SIZE))
        {
            m_output.name = std::string(m_outputNameBuffer);
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(default: result)");

        // Display deduced output type
        ImGui::Text("Output Type: ");
        ImGui::SameLine();

        if (m_isValid)
        {
            ArgumentType deducedType = deduceOutputType();
            if (deducedType == ArgumentType::Scalar)
            {
                ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Scalar");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(single value)");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.0f, 0.6f, 0.8f, 1.0f), "Vector");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(x, y, z components)");
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "Unknown");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(validate expression first)");
        }
    }

    ArgumentType ExpressionDialog::deduceOutputType() const
    {
        // For now, implement basic type deduction rules
        // Vector operations that return vectors:
        // 1. Vector component access patterns don't create vectors (they extract scalars)
        // 2. Direct vector arguments would return vectors, but most math operations return scalars
        // 3. Only specific vector construction operations would return vectors

        std::string expr = m_expression;

        // Check for vector component access (e.g., "pos.x", "A.y") - these return scalars
        if (expr.find('.') != std::string::npos)
        {
            return ArgumentType::Scalar;
        }

        // Check for vector arguments used directly (without component access)
        for (const auto & arg : m_arguments)
        {
            if (arg.type == ArgumentType::Vector)
            {
                // If we have a vector argument name used directly (not with .x, .y, .z)
                size_t pos = expr.find(arg.name);
                if (pos != std::string::npos)
                {
                    // Check if it's followed by a component accessor
                    if (pos + arg.name.length() < expr.length() &&
                        expr[pos + arg.name.length()] == '.')
                    {
                        continue; // This is component access, keep looking
                    }
                    else
                    {
                        // Vector used directly - this could be a vector operation
                        // For simplicity, most math operations on vectors return scalars
                        // Only return Vector if it's a simple pass-through
                        if (expr == arg.name)
                        {
                            return ArgumentType::Vector;
                        }
                    }
                }
            }
        }

        // Most mathematical expressions return scalars
        return ArgumentType::Scalar;
    }

} // namespace gladius::ui
