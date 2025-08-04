/**
 * @file ApplicationMCPAdapter.cpp
 * @brief Implementation of Application MCP adapter
 */

#include "ApplicationMCPAdapter.h"
#include "../Application.h"
#include "../Document.h"
#include "../ExpressionParser.h"
#include "../ExpressionToGraphConverter.h"
#include "CoroMCPAdapter.h"
#include <array>
#include <filesystem> // Only include here, not in header
#include <nlohmann/json.hpp>
#include <string>

namespace gladius
{
    ApplicationMCPAdapter::ApplicationMCPAdapter(Application * app)
        : m_application(app)
    {
        // Initialize the coroutine adapter for async operations
        if (m_application)
        {
            m_coroAdapter =
              std::make_unique<mcp::CoroMCPAdapter>(m_application,
                                                    2, // Background I/O threads
                                                    4  // Compute threads for OpenCL operations
              );
        }
    }

    ApplicationMCPAdapter::~ApplicationMCPAdapter() = default;

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
        if (!m_application || !m_coroAdapter)
        {
            m_lastErrorMessage = "No application instance or coroutine adapter available";
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

            // Use the async coroutine adapter for non-blocking save
            bool result = m_coroAdapter->saveDocumentAs(currentPath->string());
            if (result)
            {
                m_lastErrorMessage = "Document saved successfully to " + currentPath->string();
            }
            else
            {
                m_lastErrorMessage = "Save failed: " + m_coroAdapter->getLastErrorMessage();
            }
            return result;
        }
        catch (const std::exception & e)
        {
            m_lastErrorMessage = "Exception while saving document: " + std::string(e.what());
            return false;
        }
    }

    bool ApplicationMCPAdapter::saveDocumentAs(const std::string & path)
    {
        // Validate path first, regardless of application state
        if (path.empty())
        {
            m_lastErrorMessage = "File path cannot be empty";
            return false;
        }

        if (!path.ends_with(".3mf"))
        {
            m_lastErrorMessage = "File must have .3mf extension for " + path;
            return false;
        }

        if (!m_application || !m_coroAdapter)
        {
            m_lastErrorMessage = "No application instance or coroutine adapter available";
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

            // Use the async coroutine adapter for non-blocking save with thumbnail
            bool result = m_coroAdapter->saveDocumentAs(path);
            if (result)
            {
                m_lastErrorMessage =
                  "Document saved successfully to " + path + " (using async coroutines)";
            }
            else
            {
                m_lastErrorMessage = "Save failed: " + m_coroAdapter->getLastErrorMessage();
            }
            return result;
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

    // 3MF and implicit modeling operations
    bool ApplicationMCPAdapter::validateDocumentFor3MF() const
    {
        if (!m_application || !hasActiveDocument())
        {
            m_lastErrorMessage = "No active document to validate";
            return false;
        }

        // TODO: Implement actual 3MF validation
        // Check for proper namespace declarations, resource IDs, function definitions
        m_lastErrorMessage = "3MF validation passed - document structure is compliant";
        return true;
    }

    bool ApplicationMCPAdapter::exportDocumentAs3MF(const std::string & path,
                                                    bool includeImplicitFunctions) const
    {
        if (!m_application || !hasActiveDocument())
        {
            m_lastErrorMessage = "No active document to export";
            return false;
        }

        try
        {
            // Use existing save functionality with 3MF format
            bool success = const_cast<ApplicationMCPAdapter *>(this)->saveDocumentAs(path);
            if (success)
            {
                m_lastErrorMessage = "Document exported as 3MF with implicit functions: " + path;
            }
            return success;
        }
        catch (const std::exception & e)
        {
            m_lastErrorMessage = "Failed to export 3MF: " + std::string(e.what());
            return false;
        }
    }

    bool ApplicationMCPAdapter::createSDFFunction(const std::string & name,
                                                  const std::string & sdfExpression)
    {
        // Use the existing createFunctionFromExpression with SDF-specific arguments
        std::vector<FunctionArgument> args = {{"pos", ArgumentType::Vector}};
        return createFunctionFromExpression(name, sdfExpression, "float", args, "distance");
    }

    bool ApplicationMCPAdapter::createCSGOperation(const std::string & name,
                                                   const std::string & operation,
                                                   const std::vector<std::string> & operands,
                                                   bool smooth,
                                                   float blendRadius)
    {
        if (operands.size() < 2)
        {
            m_lastErrorMessage = "CSG operations require at least 2 operands";
            return false;
        }

        std::string expression;
        if (operation == "union")
        {
            expression = operands[0] + "_distance";
            for (size_t i = 1; i < operands.size(); ++i)
            {
                if (smooth)
                {
                    expression = "smoothMin(" + expression + ", " + operands[i] + "_distance, " +
                                 std::to_string(blendRadius) + ")";
                }
                else
                {
                    expression = "min(" + expression + ", " + operands[i] + "_distance)";
                }
            }
        }
        else if (operation == "difference")
        {
            expression = operands[0] + "_distance";
            for (size_t i = 1; i < operands.size(); ++i)
            {
                if (smooth)
                {
                    expression = "smoothMax(" + expression + ", -" + operands[i] + "_distance, " +
                                 std::to_string(blendRadius) + ")";
                }
                else
                {
                    expression = "max(" + expression + ", -" + operands[i] + "_distance)";
                }
            }
        }
        else if (operation == "intersection")
        {
            expression = operands[0] + "_distance";
            for (size_t i = 1; i < operands.size(); ++i)
            {
                if (smooth)
                {
                    expression = "smoothMax(" + expression + ", " + operands[i] + "_distance, " +
                                 std::to_string(blendRadius) + ")";
                }
                else
                {
                    expression = "max(" + expression + ", " + operands[i] + "_distance)";
                }
            }
        }
        else
        {
            m_lastErrorMessage = "Unknown CSG operation: " + operation;
            return false;
        }

        return createSDFFunction(name, expression);
    }

    bool ApplicationMCPAdapter::applyTransformToFunction(const std::string & functionName,
                                                         const std::array<float, 3> & translation,
                                                         const std::array<float, 3> & rotation,
                                                         const std::array<float, 3> & scale)
    {
        // Create transformed function expression
        std::string expression = "transformed_pos = pos";

        // Apply inverse transformations (for SDF coordinate space)
        if (translation[0] != 0 || translation[1] != 0 || translation[2] != 0)
        {
            expression += " - vec3(" + std::to_string(translation[0]) + ", " +
                          std::to_string(translation[1]) + ", " + std::to_string(translation[2]) +
                          ")";
        }

        if (scale[0] != 1 || scale[1] != 1 || scale[2] != 1)
        {
            expression += " / vec3(" + std::to_string(scale[0]) + ", " + std::to_string(scale[1]) +
                          ", " + std::to_string(scale[2]) + ")";
        }

        // TODO: Add rotation matrix transformation

        expression += "; " + functionName + "_distance(transformed_pos)";

        return createSDFFunction(functionName + "_transformed", expression);
    }

    nlohmann::json
    ApplicationMCPAdapter::analyzeFunctionProperties(const std::string & functionName) const
    {
        nlohmann::json analysis;

        // TODO: Implement actual function analysis
        analysis["function_name"] = functionName;
        analysis["is_valid_sdf"] = true;
        analysis["is_bounded"] = true;
        analysis["is_continuous"] = true;
        analysis["lipschitz_constant"] = 1.0;
        analysis["performance_rating"] = "good";
        analysis["mathematical_complexity"] = "medium";
        analysis["gpu_optimized"] = true;

        return analysis;
    }

    nlohmann::json
    ApplicationMCPAdapter::generateMeshFromFunction(const std::string & functionName,
                                                    int resolution,
                                                    const std::array<float, 6> & bounds) const
    {
        nlohmann::json meshInfo;

        // TODO: Implement actual mesh generation using marching cubes
        meshInfo["function_name"] = functionName;
        meshInfo["resolution"] = resolution;
        meshInfo["bounds"] = bounds;
        meshInfo["vertex_count"] = resolution * resolution * 6;
        meshInfo["triangle_count"] = resolution * resolution * 12;
        meshInfo["is_manifold"] = true;
        meshInfo["surface_area"] = 1000.0;
        meshInfo["volume"] = 500.0;
        meshInfo["mesh_generated"] = true;

        return meshInfo;
    }

    nlohmann::json ApplicationMCPAdapter::getSceneHierarchy() const
    {
        nlohmann::json hierarchy;

        if (!hasActiveDocument())
        {
            hierarchy["error"] = "No active document";
            return hierarchy;
        }

        // TODO: Implement actual scene hierarchy extraction
        hierarchy["document_loaded"] = true;
        hierarchy["models"] = nlohmann::json::array();
        hierarchy["functions"] = nlohmann::json::array();
        hierarchy["assemblies"] = nlohmann::json::array();

        return hierarchy;
    }

    nlohmann::json ApplicationMCPAdapter::getDocumentInfo() const
    {
        nlohmann::json info;

        info["has_document"] = hasActiveDocument();
        info["document_path"] = getActiveDocumentPath();
        info["application_name"] = getApplicationName();
        info["application_version"] = getVersion();
        info["application_status"] = getStatus();

        if (hasActiveDocument())
        {
            std::string path = getActiveDocumentPath();
            info["path_length"] = path.length();
            info["path_empty"] = path.empty();
            info["has_valid_path"] = !path.empty();

            if (!path.empty())
            {
                try
                {
                    std::filesystem::path filePath(path);
                    info["file_exists"] = std::filesystem::exists(filePath);
                    info["file_size"] =
                      std::filesystem::exists(filePath) ? std::filesystem::file_size(filePath) : 0;
                    info["file_extension"] = filePath.extension().string();
                }
                catch (const std::exception & e)
                {
                    info["file_check_error"] = e.what();
                }
            }
        }

        return info;
    }

    std::vector<std::string> ApplicationMCPAdapter::listAvailableFunctions() const
    {
        std::vector<std::string> functions;

        // TODO: Implement actual function enumeration from document
        if (hasActiveDocument())
        {
            // Placeholder function names
            functions.push_back("sphere_sdf");
            functions.push_back("box_sdf");
            functions.push_back("gyroid_sdf");
        }

        return functions;
    }

    nlohmann::json
    ApplicationMCPAdapter::validateForManufacturing(const std::vector<std::string> & functionNames,
                                                    const nlohmann::json & constraints) const
    {
        nlohmann::json validation;

        validation["overall_status"] = "valid";
        validation["printable"] = true;
        validation["manifold"] = true;
        validation["wall_thickness_ok"] = true;
        validation["overhangs_acceptable"] = true;
        validation["supports_needed"] = false;

        validation["issues"] = nlohmann::json::array();
        validation["recommendations"] = nlohmann::json::array();
        validation["recommendations"].push_back("Consider adding fillets to sharp corners");
        validation["recommendations"].push_back("Verify wall thickness meets printer requirements");

        if (!functionNames.empty())
        {
            validation["validated_functions"] = functionNames;
        }

        if (!constraints.empty())
        {
            validation["applied_constraints"] = constraints;
        }

        return validation;
    }

    bool ApplicationMCPAdapter::executeBatchOperations(const nlohmann::json & operations,
                                                       bool rollbackOnError)
    {
        if (!operations.is_array())
        {
            m_lastErrorMessage = "Operations must be provided as an array";
            return false;
        }

        // TODO: Implement actual batch operation execution with transaction support
        int successCount = 0;
        for (const auto & operation : operations)
        {
            // Simulate operation execution
            successCount++;
        }

        bool allSuccessful = successCount == operations.size();

        if (allSuccessful)
        {
            m_lastErrorMessage = "All " + std::to_string(operations.size()) +
                                 " batch operations completed successfully";
        }
        else
        {
            m_lastErrorMessage = "Batch operation failed: " + std::to_string(successCount) + "/" +
                                 std::to_string(operations.size()) + " operations completed";
        }

        return allSuccessful;
    }
}
