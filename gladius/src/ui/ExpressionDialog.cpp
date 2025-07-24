#include "ExpressionDialog.h"
#include "../ExpressionParser.h"

#include "imgui.h"
#include <fmt/format.h>
#include <algorithm>

namespace gladius::ui
{
    ExpressionDialog::ExpressionDialog()
        : m_parser(std::make_unique<ExpressionParser>())
    {
    }

    ExpressionDialog::~ExpressionDialog() = default;

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

        ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
        
        if (ImGui::Begin("Mathematical Expression Editor", &m_visible, 
                        ImGuiWindowFlags_NoCollapse))
        {
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

    void ExpressionDialog::setExpression(std::string const& expression)
    {
        m_expression = expression;
        
        // Copy to ImGui buffer, ensuring null termination
        size_t copySize = std::min(expression.length(), EXPRESSION_BUFFER_SIZE - 1);
        std::copy(expression.begin(), expression.begin() + copySize, m_expressionBuffer);
        m_expressionBuffer[copySize] = '\0';
        
        m_needsValidation = true;
    }

    std::string const& ExpressionDialog::getExpression() const
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

    void ExpressionDialog::renderExpressionInput()
    {
        ImGui::Text("Enter Mathematical Expression:");
        ImGui::PushItemWidth(-1); // Full width
        
        if (ImGui::InputTextMultiline("##expression", m_expressionBuffer, 
                                     EXPRESSION_BUFFER_SIZE, ImVec2(-1, 80)))
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
        
        for (std::string const& var : variables)
        {
            ImGui::BulletText("%s", var.c_str());
        }
        
        ImGui::Unindent();
        
        if (variables.size() > 3)
        {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), 
                              "Note: Many variables detected. Consider using fewer for better performance.");
        }
    }

    void ExpressionDialog::renderButtons()
    {
        ImGui::Spacing();
        
        // Apply button - only enabled if expression is valid
        bool canApply = m_isValid && m_parser->hasValidExpression();
        
        if (!canApply)
        {
            ImGui::BeginDisabled();
        }
        
        if (ImGui::Button("Apply", ImVec2(100, 0)))
        {
            if (m_onApplyCallback && canApply)
            {
                m_onApplyCallback(m_expression);
                hide(); // Close dialog after applying
            }
        }
        
        if (!canApply)
        {
            ImGui::EndDisabled();
        }
        
        ImGui::SameLine();
        
        // Preview button - enabled if expression is valid
        if (!canApply)
        {
            ImGui::BeginDisabled();
        }
        
        if (ImGui::Button("Preview", ImVec2(100, 0)))
        {
            if (m_onPreviewCallback && canApply)
            {
                m_onPreviewCallback(m_expression);
            }
        }
        
        if (!canApply)
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
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                          "Supported functions: sin, cos, tan, exp, log, sqrt, abs, etc.");
    }

} // namespace gladius::ui
