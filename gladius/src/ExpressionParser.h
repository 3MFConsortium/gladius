#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>

namespace gladius
{
    /**
     * @brief Parser for mathematical expressions using muParser
     * 
     * This class handles the parsing of mathematical expressions into an internal
     * representation, which can then be converted to Gladius node graphs.
     */
    class ExpressionParser
    {
    public:
        /// Constructor
        ExpressionParser();
        
        /// Destructor
        ~ExpressionParser();

        /**
         * @brief Parse a mathematical expression string
         * @param expression The mathematical expression as a string (e.g., "x + y*2")
         * @return true if parsing was successful, false otherwise
         */
        bool parseExpression(std::string const& expression);

        /**
         * @brief Get the last error message from parsing
         * @return Error message string, empty if no error
         */
        std::string getLastError() const;

        /**
         * @brief Check if the parser has a valid parsed expression
         * @return true if there is a valid parsed expression
         */
        bool hasValidExpression() const;

        /**
         * @brief Get the string representation of the parsed expression
         * @return String representation of the expression
         */
        std::string getExpressionString() const;

        /**
         * @brief Get the variables found in the expression
         * @return Vector of variable names (e.g., ["x", "y", "z"])
         */
        std::vector<std::string> getVariables() const;

        /**
         * @brief Evaluate the expression with given variable values
         * @param variables Map of variable names to their values
         * @return The evaluated result
         */
        double evaluate(std::map<std::string, double> const& variables);

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace gladius
