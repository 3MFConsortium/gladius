#pragma once

#include <memory>
#include <string>
#include <functional>

namespace gladius
{
    class ExpressionParser;
}

namespace gladius::ui
{
    /**
     * @brief Dialog for entering and validating mathematical expressions
     * 
     * Allows users to input mathematical expressions, validates them in real-time,
     * and provides feedback about syntax errors or variable extraction.
     */
    class ExpressionDialog
    {
    public:
        using OnApplyCallback = std::function<void(std::string const& expression)>;
        using OnPreviewCallback = std::function<void(std::string const& expression)>;

        /**
         * @brief Construct a new ExpressionDialog
         */
        ExpressionDialog();

        /**
         * @brief Destroy the ExpressionDialog
         */
        ~ExpressionDialog();

        /**
         * @brief Show the expression dialog
         */
        void show();

        /**
         * @brief Hide the expression dialog
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

        /**
         * @brief Set the initial expression text
         * @param expression The expression to display in the dialog
         */
        void setExpression(std::string const& expression);

        /**
         * @brief Get the current expression text
         * @return The current expression string
         */
        std::string const& getExpression() const;

        /**
         * @brief Set callback for when Apply button is clicked
         * @param callback Function to call with the validated expression
         */
        void setOnApplyCallback(OnApplyCallback callback);

        /**
         * @brief Set callback for when Preview button is clicked
         * @param callback Function to call with the current expression
         */
        void setOnPreviewCallback(OnPreviewCallback callback);

    private:
        /**
         * @brief Validate the current expression and update UI state
         */
        void validateExpression();

        /**
         * @brief Render the expression input field
         */
        void renderExpressionInput();

        /**
         * @brief Render the validation status and error messages
         */
        void renderValidationStatus();

        /**
         * @brief Render the variables found in the expression
         */
        void renderVariablesList();

        /**
         * @brief Render the dialog buttons (Apply, Cancel, Preview)
         */
        void renderButtons();

        std::unique_ptr<ExpressionParser> m_parser;
        bool m_visible = false;
        std::string m_expression;
        std::string m_lastValidatedExpression;
        bool m_isValid = false;
        bool m_needsValidation = true;
        
        // Callbacks
        OnApplyCallback m_onApplyCallback;
        OnPreviewCallback m_onPreviewCallback;
        
        // Input buffer for ImGui
        static constexpr size_t EXPRESSION_BUFFER_SIZE = 1024;
        char m_expressionBuffer[EXPRESSION_BUFFER_SIZE] = {0};
        
        // Disable copy and move
        ExpressionDialog(ExpressionDialog const&) = delete;
        ExpressionDialog& operator=(ExpressionDialog const&) = delete;
        ExpressionDialog(ExpressionDialog&&) = delete;
        ExpressionDialog& operator=(ExpressionDialog&&) = delete;
    };

} // namespace gladius::ui
