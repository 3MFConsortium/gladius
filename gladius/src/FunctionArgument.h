#pragma once

#include <string>
#include <vector>

namespace gladius
{
    /**
     * @brief Represents the type of a function argument
     */
    enum class ArgumentType
    {
        Scalar, // float
        Vector  // float3 (x, y, z)
    };

    /**
     * @brief Represents a function argument with name and type
     */
    struct FunctionArgument
    {
        std::string name;
        ArgumentType type;

        FunctionArgument() = default;
        FunctionArgument(std::string const & argName, ArgumentType argType)
            : name(argName)
            , type(argType)
        {
        }
    };

    /**
     * @brief Represents a function output with name and type
     */
    struct FunctionOutput
    {
        std::string name;
        ArgumentType type;

        FunctionOutput() = default;
        FunctionOutput(std::string const & outputName, ArgumentType outputType)
            : name(outputName)
            , type(outputType)
        {
        }

        // Default constructor with "result" name and Scalar type
        static FunctionOutput defaultOutput()
        {
            return FunctionOutput("result", ArgumentType::Scalar);
        }
    };

    /**
     * @brief Represents a component access (e.g., A.x, B.y)
     */
    struct ComponentAccess
    {
        std::string argumentName; // e.g., "A"
        std::string component;    // e.g., "x", "y", "z"

        ComponentAccess() = default;
        ComponentAccess(std::string const & argName, std::string const & comp)
            : argumentName(argName)
            , component(comp)
        {
        }
    };

    /**
     * @brief Utility functions for working with function arguments
     */
    class ArgumentUtils
    {
      public:
        /**
         * @brief Check if a string is a valid component name
         * @param component The component string to validate
         * @return true if component is "x", "y", or "z"
         */
        static bool isValidComponent(std::string const & component);

        /**
         * @brief Parse a component access string (e.g., "A.x" -> ArgumentName="A", Component="x")
         * @param expression The expression string to parse
         * @return ComponentAccess if valid, empty ComponentAccess if invalid
         */
        static ComponentAccess parseComponentAccess(std::string const & expression);

        /**
         * @brief Check if an expression contains component access syntax
         * @param expression The expression to check
         * @return true if expression contains patterns like "name.component"
         */
        static bool hasComponentAccess(std::string const & expression);

        /**
         * @brief Get string representation of ArgumentType
         * @param type The argument type
         * @return String representation ("Scalar" or "Vector")
         */
        static std::string argumentTypeToString(ArgumentType type);

        /**
         * @brief Convert string to ArgumentType
         * @param typeStr The string representation
         * @return ArgumentType, defaults to Scalar if invalid
         */
        static ArgumentType stringToArgumentType(std::string const & typeStr);

        /**
         * @brief Validate argument name (no conflicts with built-in functions/variables)
         * @param name The argument name to validate
         * @return true if name is valid for use as argument
         */
        static bool isValidArgumentName(std::string const & name);
    };

} // namespace gladius
