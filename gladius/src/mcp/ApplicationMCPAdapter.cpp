/**
 * @file ApplicationMCPAdapter.cpp
 * @brief Implementation of Application MCP adapter
 */

#include "ApplicationMCPAdapter.h"
#include "../Application.h"
#include "../Document.h"
#include "../ExpressionParser.h"
#include "../ExpressionToGraphConverter.h"
#include <filesystem> // Only include here, not in header
#include <nlohmann/json.hpp>

namespace gladius
{
    ApplicationMCPAdapter::ApplicationMCPAdapter(Application * app)
        : m_application(app)
    {
    }

    std::string ApplicationMCPAdapter::getVersion() const
    {
        if (!m_application)
        {
            return "Unknown";
        }

        // TODO: Get actual version from Application
        return "1.0.0";
    }

    bool ApplicationMCPAdapter::isRunning() const
    {
        return m_application != nullptr;
    }

    std::string ApplicationMCPAdapter::getApplicationName() const
    {
        return "Gladius";
    }

    std::string ApplicationMCPAdapter::getStatus() const
    {
        if (!m_application)
        {
            return "not_running";
        }

        return "running";
    }

    bool ApplicationMCPAdapter::hasActiveDocument() const
    {
        if (!m_application)
        {
            return false;
        }

        auto document = m_application->getCurrentDocument();
        return document != nullptr;
    }

    std::string ApplicationMCPAdapter::getActiveDocumentPath() const
    {
        if (!m_application)
        {
            return "";
        }

        auto document = m_application->getCurrentDocument();
        if (!document)
        {
            return "";
        }

        // Get the current assembly filename from the document
        auto filename = document->getCurrentAssemblyFilename();
        if (filename.has_value())
        {
            return filename->string();
        }

        return "";
    }

    bool ApplicationMCPAdapter::createNewDocument()
    {
        if (!m_application)
        {
            return false;
        }

        try
        {
            // Use MainWindow's newModel method and hide welcome screen like the UI callback does
            m_application->getMainWindow().newModel();
            m_application->getMainWindow().hideWelcomeScreen();
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    bool ApplicationMCPAdapter::openDocument(const std::string & path)
    {
        if (!m_application)
        {
            return false;
        }

        try
        {
            // Use MainWindow's open method to properly hide welcome screen and handle UI updates
            m_application->getMainWindow().open(std::filesystem::path(path));
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    bool ApplicationMCPAdapter::saveDocument()
    {
        if (!m_application)
        {
            m_lastErrorMessage = "No application instance available";
            return false;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                m_lastErrorMessage =
                  "No active document available. Please create or open a document first.";
                return false;
            }

            // Check if document has a current filename
            auto currentPath = document->getCurrentAssemblyFilename();
            if (!currentPath.has_value())
            {
                m_lastErrorMessage = "Document has not been saved before. Use 'save_document_as' "
                                     "to specify a filename.";
                return false;
            }

            document->saveAs(currentPath.value());
            m_lastErrorMessage = "Document saved successfully to " + currentPath->string();
            return true;
        }
        catch (const std::exception & e)
        {
            m_lastErrorMessage = "Exception while saving document: " + std::string(e.what());
            return false;
        }
    }

    bool ApplicationMCPAdapter::saveDocumentAs(const std::string & path)
    {
        try
        {
            std::filesystem::path filePath(path);

            // Validate path first (this can be tested without application instance)
            if (path.empty())
            {
                m_lastErrorMessage = "File path cannot be empty";
                return false;
            }

            // Check file extension
            if (!filePath.has_extension() || filePath.extension() != ".3mf")
            {
                m_lastErrorMessage = "File must have .3mf extension. Current path: " + path;
                return false;
            }

            // Now check application instance
            if (!m_application)
            {
                m_lastErrorMessage = "No application instance available";
                return false;
            }

            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                m_lastErrorMessage =
                  "No active document available. Please create or open a document first.";
                return false;
            }

            // Check if directory exists and create if necessary
            auto parentDir = filePath.parent_path();
            if (!parentDir.empty() && !std::filesystem::exists(parentDir))
            {
                std::error_code ec;
                if (!std::filesystem::create_directories(parentDir, ec))
                {
                    m_lastErrorMessage = "Failed to create directory: " + parentDir.string() +
                                         " (error: " + ec.message() + ")";
                    return false;
                }
            }

            document->saveAs(filePath);
            m_lastErrorMessage = "Document saved successfully to " + path;
            return true;
        }
        catch (const std::exception & e)
        {
            m_lastErrorMessage = "Exception while saving document: " + std::string(e.what());
            return false;
        }
    }

    bool ApplicationMCPAdapter::exportDocument(const std::string & path, const std::string & format)
    {
        if (!m_application)
        {
            return false;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                if (format == "stl")
                {
                    document->exportAsStl(std::filesystem::path(path));
                    return true;
                }
                // Add more export formats as needed
                return false;
            }
            return false;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    bool ApplicationMCPAdapter::setFloatParameter(uint32_t modelId,
                                                  const std::string & nodeName,
                                                  const std::string & parameterName,
                                                  float value)
    {
        if (!m_application)
        {
            return false;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                document->setFloatParameter(modelId, nodeName, parameterName, value);
                return true;
            }
            return false;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    float ApplicationMCPAdapter::getFloatParameter(uint32_t modelId,
                                                   const std::string & nodeName,
                                                   const std::string & parameterName)
    {
        if (!m_application)
        {
            return 0.0f;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                return document->getFloatParameter(modelId, nodeName, parameterName);
            }
            return 0.0f;
        }
        catch (const std::exception &)
        {
            return 0.0f;
        }
    }

    bool ApplicationMCPAdapter::setStringParameter(uint32_t modelId,
                                                   const std::string & nodeName,
                                                   const std::string & parameterName,
                                                   const std::string & value)
    {
        if (!m_application)
        {
            return false;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                document->setStringParameter(modelId, nodeName, parameterName, value);
                return true;
            }
            return false;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    std::string ApplicationMCPAdapter::getStringParameter(uint32_t modelId,
                                                          const std::string & nodeName,
                                                          const std::string & parameterName)
    {
        if (!m_application)
        {
            return "";
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                return document->getStringParameter(modelId, nodeName, parameterName);
            }
            return "";
        }
        catch (const std::exception &)
        {
            return "";
        }
    }

    bool ApplicationMCPAdapter::createFunctionFromExpression(
      const std::string & name,
      const std::string & expression,
      const std::string & outputType,
      const std::vector<FunctionArgument> & providedArguments,
      const std::string & outputName)
    {
        if (!m_application)
        {
            m_lastErrorMessage = "No application instance available";
            return false;
        }

        if (name.empty())
        {
            m_lastErrorMessage = "Function name cannot be empty";
            return false;
        }

        if (expression.empty())
        {
            m_lastErrorMessage = "Expression cannot be empty";
            return false;
        }

        // Validate output type
        if (outputType != "float" && outputType != "vec3")
        {
            m_lastErrorMessage =
              "Invalid output type '" + outputType + "'. Must be 'float' or 'vec3'";
            return false;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                m_lastErrorMessage =
                  "No active document available. Please create or open a document first.";
                return false;
            }

            // Parse and validate the expression syntax
            ExpressionParser parser;
            if (!parser.parseExpression(expression))
            {
                m_lastErrorMessage =
                  "Expression parsing failed: " + parser.getLastError() +
                  "\n\nSupported syntax:" + "\n- Variables: x, y, z (for 3D coordinates)" +
                  "\n- Math operators: +, -, *, /, ^ (power)" +
                  "\n- Functions: sin(), cos(), tan(), sqrt(), abs(), exp(), log(), pow()" +
                  "\n- Constants: pi, e" +
                  "\n- Component access: pos.x, pos.y, pos.z (for vec3 inputs)" +
                  "\n- Parentheses for grouping: (x + y) * z" + "\n\nExample valid expressions:" +
                  "\n- Gyroid: 'sin(x)*cos(y) + sin(y)*cos(z) + sin(z)*cos(x)'" +
                  "\n- Sphere: 'sqrt(x*x + y*y + z*z) - 5'" +
                  "\n- Scaled wave: 'sin(x*2*pi/10)*cos(y*2*pi/10)'";
                return false;
            }

            // Convert expression to node graph using ExpressionToGraphConverter
            auto variables = parser.getVariables();

            // Create function arguments - use provided arguments or auto-detect
            std::vector<FunctionArgument> arguments;
            std::string transformedExpression = expression;

            if (!providedArguments.empty())
            {
                // Use the provided arguments
                arguments = providedArguments;

                // Validate that all variables in the expression are covered by the arguments
                for (const auto & variable : variables)
                {
                    bool found = false;
                    for (const auto & arg : arguments)
                    {
                        if (arg.name == variable ||
                            (arg.type == ArgumentType::Vector &&
                             (variable == arg.name + ".x" || variable == arg.name + ".y" ||
                              variable == arg.name + ".z")))
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        m_lastErrorMessage =
                          "Variable '" + variable +
                          "' used in expression is not defined in function arguments. " +
                          "Please define it as a function input or use component access like "
                          "'pos.x' for vector inputs.";
                        return false;
                    }
                }
            }
            else
            {
                // Auto-detect arguments from expression variables
                // Check if expression uses x, y, z coordinates
                bool usesXYZ = false;
                for (const auto & variable : variables)
                {
                    if (variable == "x" || variable == "y" || variable == "z")
                    {
                        usesXYZ = true;
                        break;
                    }
                }

                if (usesXYZ)
                {
                    // For expressions using x, y, z coordinates, create a single 3D position input
                    FunctionArgument posArg("pos", ArgumentType::Vector);
                    arguments.push_back(posArg);

                    // Transform the expression to use component access
                    // Replace x, y, z with pos.x, pos.y, pos.z
                    size_t pos = 0;
                    while ((pos = transformedExpression.find("x", pos)) != std::string::npos)
                    {
                        // Make sure it's a standalone variable, not part of another word
                        bool isStandalone =
                          (pos == 0 || !std::isalnum(transformedExpression[pos - 1])) &&
                          (pos + 1 >= transformedExpression.length() ||
                           !std::isalnum(transformedExpression[pos + 1]));
                        if (isStandalone)
                        {
                            transformedExpression.replace(pos, 1, "pos.x");
                            pos += 5; // Skip past "pos.x"
                        }
                        else
                        {
                            pos += 1;
                        }
                    }

                    pos = 0;
                    while ((pos = transformedExpression.find("y", pos)) != std::string::npos)
                    {
                        bool isStandalone =
                          (pos == 0 || !std::isalnum(transformedExpression[pos - 1])) &&
                          (pos + 1 >= transformedExpression.length() ||
                           !std::isalnum(transformedExpression[pos + 1]));
                        if (isStandalone)
                        {
                            transformedExpression.replace(pos, 1, "pos.y");
                            pos += 5; // Skip past "pos.y"
                        }
                        else
                        {
                            pos += 1;
                        }
                    }

                    pos = 0;
                    while ((pos = transformedExpression.find("z", pos)) != std::string::npos)
                    {
                        bool isStandalone =
                          (pos == 0 || !std::isalnum(transformedExpression[pos - 1])) &&
                          (pos + 1 >= transformedExpression.length() ||
                           !std::isalnum(transformedExpression[pos + 1]));
                        if (isStandalone)
                        {
                            transformedExpression.replace(pos, 1, "pos.z");
                            pos += 5; // Skip past "pos.z"
                        }
                        else
                        {
                            pos += 1;
                        }
                    }
                }
                else
                {
                    // For other variables, create scalar arguments
                    for (const auto & variable : variables)
                    {
                        FunctionArgument arg(variable, ArgumentType::Scalar);
                        arguments.push_back(arg);
                    }
                }
            }

            // Create function output specification
            FunctionOutput output;
            output.name = outputName.empty() ? "result" : outputName;
            output.type = (outputType == "vec3") ? ArgumentType::Vector : ArgumentType::Scalar;

            // Create a new function model and get reference to it
            auto & model = document->createNewFunction();

            // Convert expression to node graph
            auto resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
              transformedExpression, model, parser, arguments, output);

            if (resultNodeId == 0)
            {
                m_lastErrorMessage = std::string("Failed to convert expression to node graph. ") +
                                     "The expression '" + transformedExpression +
                                     "' with arguments [";
                for (size_t i = 0; i < arguments.size(); ++i)
                {
                    if (i > 0)
                        m_lastErrorMessage += ", ";
                    m_lastErrorMessage +=
                      arguments[i].name + ":" +
                      (arguments[i].type == ArgumentType::Vector ? "vec3" : "float");
                }
                m_lastErrorMessage += "] could not be converted to a valid node graph.";
                return false;
            }

            // Success!
            m_lastErrorMessage = std::string("Function '") + name +
                                 "' created successfully with expression '" +
                                 transformedExpression + "' and arguments [";
            for (size_t i = 0; i < arguments.size(); ++i)
            {
                if (i > 0)
                    m_lastErrorMessage += ", ";
                m_lastErrorMessage +=
                  arguments[i].name + ":" +
                  (arguments[i].type == ArgumentType::Vector ? "vec3" : "float");
            }
            m_lastErrorMessage += "]";
            return true;
        }
        catch (const std::exception & e)
        {
            m_lastErrorMessage = "Exception during expression validation: " + std::string(e.what());
            return false;
        }
    }
}
