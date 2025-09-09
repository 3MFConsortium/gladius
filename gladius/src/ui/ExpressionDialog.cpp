#include "ExpressionDialog.h"
#include "../ExpressionParser.h"
#include "../ExpressionToGraphConverter.h"
#include "../FunctionArgument.h"

#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <fmt/format.h>
#include <iostream>
#include <memory>

namespace gladius::ui
{
    ExpressionDialog::ExpressionDialog()
        : m_parser(std::make_unique<ExpressionParser>())
        , m_output(FunctionOutput::defaultOutput())
    {
        // Initialize output name buffer
        strncpy_s(m_outputNameBuffer, sizeof(m_outputNameBuffer), m_output.name.c_str(), _TRUNCATE);
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

        // Slightly larger default size for better UX
        ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Create Mathematical Function", &m_visible, ImGuiWindowFlags_NoCollapse))
        {
            // Main content with better spacing
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 12));

            renderFunctionBasics();
            ImGui::Separator();
            renderExpressionEditor();
            ImGui::Separator();
            renderSmartAssistant();
            ImGui::Separator();
            renderActionButtons();

            ImGui::PopStyleVar();
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

        // Check if the expression can be converted to a graph
        // This includes both standard parser validation AND function calls with component access
        m_isValid = ExpressionToGraphConverter::canConvertToGraph(m_expression, *m_parser);

        // Update output type based on expression (regardless of parser validity)
        // Our type deduction is more sophisticated than the basic expression parser
        m_output.type = deduceOutputType();
    }

    void ExpressionDialog::renderArgumentsSection()
    {
        ImGui::TextUnformatted("Input Parameters");

        // Show template notifications
        renderTemplateNotifications();

        if (m_arguments.empty())
        {
            ImGui::TextColored(
              ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
              "No parameters defined - this function will work with constants only");
        }
        else
        {
            // Compact argument display
            ImGui::Indent();
            for (size_t i = 0; i < m_arguments.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));

                // Argument name with type indicator
                std::string typeIcon =
                  (m_arguments[i].type == ArgumentType::Vector) ? "[Vec]" : "[Num]";
                std::string argDisplay =
                  fmt::format("{} {} ({})",
                              typeIcon,
                              m_arguments[i].name,
                              ArgumentUtils::argumentTypeToString(m_arguments[i].type));

                ImGui::TextUnformatted(argDisplay.c_str());
                ImGui::SameLine();

                // Small remove button
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
                if (ImGui::Button("×"))
                {
                    m_argumentToRemove = static_cast<int>(i);
                }
                ImGui::PopStyleVar();

                ImGui::PopID();
            }
            ImGui::Unindent();
        }

        // Quick add section
        ImGui::Spacing();

        // Inline parameter addition
        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##newArgName", m_newArgumentNameBuffer, ARGUMENT_NAME_BUFFER_SIZE);
        ImGui::SameLine();

        ImGui::SetNextItemWidth(80);
        const char * typeItems[] = {"Scalar", "Vector"};
        ImGui::Combo("##argType", &m_selectedArgumentType, typeItems, IM_ARRAYSIZE(typeItems));
        ImGui::SameLine();

        // Validate and show add button
        std::string newArgName = m_newArgumentNameBuffer;
        bool canAdd = !newArgName.empty() && ArgumentUtils::isValidArgumentName(newArgName) &&
                      std::find_if(m_arguments.begin(),
                                   m_arguments.end(),
                                   [&newArgName](FunctionArgument const & arg)
                                   { return arg.name == newArgName; }) == m_arguments.end();

        if (!canAdd && !newArgName.empty())
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("+ Add"))
        {
            ArgumentType type =
              (m_selectedArgumentType == 0) ? ArgumentType::Scalar : ArgumentType::Vector;
            m_arguments.emplace_back(newArgName, type);
            m_newArgumentNameBuffer[0] = '\0';
            m_needsValidation = true;
        }

        if (!canAdd && !newArgName.empty())
        {
            ImGui::EndDisabled();
        }

        // Show validation error inline
        if (!newArgName.empty() && !canAdd)
        {
            ImGui::SameLine();
            if (!ArgumentUtils::isValidArgumentName(newArgName))
            {
                ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "Invalid name");
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Argument names must be valid identifiers\n(letters, "
                                      "numbers, underscore; can't start with number)");
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "Already exists");
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("An argument with this name already exists.\nArgument names "
                                      "must be unique within a function.");
                }
            }
        }

        // Handle removal
        if (m_argumentToRemove >= 0 && m_argumentToRemove < static_cast<int>(m_arguments.size()))
        {
            m_arguments.erase(m_arguments.begin() + m_argumentToRemove);
            m_argumentToRemove = -1;
            m_needsValidation = true;
        }

        // Compact help
        if (m_arguments.empty())
        {
            ImGui::TextColored(
              ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
              "[Num] Scalar: single number • [Vec] Vector: pos.x, pos.y, pos.z components");
        }
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
                                         ? static_cast<int>(m_selectedSuggestion - 1)
                                         : static_cast<int>(m_autocompleteSuggestions.size() - 1);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Tab) || ImGui::IsKeyPressed(ImGuiKey_Enter))
            {
                if (m_selectedSuggestion >= 0 &&
                    m_selectedSuggestion < m_autocompleteSuggestions.size())
                {
                    // Insert the selected suggestion
                    std::string suggestion = m_autocompleteSuggestions[m_selectedSuggestion];
                    m_expression += suggestion;
                    strncpy_s(m_expressionBuffer, sizeof(m_expressionBuffer), m_expression.c_str(), _TRUNCATE);
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

        // Position the popup near the text cursor
        ImVec2 textPos = ImGui::GetItemRectMin();
        ImVec2 textSize = ImGui::GetItemRectSize();

        // Try to position near cursor, fallback to below text input
        ImGui::SetNextWindowPos(ImVec2(textPos.x, textPos.y + textSize.y + 2));
        ImGui::SetNextWindowSize(ImVec2(std::max(300.0f, textSize.x * 0.8f), 0));

        ImGuiWindowFlags popupFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_Tooltip |
                                      ImGuiWindowFlags_NoFocusOnAppearing;

        if (ImGui::Begin("##autocomplete", nullptr, popupFlags))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));

            // Header
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                               "Autocomplete (Tab or Enter to insert):");
            ImGui::Separator();

            for (int i = 0; i < m_autocompleteSuggestions.size() && i < 10;
                 ++i) // Limit to 10 suggestions
            {
                bool isSelected = (i == m_selectedSuggestion);

                // Color coding for different types of suggestions
                ImVec4 itemColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                std::string prefix = "";

                std::string suggestion = m_autocompleteSuggestions[i];
                if (suggestion.find("(") != std::string::npos)
                {
                    itemColor = ImVec4(0.4f, 0.8f, 0.9f, 1.0f); // Blue for functions
                    prefix = "[Fn] ";
                }
                else if (suggestion.find(".") != std::string::npos)
                {
                    itemColor = ImVec4(0.8f, 0.4f, 0.8f, 1.0f); // Purple for components
                    prefix = "[Vec] ";
                }
                else
                {
                    itemColor = ImVec4(0.7f, 0.9f, 0.7f, 1.0f); // Green for variables
                    prefix = "[Var] ";
                }

                if (isSelected)
                {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.6f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.4f, 0.7f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.2f, 0.2f, 0.5f, 0.8f));
                }

                std::string displayText = prefix + suggestion;
                if (ImGui::Selectable(
                      displayText.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    // Insert the selected suggestion
                    insertAutocompleteSuggestion(suggestion);
                }

                if (isSelected)
                {
                    ImGui::PopStyleColor(3);

                    // Show tooltip with description for functions
                    if (suggestion.find("(") != std::string::npos)
                    {
                        std::string funcName = suggestion.substr(0, suggestion.find('('));
                        std::string description = getFunctionDescription(funcName);
                        if (!description.empty())
                        {
                            ImGui::SameLine();
                            ImGui::TextColored(
                              ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "- %s", description.c_str());
                        }
                    }
                }
            }

            ImGui::PopStyleVar(2);
        }
        ImGui::End();
    }

    void ExpressionDialog::insertAutocompleteSuggestion(std::string const & suggestion)
    {
        // Find the cursor position and insert suggestion
        size_t cursorPos = strlen(m_expressionBuffer);

        // Find the start of the current word
        size_t wordStart = cursorPos;
        while (wordStart > 0 && (std::isalnum(m_expressionBuffer[wordStart - 1]) ||
                                 m_expressionBuffer[wordStart - 1] == '_'))
        {
            wordStart--;
        }

        // Replace from wordStart to cursor with suggestion
        std::string beforeWord = m_expression.substr(0, wordStart);
        std::string afterCursor = m_expression.substr(cursorPos);

        m_expression = beforeWord + suggestion + afterCursor;
        strncpy_s(m_expressionBuffer, sizeof(m_expressionBuffer), m_expression.c_str(), _TRUNCATE);

        m_needsValidation = true;
        m_needsSyntaxUpdate = true;
        m_showAutocomplete = false;
    }

    std::string ExpressionDialog::getFunctionDescription(std::string const & funcName) const
    {
        static std::map<std::string, std::string> descriptions = {
          {"sin", "Sine function"},
          {"cos", "Cosine function"},
          {"tan", "Tangent function"},
          {"exp", "Exponential function (e^x)"},
          {"log", "Natural logarithm"},
          {"sqrt", "Square root"},
          {"abs", "Absolute value"},
          {"pow", "Power function (x^y)"},
          {"min", "Minimum of two values"},
          {"max", "Maximum of two values"},
          {"clamp", "Clamp value between min and max"},
          {"floor", "Floor function (round down)"},
          {"ceil", "Ceiling function (round up)"},
          {"round", "Round to nearest integer"}};

        auto it = descriptions.find(funcName);
        return (it != descriptions.end()) ? it->second : "";
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
        for (int i = static_cast<int>(m_expression.length()) - 1; i >= 0; --i)
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

        // Enhanced function suggestions with descriptions
        std::vector<std::pair<std::string, std::string>> functions = {
          {"sin(", "Sine function"},
          {"cos(", "Cosine function"},
          {"tan(", "Tangent function"},
          {"asin(", "Arc sine"},
          {"acos(", "Arc cosine"},
          {"atan(", "Arc tangent"},
          {"atan2(", "Two-argument arc tangent"},
          {"exp(", "Exponential function (e^x)"},
          {"log(", "Natural logarithm"},
          {"log10(", "Base-10 logarithm"},
          {"sqrt(", "Square root"},
          {"abs(", "Absolute value"},
          {"pow(", "Power function (x^y)"},
          {"min(", "Minimum of two values"},
          {"max(", "Maximum of two values"},
          {"clamp(", "Clamp value between min and max"},
          {"floor(", "Floor function"},
          {"ceil(", "Ceiling function"},
          {"round(", "Round to nearest integer"}};

        for (const auto & func : functions)
        {
            std::string funcName = func.first.substr(0, func.first.find('('));
            if (funcName.find(lastToken) == 0)
            {
                suggestions.push_back(func.first.substr(lastToken.length()));
            }
        }

        // Smart argument suggestions
        for (const auto & arg : m_arguments)
        {
            if (arg.type == ArgumentType::Vector)
            {
                if (arg.name.find(lastToken) == 0)
                {
                    suggestions.push_back(arg.name.substr(lastToken.length()));
                    if (arg.name == lastToken) // Exact match, suggest components
                    {
                        suggestions.push_back(".x");
                        suggestions.push_back(".y");
                        suggestions.push_back(".z");
                    }
                }
            }
            else if (arg.name.find(lastToken) == 0)
            {
                suggestions.push_back(arg.name.substr(lastToken.length()));
            }
        }

        // Mathematical constants
        std::vector<std::string> constants = {"pi", "e"};
        for (const auto & constant : constants)
        {
            if (constant.find(lastToken) == 0)
            {
                suggestions.push_back(constant.substr(lastToken.length()));
            }
        }

        // Remove duplicates and sort
        std::sort(suggestions.begin(), suggestions.end());
        suggestions.erase(std::unique(suggestions.begin(), suggestions.end()), suggestions.end());

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
        // For function calls with component access, we don't require parser.hasValidExpression()
        bool hasParserValidExpression = m_parser->hasValidExpression();
        bool isFunctionWithComponent =
          ExpressionToGraphConverter::canConvertToGraph(m_expression, *m_parser) &&
          !hasParserValidExpression;
        bool canApply = m_isValid && (hasParserValidExpression || isFunctionWithComponent) &&
                        !m_functionName.empty();

        if (!canApply)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Create Function", ImVec2(120, 0)))
        {

            if (m_onApplyCallback && canApply)
            {
                // Update output name from buffer
                m_output.name = std::string(m_outputNameBuffer);
                m_onApplyCallback(m_functionName, m_expression, m_arguments, m_output);
                hide(); // Close dialog after applying
            }
        }

        if (!canApply)
        {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        // Preview button - enabled if expression is valid
        // For function calls with component access, we don't require parser.hasValidExpression()
        bool canPreview = m_isValid && (hasParserValidExpression || isFunctionWithComponent);

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
        ImGui::TextUnformatted("Output");

        // Compact output configuration
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("Name##output", m_outputNameBuffer, OUTPUT_NAME_BUFFER_SIZE))
        {
            m_output.name = std::string(m_outputNameBuffer);
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(default: result)");

        // Auto-detected type with icon
        if (m_isValid)
        {
            ArgumentType deducedType = deduceOutputType();
            std::string typeText =
              (deducedType == ArgumentType::Scalar) ? "Single value" : "Vector (x,y,z)";
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Type: %s", typeText.c_str());
        }
        else
        {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Type: Auto-detected");
        }
    }

    ArgumentType ExpressionDialog::deduceOutputType() const
    {
        std::string expr = m_expression;

        // Remove whitespace for easier parsing
        expr.erase(std::remove_if(expr.begin(), expr.end(), ::isspace), expr.end());

        // Check for component access at the end of the expression (e.g., "sin(pos).x")
        // This indicates we're extracting a scalar component from a vector result
        std::vector<std::string> components = {".x", ".y", ".z"};
        for (const auto & comp : components)
        {
            if (expr.length() >= comp.length() &&
                expr.substr(expr.length() - comp.length()) == comp)
            {
                return ArgumentType::Scalar;
            }
        }

        // Check for argument component access (e.g., "pos.x", "A.y")
        // Look for patterns like argumentName.component
        bool hasArgumentComponentAccess = false;
        for (const auto & arg : m_arguments)
        {
            for (const auto & comp : components)
            {
                std::string pattern = arg.name + comp;
                if (expr.find(pattern) != std::string::npos)
                {
                    hasArgumentComponentAccess = true;
                    break;
                }
            }
            if (hasArgumentComponentAccess)
                break;
        }

        // If we have argument component access, check if that's ALL we have
        if (hasArgumentComponentAccess)
        {
            // Count how many vector arguments are used directly (without component access)
            int directVectorUsage = 0;
            for (const auto & arg : m_arguments)
            {
                if (arg.type == ArgumentType::Vector)
                {
                    // Check if argument is used directly (not just in component access)
                    size_t pos = 0;
                    while ((pos = expr.find(arg.name, pos)) != std::string::npos)
                    {
                        // Check if this occurrence is NOT followed by a component accessor
                        if (pos + arg.name.length() >= expr.length() ||
                            expr[pos + arg.name.length()] != '.')
                        {
                            directVectorUsage++;
                            break;
                        }
                        pos += arg.name.length();
                    }
                }
            }

            // If we only have component access (no direct vector usage), result is scalar
            if (directVectorUsage == 0)
            {
                return ArgumentType::Scalar;
            }
        }

        // Check if expression contains vector arguments used directly
        bool hasDirectVectorArgument = false;
        for (const auto & arg : m_arguments)
        {
            if (arg.type == ArgumentType::Vector)
            {
                // Check if argument is used directly (not just in component access)
                size_t pos = 0;
                while ((pos = expr.find(arg.name, pos)) != std::string::npos)
                {
                    // Check if this occurrence is NOT followed by a component accessor
                    if (pos + arg.name.length() >= expr.length() ||
                        expr[pos + arg.name.length()] != '.')
                    {
                        hasDirectVectorArgument = true;
                        break;
                    }
                    pos += arg.name.length();
                }
                if (hasDirectVectorArgument)
                    break;
            }
        }

        // If no direct vector arguments, result is scalar
        if (!hasDirectVectorArgument)
        {
            return ArgumentType::Scalar;
        }

        // Check for vector-preserving operations (element-wise functions)
        std::vector<std::string> vectorFunctions = {"sin",
                                                    "cos",
                                                    "tan",
                                                    "asin",
                                                    "acos",
                                                    "atan",
                                                    "exp",
                                                    "log",
                                                    "sqrt",
                                                    "abs",
                                                    "floor",
                                                    "ceil",
                                                    "round",
                                                    "pow",
                                                    "+",
                                                    "-",
                                                    "*",
                                                    "/"};

        // If expression is just a vector argument name, return vector
        for (const auto & arg : m_arguments)
        {
            if (arg.type == ArgumentType::Vector && expr == arg.name)
            {
                return ArgumentType::Vector;
            }
        }

        // Check if expression uses vector-preserving functions
        bool hasVectorPreservingOperation = false;
        for (const auto & func : vectorFunctions)
        {
            if (expr.find(func) != std::string::npos)
            {
                hasVectorPreservingOperation = true;
                break;
            }
        }

        // If we have direct vector arguments and vector-preserving operations, result is vector
        if (hasDirectVectorArgument && hasVectorPreservingOperation)
        {
            return ArgumentType::Vector;
        }

        // Check for reduction operations that return scalars even with vector inputs
        std::vector<std::string> scalarFunctions = {
          "length", "dot", "cross", "magnitude", "norm", "min", "max", "sum", "mean", "distance"};

        for (const auto & func : scalarFunctions)
        {
            if (expr.find(func) != std::string::npos)
            {
                return ArgumentType::Scalar;
            }
        }

        // Default: if we have direct vector arguments, assume vector output
        // This covers cases like vector arithmetic operations
        return hasDirectVectorArgument ? ArgumentType::Vector : ArgumentType::Scalar;
    }

    void ExpressionDialog::renderFunctionBasics()
    {
        // Function name with cleaner styling
        ImGui::TextUnformatted("Function Name");
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##functionName", m_functionNameBuffer, FUNCTION_NAME_BUFFER_SIZE))
        {
            m_functionName = m_functionNameBuffer;
        }
        ImGui::PopItemWidth();

        if (m_functionName.empty())
        {
            ImGui::TextColored(
              ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
              "Give your function a descriptive name (e.g., 'wave_pattern', 'distance_field')");
        }

        ImGui::Spacing();

        // Collapsible advanced options
        if (ImGui::CollapsingHeader("Function Parameters", ImGuiTreeNodeFlags_DefaultOpen))
        {
            renderArgumentsSection();
            ImGui::Spacing();
            renderOutputSection();
        }
    }

    void ExpressionDialog::renderExpressionEditor()
    {
        ImGui::TextUnformatted("Mathematical Expression");

        // Quick template buttons
        if (ImGui::Button("Templates"))
        {
            m_showExpressionTemplates = !m_showExpressionTemplates;
        }
        ImGui::SameLine();
        if (ImGui::Button("Functions"))
        {
            // Show function reference
            ImGui::OpenPopup("FunctionReference");
        }

        // Templates section
        if (m_showExpressionTemplates)
        {
            renderExpressionTemplates();
            ImGui::Spacing();
        }

        // Function reference popup
        if (ImGui::BeginPopup("FunctionReference"))
        {
            ImGui::Text("Available Functions:");
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Basic Math:");
            ImGui::BulletText("sin(), cos(), tan(), asin(), acos(), atan()");
            ImGui::BulletText("exp(), log(), sqrt(), abs(), pow(x,y)");
            ImGui::BulletText("min(x,y), max(x,y), clamp(x,min,max)");
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Operators:");
            ImGui::BulletText("+ - * / ( )");
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Vector Components:");
            ImGui::BulletText("pos.x, pos.y, pos.z");
            ImGui::EndPopup();
        }

        // Main expression input
        renderEnhancedExpressionInput();

        // Autocomplete
        if (m_showAutocomplete)
        {
            renderAutocompleteSuggestions();
        }

        // Quick examples for empty expression
        if (m_expression.empty())
        {
            ImGui::TextColored(
              ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
              "Examples: sin(pos.x), sqrt(pos.x*pos.x + pos.y*pos.y), exp(-radius)");
        }
    }

    void ExpressionDialog::renderEnhancedExpressionInput()
    {
        ImGui::PushItemWidth(-1);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));

        // Update syntax highlighting if needed
        if (m_needsSyntaxUpdate)
        {
            applySyntaxHighlighting(m_expression, m_syntaxColors);
            m_needsSyntaxUpdate = false;
        }

        // Custom text input with syntax highlighting
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput |
                                    ImGuiInputTextFlags_CallbackCompletion |
                                    ImGuiInputTextFlags_CallbackCharFilter;

        // Enhanced text input callback
        auto callback = [](ImGuiInputTextCallbackData * data) -> int
        {
            ExpressionDialog * dialog = static_cast<ExpressionDialog *>(data->UserData);

            if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
            {
                // Handle Tab completion
                dialog->m_autocompleteSuggestions = dialog->getAutocompleteSuggestions();
                if (!dialog->m_autocompleteSuggestions.empty())
                {
                    dialog->m_showAutocomplete = true;
                    dialog->m_selectedSuggestion = 0;
                }
                return 0;
            }

            if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter)
            {
                // Mark for syntax highlighting update
                dialog->m_needsSyntaxUpdate = true;
                return 0;
            }

            return 0;
        };

        if (ImGui::InputTextMultiline("##expression",
                                      m_expressionBuffer,
                                      EXPRESSION_BUFFER_SIZE,
                                      ImVec2(-1, 100),
                                      flags,
                                      callback,
                                      this))
        {
            m_expression = m_expressionBuffer;
            m_needsValidation = true;
            m_needsSyntaxUpdate = true;
            m_autocompleteSuggestions = getAutocompleteSuggestions();
            m_showAutocomplete = !m_autocompleteSuggestions.empty();
            m_selectedSuggestion = -1;
        }

        ImGui::PopStyleVar();
        ImGui::PopItemWidth();

        // Show syntax-highlighted overlay (simplified approach)
        if (!m_expression.empty() && m_syntaxColors.size() == m_expression.length())
        {
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetItemRectSize().x);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetItemRectSize().y);

            // Note: This is a simplified approach. For full syntax highlighting,
            // we'd need a more sophisticated text rendering system
        }
    }

    void ExpressionDialog::applySyntaxHighlighting(std::string const & text,
                                                   std::vector<ImVec4> & colors)
    {
        colors.clear();
        colors.resize(text.length(), ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // Default white

        // Simple tokenization for syntax highlighting
        for (size_t i = 0; i < text.length(); ++i)
        {
            char c = text[i];

            // Numbers
            if (std::isdigit(c) || c == '.')
            {
                colors[i] = ImVec4(0.8f, 0.8f, 0.4f, 1.0f); // Yellow for numbers
            }
            // Operators
            else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '=' || c == '(' ||
                     c == ')' || c == ',' || c == '.')
            {
                colors[i] = ImVec4(0.9f, 0.6f, 0.3f, 1.0f); // Orange for operators
            }
            // Check for function names and arguments
            else if (std::isalpha(c) || c == '_')
            {
                // Extract word
                std::string word;
                size_t start = i;
                while (i < text.length() && (std::isalnum(text[i]) || text[i] == '_'))
                {
                    word += text[i];
                    ++i;
                }
                --i; // Adjust for loop increment

                ImVec4 wordColor;
                if (isMathFunction(word))
                {
                    wordColor = ImVec4(0.4f, 0.8f, 0.9f, 1.0f); // Light blue for functions
                }
                else if (isUserArgument(word))
                {
                    wordColor = ImVec4(0.8f, 0.4f, 0.8f, 1.0f); // Purple for user arguments
                }
                else
                {
                    wordColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White for unknown words
                }

                // Apply color to the entire word
                for (size_t j = start; j <= i && j < colors.size(); ++j)
                {
                    colors[j] = wordColor;
                }
            }
        }
    }

    bool ExpressionDialog::isMathFunction(std::string const & word) const
    {
        static std::vector<std::string> mathFunctions = {
          "sin",   "cos",   "tan",   "asin", "acos", "atan", "atan2", "exp",
          "log",   "log10", "sqrt",  "abs",  "pow",  "min",  "max",   "clamp",
          "floor", "ceil",  "round", "fmod", "sinh", "cosh", "tanh"};

        return std::find(mathFunctions.begin(), mathFunctions.end(), word) != mathFunctions.end();
    }

    bool ExpressionDialog::isUserArgument(std::string const & word) const
    {
        for (const auto & arg : m_arguments)
        {
            if (arg.name == word)
            {
                return true;
            }
        }
        return false;
    }

    void ExpressionDialog::insertTemplate(std::string const & templateExpr)
    {
        // Replace current expression
        m_expression = templateExpr;
        strncpy_s(m_expressionBuffer, sizeof(m_expressionBuffer), m_expression.c_str(), _TRUNCATE);

        // Auto-add expected arguments based on template
        addExpectedArgumentsForTemplate(templateExpr);

        m_needsValidation = true;
        m_needsSyntaxUpdate = true;
        m_showExpressionTemplates = false;
    }

    void ExpressionDialog::addExpectedArgumentsForTemplate(std::string const & templateExpr)
    {
        // Define expected arguments for common template patterns
        struct TemplateArgument
        {
            std::string name;
            ArgumentType type;
            std::string description;
        };

        std::vector<TemplateArgument> expectedArgs;

        // Analyze template and determine expected arguments
        if (templateExpr.find("pos.x") != std::string::npos ||
            templateExpr.find("pos.y") != std::string::npos ||
            templateExpr.find("pos.z") != std::string::npos)
        {
            expectedArgs.push_back({"pos", ArgumentType::Vector, "Position vector (x,y,z)"});
        }

        if (templateExpr.find("radius") != std::string::npos)
        {
            expectedArgs.push_back({"radius", ArgumentType::Scalar, "Sphere radius"});
        }

        if (templateExpr.find("size") != std::string::npos)
        {
            expectedArgs.push_back({"size", ArgumentType::Scalar, "Box size"});
        }

        if (templateExpr.find("frequency") != std::string::npos)
        {
            expectedArgs.push_back({"frequency", ArgumentType::Scalar, "Wave frequency"});
        }

        // Add arguments that don't already exist
        for (auto const & expectedArg : expectedArgs)
        {
            // Check if argument already exists
            bool exists = std::find_if(m_arguments.begin(),
                                       m_arguments.end(),
                                       [&expectedArg](FunctionArgument const & arg) {
                                           return arg.name == expectedArg.name;
                                       }) != m_arguments.end();

            if (!exists)
            {
                m_arguments.emplace_back(expectedArg.name, expectedArg.type);

                // Add to notification list for UI feedback
                m_autoAddedArguments.push_back({expectedArg.name, expectedArg.type});
            }
            else
            {
                std::cerr << "Argument '" << expectedArg.name << "' already exists, skipping"
                          << std::endl;

                // Check if existing argument has different type
                auto existingArg = std::find_if(m_arguments.begin(),
                                                m_arguments.end(),
                                                [&expectedArg](FunctionArgument const & arg)
                                                { return arg.name == expectedArg.name; });

                if (existingArg != m_arguments.end() && existingArg->type != expectedArg.type)
                {
                    m_argumentTypeConflicts.push_back(
                      {expectedArg.name, existingArg->type, expectedArg.type});
                }
            }
        }
    }

    void ExpressionDialog::renderTemplateNotifications()
    {
        // Show auto-added arguments
        if (!m_autoAddedArguments.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
            ImGui::Text("Auto-added arguments:");
            ImGui::PopStyleColor();

            ImGui::SameLine();
            if (ImGui::SmallButton("✓ OK"))
            {
                m_autoAddedArguments.clear();
            }

            ImGui::Indent();
            for (auto const & arg : m_autoAddedArguments)
            {
                ImGui::BulletText("%s (%s)",
                                  arg.name.c_str(),
                                  (arg.type == ArgumentType::Scalar ? "Scalar" : "Vector"));
            }
            ImGui::Unindent();
            ImGui::Spacing();
        }

        // Show type conflicts
        if (!m_argumentTypeConflicts.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.2f, 1.0f));
            ImGui::Text("Type conflicts detected:");
            ImGui::PopStyleColor();

            ImGui::SameLine();
            if (ImGui::SmallButton("✓ OK##conflicts"))
            {
                m_argumentTypeConflicts.clear();
            }

            ImGui::Indent();
            for (auto const & conflict : m_argumentTypeConflicts)
            {
                ImGui::BulletText(
                  "%s: existing %s, expected %s",
                  conflict.name.c_str(),
                  (conflict.existingType == ArgumentType::Scalar ? "Scalar" : "Vector"),
                  (conflict.expectedType == ArgumentType::Scalar ? "Scalar" : "Vector"));
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(
                      "Template expected %s type, but existing argument is %s.\nYou may need to "
                      "manually adjust the argument type.",
                      (conflict.expectedType == ArgumentType::Scalar ? "Scalar" : "Vector"),
                      (conflict.existingType == ArgumentType::Scalar ? "Scalar" : "Vector"));
                }
            }
            ImGui::Unindent();
            ImGui::Spacing();
        }
    }

    void ExpressionDialog::renderExpressionTemplates()
    {
        ImGui::Text("Quick Templates:");

        struct Template
        {
            std::string name;
            std::string expression;
            std::string description;
        };

        static std::vector<Template> templates = {
          {"Sphere",
           "sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z) - radius",
           "Distance field for a sphere\nAdds: pos (Vector), radius (Scalar)"},
          {"Wave Pattern", "sin(pos.x) * cos(pos.y)", "Sine wave pattern\nAdds: pos (Vector)"},
          {"Exponential Decay",
           "exp(-sqrt(pos.x*pos.x + pos.y*pos.y))",
           "Exponential falloff from center\nAdds: pos (Vector)"},
          {"Box Distance",
           "max(abs(pos.x), max(abs(pos.y), abs(pos.z))) - size",
           "Distance field for a box\nAdds: pos (Vector), size (Scalar)"},
          {"Ripples",
           "sin(sqrt(pos.x*pos.x + pos.y*pos.y) * frequency)",
           "Circular ripple pattern\nAdds: pos (Vector), frequency (Scalar)"},
          {"Twisted",
           "sin(pos.x + pos.y) * cos(pos.z)",
           "Twisted geometric pattern\nAdds: pos (Vector)"}};

        ImGui::Columns(3, "Templates", false);
        for (size_t i = 0; i < templates.size(); ++i)
        {
            if (ImGui::Button(templates[i].name.c_str(), ImVec2(-1, 30)))
            {
                insertTemplate(templates[i].expression);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", templates[i].description.c_str());
            }
            if ((i + 1) % 3 != 0)
                ImGui::NextColumn();
        }
        ImGui::Columns(1);
    }

    void ExpressionDialog::renderSmartAssistant()
    {
        validateExpression();

        // Status indicator with better visual feedback
        if (m_expression.empty())
        {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Enter an expression above");
            return;
        }

        // Validation status with icons
        if (m_isValid)
        {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "[OK] Expression is valid");

            // Show detected variables in a cleaner way
            if (m_parser->hasValidExpression())
            {
                std::vector<std::string> variables = m_parser->getVariables();
                if (!variables.empty())
                {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f), "Variables: ");
                    ImGui::SameLine();
                    for (size_t i = 0; i < variables.size(); ++i)
                    {
                        if (i > 0)
                        {
                            ImGui::SameLine();
                            ImGui::Text(",");
                            ImGui::SameLine();
                        }
                        ImGui::TextColored(
                          ImVec4(0.9f, 0.7f, 0.9f, 1.0f), "%s", variables[i].c_str());
                    }
                }
            }

            // Output type with better styling
            ImGui::Spacing();
            ArgumentType outputType = deduceOutputType();
            if (outputType == ArgumentType::Scalar)
            {
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 0.7f, 1.0f), "Output: Single value (Scalar)");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f),
                                   "Output: Vector (x, y, z components)");
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "[ERROR] Expression has errors");

            std::string error = m_parser->getLastError();
            if (!error.empty())
            {
                ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.6f, 1.0f), "Error: %s", error.c_str());
            }
        }
    }

    void ExpressionDialog::renderActionButtons()
    {
        ImGui::Spacing();

        // Center the buttons
        float buttonWidth = 120.0f;
        float spacing = 10.0f;
        float totalWidth = buttonWidth * 3 + spacing * 2;
        float availWidth = ImGui::GetContentRegionAvail().x;
        float offsetX = (availWidth - totalWidth) * 0.5f;
        if (offsetX > 0)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

        // Create Function button - primary action
        bool hasParserValidExpression = m_parser->hasValidExpression();
        bool isFunctionWithComponent =
          ExpressionToGraphConverter::canConvertToGraph(m_expression, *m_parser) &&
          !hasParserValidExpression;
        bool canApply = m_isValid && (hasParserValidExpression || isFunctionWithComponent) &&
                        !m_functionName.empty();

        if (canApply)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
        }
        else
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Create Function", ImVec2(buttonWidth, 35)))
        {
            if (m_onApplyCallback && canApply)
            {
                m_output.name = std::string(m_outputNameBuffer);
                m_onApplyCallback(m_functionName, m_expression, m_arguments, m_output);
                hide();
            }
        }

        if (canApply)
        {
            ImGui::PopStyleColor(3);
        }
        else
        {
            ImGui::EndDisabled();
        }

        // Preview button
        ImGui::SameLine();
        bool canPreview = m_isValid && (hasParserValidExpression || isFunctionWithComponent);

        if (!canPreview)
        {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Preview", ImVec2(buttonWidth, 35)))
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

        // Cancel button
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 35)))
        {
            hide();
        }

        // Help text at bottom
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(
          ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
          "Use templates for quick start • Press Tab for autocomplete • Hover functions for help");
    }

} // namespace gladius::ui
