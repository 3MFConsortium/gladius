#pragma once

#include "../FunctionArgument.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

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
     * Creates new functions with user-specified names.
     */
    class ExpressionDialog
    {
      public:
        using OnApplyCallback =
          std::function<void(std::string const & functionName,
                             std::string const & expression,
                             std::vector<gladius::FunctionArgument> const & arguments,
                             gladius::FunctionOutput const & output)>;
        using OnPreviewCallback = std::function<void(std::string const & expression)>;

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
         * @brief Set the initial function name
         * @param functionName The function name to display in the dialog
         */
        void setFunctionName(std::string const & functionName);

        /**
         * @brief Get the current function name
         * @return The current function name string
         */
        std::string const & getFunctionName() const;

        /**
         * @brief Set the initial expression text
         * @param expression The expression to display in the dialog
         */
        void setExpression(std::string const & expression);

        /**
         * @brief Get the current expression text
         * @return The current expression string
         */
        std::string const & getExpression() const;

        /**
         * @brief Get the current function arguments
         * @return Vector of function arguments
         */
        std::vector<gladius::FunctionArgument> const & getFunctionArguments() const;

        /**
         * @brief Set the function arguments
         * @param arguments Vector of function arguments
         */
        void setFunctionArguments(std::vector<gladius::FunctionArgument> const & arguments);

        /**
         * @brief Get the current function output
         * @return The function output specification
         */
        gladius::FunctionOutput const & getFunctionOutput() const;

        /**
         * @brief Set the function output
         * @param output The function output specification
         */
        void setFunctionOutput(gladius::FunctionOutput const & output);

        /**
         * @brief Set callback for when Apply button is clicked
         * @param callback Function to call with the function name and validated expression
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
         * @brief Render the function arguments section
         */
        void renderArgumentsSection();

        /**
         * @brief Render the function output section
         */
        void renderOutputSection();

        /**
         * @brief Deduce output type from the current expression
         * @return The deduced argument type (Scalar or Vector)
         */
        gladius::ArgumentType deduceOutputType() const;

        /**
         * @brief Render the function name input field
         */
        void renderFunctionNameInput();

        /**
         * @brief Render the expression input field with autocomplete
         */
        void renderExpressionInput();

        /**
         * @brief Show autocomplete suggestions based on current input
         */
        void renderAutocompleteSuggestions();

        /**
         * @brief Get autocomplete suggestions for the current cursor position
         * @return Vector of suggestion strings
         */
        std::vector<std::string> getAutocompleteSuggestions() const;

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
        std::string m_functionName;
        std::string m_expression;
        std::string m_lastValidatedExpression;
        bool m_isValid = false;
        bool m_needsValidation = true;

        // Function arguments
        std::vector<gladius::FunctionArgument> m_arguments;

        // Function output
        gladius::FunctionOutput m_output;

        // Callbacks
        OnApplyCallback m_onApplyCallback;
        OnPreviewCallback m_onPreviewCallback;

        // Input buffers for ImGui
        static constexpr size_t FUNCTION_NAME_BUFFER_SIZE = 256;
        static constexpr size_t EXPRESSION_BUFFER_SIZE = 1024;
        static constexpr size_t ARGUMENT_NAME_BUFFER_SIZE = 64;
        static constexpr size_t OUTPUT_NAME_BUFFER_SIZE = 64;
        char m_functionNameBuffer[FUNCTION_NAME_BUFFER_SIZE] = {0};
        char m_expressionBuffer[EXPRESSION_BUFFER_SIZE] = {0};
        char m_newArgumentNameBuffer[ARGUMENT_NAME_BUFFER_SIZE] = {0};
        char m_outputNameBuffer[OUTPUT_NAME_BUFFER_SIZE] = {0};

        // UI state for argument editing
        int m_selectedArgumentType = 0; // 0 = Scalar, 1 = Vector
        int m_argumentToRemove = -1;

        // Autocomplete state
        bool m_showAutocomplete = false;
        std::vector<std::string> m_autocompleteSuggestions;
        int m_selectedSuggestion = -1; // Disable copy and move
        ExpressionDialog(ExpressionDialog const &) = delete;
        ExpressionDialog & operator=(ExpressionDialog const &) = delete;
        ExpressionDialog(ExpressionDialog &&) = delete;
        ExpressionDialog & operator=(ExpressionDialog &&) = delete;
    };

} // namespace gladius::ui
