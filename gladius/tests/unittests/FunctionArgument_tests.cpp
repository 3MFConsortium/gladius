#include "FunctionArgument.h"
#include <gtest/gtest.h>

namespace gladius::tests
{
    class FunctionArgumentTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            // Setup any common test fixtures if needed
        }
    };

    // ArgumentUtils::isValidComponent tests
    TEST_F(FunctionArgumentTest, IsValidComponent_ValidComponents_ReturnsTrue)
    {
        EXPECT_TRUE(ArgumentUtils::isValidComponent("x"));
        EXPECT_TRUE(ArgumentUtils::isValidComponent("y"));
        EXPECT_TRUE(ArgumentUtils::isValidComponent("z"));
    }

    TEST_F(FunctionArgumentTest, IsValidComponent_InvalidComponents_ReturnsFalse)
    {
        EXPECT_FALSE(ArgumentUtils::isValidComponent("w"));
        EXPECT_FALSE(ArgumentUtils::isValidComponent("a"));
        EXPECT_FALSE(ArgumentUtils::isValidComponent(""));
        EXPECT_FALSE(ArgumentUtils::isValidComponent("X")); // Case sensitive
        EXPECT_FALSE(ArgumentUtils::isValidComponent("xy"));
        EXPECT_FALSE(ArgumentUtils::isValidComponent("1"));
    }

    // ArgumentUtils::parseComponentAccess tests
    TEST_F(FunctionArgumentTest, ParseComponentAccess_ValidComponentAccess_ReturnsCorrectComponents)
    {
        auto result = ArgumentUtils::parseComponentAccess("pos.x");
        EXPECT_EQ(result.argumentName, "pos");
        EXPECT_EQ(result.component, "x");

        result = ArgumentUtils::parseComponentAccess("normal.y");
        EXPECT_EQ(result.argumentName, "normal");
        EXPECT_EQ(result.component, "y");

        result = ArgumentUtils::parseComponentAccess("velocity.z");
        EXPECT_EQ(result.argumentName, "velocity");
        EXPECT_EQ(result.component, "z");
    }

    TEST_F(FunctionArgumentTest, ParseComponentAccess_InvalidComponentAccess_ReturnsEmpty)
    {
        auto result = ArgumentUtils::parseComponentAccess("pos.w"); // Invalid component
        EXPECT_TRUE(result.argumentName.empty());
        EXPECT_TRUE(result.component.empty());

        result = ArgumentUtils::parseComponentAccess("pos"); // No dot
        EXPECT_TRUE(result.argumentName.empty());
        EXPECT_TRUE(result.component.empty());

        result = ArgumentUtils::parseComponentAccess(".x"); // No argument name
        EXPECT_TRUE(result.argumentName.empty());
        EXPECT_TRUE(result.component.empty());

        result = ArgumentUtils::parseComponentAccess("pos."); // No component
        EXPECT_TRUE(result.argumentName.empty());
        EXPECT_TRUE(result.component.empty());

        result = ArgumentUtils::parseComponentAccess("pos.x.y"); // Multiple dots
        EXPECT_TRUE(result.argumentName.empty());
        EXPECT_TRUE(result.component.empty());
    }

    // ArgumentUtils::hasComponentAccess tests
    TEST_F(FunctionArgumentTest, HasComponentAccess_ExpressionWithComponents_ReturnsTrue)
    {
        EXPECT_TRUE(ArgumentUtils::hasComponentAccess("pos.x"));
        EXPECT_TRUE(ArgumentUtils::hasComponentAccess("pos.x + vel.y"));
        EXPECT_TRUE(
          ArgumentUtils::hasComponentAccess("sqrt(normal.x * normal.x + normal.y * normal.y)"));
        EXPECT_TRUE(ArgumentUtils::hasComponentAccess("sin(angle.z)"));
    }

    TEST_F(FunctionArgumentTest, HasComponentAccess_ExpressionWithoutComponents_ReturnsFalse)
    {
        EXPECT_FALSE(ArgumentUtils::hasComponentAccess("x + y"));
        EXPECT_FALSE(ArgumentUtils::hasComponentAccess("sin(angle)"));
        EXPECT_FALSE(ArgumentUtils::hasComponentAccess("sqrt(x * x + y * y)"));
        EXPECT_FALSE(ArgumentUtils::hasComponentAccess("42"));
        EXPECT_FALSE(ArgumentUtils::hasComponentAccess(""));
    }

    // ArgumentUtils::argumentTypeToString tests
    TEST_F(FunctionArgumentTest, ArgumentTypeToString_ValidTypes_ReturnsCorrectStrings)
    {
        EXPECT_EQ(ArgumentUtils::argumentTypeToString(ArgumentType::Scalar), "Scalar");
        EXPECT_EQ(ArgumentUtils::argumentTypeToString(ArgumentType::Vector), "Vector");
    }

    // ArgumentUtils::stringToArgumentType tests
    TEST_F(FunctionArgumentTest, StringToArgumentType_ValidStrings_ReturnsCorrectTypes)
    {
        EXPECT_EQ(ArgumentUtils::stringToArgumentType("Scalar"), ArgumentType::Scalar);
        EXPECT_EQ(ArgumentUtils::stringToArgumentType("Vector"), ArgumentType::Vector);
    }

    TEST_F(FunctionArgumentTest, StringToArgumentType_InvalidStrings_ReturnsScalarDefault)
    {
        EXPECT_EQ(ArgumentUtils::stringToArgumentType("Invalid"), ArgumentType::Scalar);
        EXPECT_EQ(ArgumentUtils::stringToArgumentType(""), ArgumentType::Scalar);
        EXPECT_EQ(ArgumentUtils::stringToArgumentType("scalar"),
                  ArgumentType::Scalar); // Case sensitive
        EXPECT_EQ(ArgumentUtils::stringToArgumentType("vector"),
                  ArgumentType::Scalar); // Case sensitive
    }

    // ArgumentUtils::isValidArgumentName tests
    TEST_F(FunctionArgumentTest, IsValidArgumentName_ValidNames_ReturnsTrue)
    {
        EXPECT_TRUE(ArgumentUtils::isValidArgumentName("pos"));
        EXPECT_TRUE(ArgumentUtils::isValidArgumentName("position"));
        EXPECT_TRUE(ArgumentUtils::isValidArgumentName("velocity"));
        EXPECT_TRUE(ArgumentUtils::isValidArgumentName("normal"));
        EXPECT_TRUE(ArgumentUtils::isValidArgumentName("scale"));
        EXPECT_TRUE(ArgumentUtils::isValidArgumentName("offset"));
        EXPECT_TRUE(ArgumentUtils::isValidArgumentName("myVar"));
        EXPECT_TRUE(ArgumentUtils::isValidArgumentName("var_name"));
        EXPECT_TRUE(ArgumentUtils::isValidArgumentName("var123"));
    }

    TEST_F(FunctionArgumentTest, IsValidArgumentName_ReservedFunctionNames_ReturnsFalse)
    {
        // Test built-in mathematical functions
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("sin"));
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("cos"));
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("tan"));
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("sqrt"));
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("exp"));
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("log"));
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("abs"));
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("pow"));
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("min"));
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("max"));

        // Test mathematical constants
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("pi"));
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("e"));
    }

    TEST_F(FunctionArgumentTest, IsValidArgumentName_InvalidNames_ReturnsFalse)
    {
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName(""));         // Empty
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("123"));      // Starts with number
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("var.name")); // Contains dot
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("var name")); // Contains space
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("var-name")); // Contains hyphen
        EXPECT_FALSE(ArgumentUtils::isValidArgumentName("var+name")); // Contains operator
    }

    // FunctionArgument constructor tests
    TEST_F(FunctionArgumentTest, FunctionArgument_Constructor_CreatesCorrectArgument)
    {
        FunctionArgument scalarArg("radius", ArgumentType::Scalar);
        EXPECT_EQ(scalarArg.name, "radius");
        EXPECT_EQ(scalarArg.type, ArgumentType::Scalar);

        FunctionArgument vectorArg("position", ArgumentType::Vector);
        EXPECT_EQ(vectorArg.name, "position");
        EXPECT_EQ(vectorArg.type, ArgumentType::Vector);
    }

    TEST_F(FunctionArgumentTest, FunctionArgument_DefaultConstructor_CreatesEmptyArgument)
    {
        FunctionArgument arg;
        EXPECT_TRUE(arg.name.empty());
        EXPECT_EQ(arg.type, ArgumentType::Scalar); // Should default to Scalar
    }

    // ComponentAccess constructor tests
    TEST_F(FunctionArgumentTest, ComponentAccess_Constructor_CreatesCorrectAccess)
    {
        ComponentAccess access("pos", "x");
        EXPECT_EQ(access.argumentName, "pos");
        EXPECT_EQ(access.component, "x");
    }

    TEST_F(FunctionArgumentTest, ComponentAccess_DefaultConstructor_CreatesEmptyAccess)
    {
        ComponentAccess access;
        EXPECT_TRUE(access.argumentName.empty());
        EXPECT_TRUE(access.component.empty());
    }

} // namespace gladius::tests
