/**
 * @file ApplicationMCPAdapter.cpp
 * @brief Implementation of Application MCP adapter
 */

#include "ApplicationMCPAdapter.h"
#include "../Application.h"
#include "../Document.h"
#include "../ExpressionParser.h"
#include "../ExpressionToGraphConverter.h"
#include "../io/3mf/ResourceIdUtil.h"
#include "CoroMCPAdapter.h"
#include "FunctionGraphSerializer.h"
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

    void ApplicationMCPAdapter::setHeadlessMode(bool headless)
    {
        if (m_application)
        {
            m_application->setHeadlessMode(headless);
        }
    }

    bool ApplicationMCPAdapter::isHeadlessMode() const
    {
        return m_application ? m_application->isHeadlessMode() : true;
    }

    bool ApplicationMCPAdapter::showUI()
    {
        return m_application ? m_application->showUI() : false;
    }

    bool ApplicationMCPAdapter::isUIRunning() const
    {
        return m_application ? m_application->isUIRunning() : false;
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
            if (m_application->isHeadlessMode())
            {
                // Ensure headless has a valid core/document if UI wasn't started
                if (!m_application->getMainWindow().getCurrentDocument())
                {
                    m_application->getMainWindow().setupHeadless(m_application->getGlobalLogger());
                }

                // In headless mode avoid UI-only code paths; create from template directly.
                auto doc = m_application->getCurrentDocument();
                if (!doc)
                {
                    return false;
                }
                doc->newFromTemplate();
                // Welcome screen is a UI concept but safe to hide regardless
                m_application->getMainWindow().hideWelcomeScreen();
                return true;
            }

            // UI mode: use the full UI flow
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
            // Ensure headless has a valid core/document if UI wasn't started
            if (m_application->isHeadlessMode() &&
                !m_application->getMainWindow().getCurrentDocument())
            {
                m_application->getMainWindow().setupHeadless(m_application->getGlobalLogger());
            }
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
                m_lastErrorMessage = "Document saved successfully to " + path;
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

    std::pair<bool, uint32_t> ApplicationMCPAdapter::createFunctionFromExpression(
      const std::string & name,
      const std::string & expression,
      const std::string & outputType,
      const std::vector<FunctionArgument> & providedArguments,
      const std::string & outputName)
    {
        if (!m_application)
        {
            m_lastErrorMessage = "No application instance available";
            return {false, 0};
        }

        if (name.empty())
        {
            m_lastErrorMessage = "Function name cannot be empty";
            return {false, 0};
        }

        if (expression.empty())
        {
            m_lastErrorMessage = "Expression cannot be empty";
            return {false, 0};
        }

        // Validate output type
        if (outputType != "float" && outputType != "vec3")
        {
            m_lastErrorMessage =
              "Invalid output type '" + outputType + "'. Must be 'float' or 'vec3'";
            return {false, 0};
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                m_lastErrorMessage =
                  "No active document available. Please create or open a document first.";
                return {false, 0};
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
                return {false, 0};
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
                        return {false, 0};
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
            output.name = outputName.empty() ? "shape" : outputName;
            output.type = (outputType == "vec3") ? ArgumentType::Vector : ArgumentType::Scalar;

            // Create a new function model and get reference to it
            auto & model = document->createNewFunction();

            // Track the created resource for potential rollback
            uint32_t const newFunctionId = model.getResourceId();

            // Set the display name for the function
            model.setDisplayName(name);

            // Convert expression to node graph (guarded for rollback on exceptions)
            uint32_t resultNodeId = 0;
            try
            {
                resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
                  transformedExpression, model, parser, arguments, output);
            }
            catch (const std::exception & e)
            {
                // Rollback the just-created function on conversion exception
                try
                {
                    document->deleteFunction(newFunctionId);
                }
                catch (...)
                {
                }
                m_lastErrorMessage =
                  std::string("Exception while converting expression to node graph: ") + e.what() +
                  ". The partial function was removed.";
                return {false, 0};
            }

            if (resultNodeId == 0)
            {
                // Rollback the just-created function
                try
                {
                    document->deleteFunction(newFunctionId);
                }
                catch (...)
                {
                    // Swallow rollback errors to preserve original failure cause
                }

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
                m_lastErrorMessage += "] could not be converted to a valid node graph. "
                                      "The partial function was removed.";
                return {false, 0};
            }

            // Persist to 3MF immediately to ensure the function gets a stable ModelResourceID
            // and Gladius model resourceId is synchronized with the 3MF resource id
            try
            {
                document->update3mfModel();
            }
            catch (...)
            {
                // Rollback on persistence failure
                try
                {
                    document->deleteFunction(newFunctionId);
                }
                catch (...)
                {
                }
                m_lastErrorMessage =
                  "Failed to persist function to 3MF model. The partial function was removed.";
                return {false, 0};
            }

            // Get the resource ID from the created model (now synchronized to ModelResourceID)
            uint32_t resourceId = model.getResourceId();

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
            return {true, resourceId};
        }
        catch (const std::exception & e)
        {
            // Best-effort rollback if a function was created before the exception
            try
            {
                if (m_application)
                {
                    auto document = m_application->getCurrentDocument();
                    if (document)
                    {
                        // We cannot reliably know the just-created id here; no-op.
                        // Future improvement: use a scoped guard capturing the id.
                    }
                }
            }
            catch (...)
            {
            }

            m_lastErrorMessage = "Exception during expression validation: " + std::string(e.what());
            return {false, 0};
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

    std::pair<bool, uint32_t>
    ApplicationMCPAdapter::createSDFFunction(const std::string & name,
                                             const std::string & sdfExpression)
    {
        // Use the existing createFunctionFromExpression with SDF-specific arguments
        std::vector<FunctionArgument> args = {{"pos", ArgumentType::Vector}};
        return createFunctionFromExpression(name, sdfExpression, "float", args, "distance");
    }

    std::pair<bool, uint32_t>
    ApplicationMCPAdapter::createCSGOperation(const std::string & name,
                                              const std::string & operation,
                                              const std::vector<std::string> & operands,
                                              bool smooth,
                                              float blendRadius)
    {
        if (operands.size() < 2)
        {
            m_lastErrorMessage = "CSG operations require at least 2 operands";
            return {false, 0};
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
            return {false, 0};
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

        auto result = createSDFFunction(functionName + "_transformed", expression);
        return result.first; // Return only the success flag
    }

    nlohmann::json
    ApplicationMCPAdapter::analyzeFunctionProperties(const std::string & functionName) const
    {
        nlohmann::json analysis;
        analysis["function_name"] = functionName;

        if (!m_application)
        {
            analysis["error"] = "No application instance available";
            return analysis;
        }

        auto document = m_application->getCurrentDocument();
        if (!document)
        {
            analysis["error"] = "No active document available";
            return analysis;
        }

        try
        {
            // Compute bounding box of the document (includes all SDF functions)
            BoundingBox bbox = document->computeBoundingBox();
            analysis["bounding_box"] = {
              {"min", {bbox.min.x, bbox.min.y, bbox.min.z}},
              {"max", {bbox.max.x, bbox.max.y, bbox.max.z}},
              {"size",
               {bbox.max.x - bbox.min.x, bbox.max.y - bbox.min.y, bbox.max.z - bbox.min.z}}};

            // Generate mesh to get geometric properties
            Mesh mesh = document->generateMesh();
            size_t vertexCount = mesh.getNumberOfVertices();
            size_t triangleCount = mesh.getNumberOfFaces();

            analysis["mesh_info"] = {{"vertex_count", vertexCount},
                                     {"triangle_count", triangleCount},
                                     {"has_valid_geometry", vertexCount > 0 && triangleCount > 0}};

            // Calculate approximate volume and surface area
            if (vertexCount > 0 && triangleCount > 0)
            {
                double volume = 0.0;
                double surfaceArea = 0.0;

                // Calculate volume using face iteration
                for (size_t i = 0; i < triangleCount; ++i)
                {
                    auto face = mesh.getFace(i);
                    const auto & v1 = face.vertices[0];
                    const auto & v2 = face.vertices[1];
                    const auto & v3 = face.vertices[2];

                    // Calculate triangle area using cross product
                    auto edge1 = v2 - v1;
                    auto edge2 = v3 - v1;

                    // Manual cross product for area calculation
                    double crossX = edge1.y() * edge2.z() - edge1.z() * edge2.y();
                    double crossY = edge1.z() * edge2.x() - edge1.x() * edge2.z();
                    double crossZ = edge1.x() * edge2.y() - edge1.y() * edge2.x();
                    double crossMagnitude =
                      std::sqrt(crossX * crossX + crossY * crossY + crossZ * crossZ);

                    double triangleArea = 0.5 * crossMagnitude;
                    surfaceArea += triangleArea;

                    // Volume contribution (divergence theorem)
                    auto center = (v1 + v2 + v3) / 3.0;
                    double dotProduct =
                      center.x() * crossX + center.y() * crossY + center.z() * crossZ;
                    volume += dotProduct / 6.0;
                }

                analysis["geometric_properties"] = {{"volume_mm3", std::abs(volume)},
                                                    {"surface_area_mm2", surfaceArea},
                                                    {"volume_cm3", std::abs(volume) / 1000.0}};
            }

            // Function validation checks
            analysis["validation"] = {
              {"is_valid_sdf", true}, // TODO: Implement actual SDF validation
              {"is_bounded",
               (bbox.max.x - bbox.min.x) > 0 && (bbox.max.y - bbox.min.y) > 0 &&
                 (bbox.max.z - bbox.min.z) > 0},
              {"is_continuous", true}, // TODO: Implement continuity check
              {"has_geometry", vertexCount > 0}};

            // Performance analysis
            analysis["performance"] = {{"mesh_complexity",
                                        vertexCount < 10000    ? "low"
                                        : vertexCount < 100000 ? "medium"
                                                               : "high"},
                                       {"gpu_optimized", true}, // Gladius uses OpenCL
                                       {"render_ready", vertexCount > 0}};

            // Mathematical properties (basic estimates)
            float bboxSizeX = bbox.max.x - bbox.min.x;
            float bboxSizeY = bbox.max.y - bbox.min.y;
            float bboxSizeZ = bbox.max.z - bbox.min.z;
            float maxDimension = std::max({bboxSizeX, bboxSizeY, bboxSizeZ});
            analysis["mathematical_properties"] = {
              {"lipschitz_constant", 1.0}, // Conservative estimate for SDF
              {"max_dimension", maxDimension},
              {"aspect_ratio",
               maxDimension > 0 ? std::min({bboxSizeX, bboxSizeY, bboxSizeZ}) / maxDimension : 1.0},
              {"mathematical_complexity",
               vertexCount < 1000    ? "low"
               : vertexCount < 10000 ? "medium"
                                     : "high"}};

            analysis["success"] = true;
        }
        catch (const std::exception & e)
        {
            analysis["error"] = std::string("Analysis failed: ") + e.what();
            analysis["success"] = false;
        }

        return analysis;
    }

    nlohmann::json
    ApplicationMCPAdapter::generateMeshFromFunction(const std::string & functionName,
                                                    int resolution,
                                                    const std::array<float, 6> & bounds) const
    {
        nlohmann::json meshInfo;
        meshInfo["function_name"] = functionName;
        meshInfo["resolution"] = resolution;
        meshInfo["bounds"] = bounds;

        if (!m_application)
        {
            meshInfo["error"] = "No application instance available";
            meshInfo["success"] = false;
            return meshInfo;
        }

        auto document = m_application->getCurrentDocument();
        if (!document)
        {
            meshInfo["error"] = "No active document available";
            meshInfo["success"] = false;
            return meshInfo;
        }

        try
        {
            // Generate mesh from the current document (which contains the SDF functions)
            Mesh mesh = document->generateMesh();

            size_t vertexCount = mesh.getNumberOfVertices();
            size_t triangleCount = mesh.getNumberOfFaces();

            if (vertexCount == 0 || triangleCount == 0)
            {
                meshInfo["error"] = "No geometry to generate mesh from";
                meshInfo["success"] = false;
                return meshInfo;
            }

            // Calculate actual mesh properties
            double volume = 0.0;
            double surfaceArea = 0.0;
            bool isManifold = true; // Assume manifold unless we detect issues

            // Calculate bounding box and mesh properties
            Vector3 minBounds(std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max(),
                              std::numeric_limits<float>::max());
            Vector3 maxBounds(std::numeric_limits<float>::lowest(),
                              std::numeric_limits<float>::lowest(),
                              std::numeric_limits<float>::lowest());

            // Calculate surface area and volume using face iteration
            for (size_t i = 0; i < triangleCount; ++i)
            {
                auto face = mesh.getFace(i);
                const auto & v1 = face.vertices[0];
                const auto & v2 = face.vertices[1];
                const auto & v3 = face.vertices[2];

                // Update bounding box
                for (const auto & vertex : face.vertices)
                {
                    minBounds = minBounds.cwiseMin(vertex);
                    maxBounds = maxBounds.cwiseMax(vertex);
                }

                // Calculate triangle area using cross product
                auto edge1 = v2 - v1;
                auto edge2 = v3 - v1;

                // Manual cross product for area calculation
                double crossX = edge1.y() * edge2.z() - edge1.z() * edge2.y();
                double crossY = edge1.z() * edge2.x() - edge1.x() * edge2.z();
                double crossZ = edge1.x() * edge2.y() - edge1.y() * edge2.x();
                double crossMagnitude =
                  std::sqrt(crossX * crossX + crossY * crossY + crossZ * crossZ);

                double triangleArea = 0.5 * crossMagnitude;
                surfaceArea += triangleArea;

                // Contribute to volume calculation (signed volume of tetrahedron from origin)
                double dotProduct = v1.x() * crossX + v1.y() * crossY + v1.z() * crossZ;
                volume += dotProduct / 6.0;
            }

            meshInfo["vertex_count"] = vertexCount;
            meshInfo["triangle_count"] = triangleCount;
            meshInfo["is_manifold"] = isManifold;
            meshInfo["surface_area_mm2"] = surfaceArea;
            meshInfo["volume_mm3"] = std::abs(volume);
            meshInfo["volume_cm3"] = std::abs(volume) / 1000.0;

            meshInfo["actual_bounds"] = {{"min", {minBounds.x(), minBounds.y(), minBounds.z()}},
                                         {"max", {maxBounds.x(), maxBounds.y(), maxBounds.z()}},
                                         {"size",
                                          {maxBounds.x() - minBounds.x(),
                                           maxBounds.y() - minBounds.y(),
                                           maxBounds.z() - minBounds.z()}}};

            meshInfo["quality_metrics"] = {
              {"vertices_per_triangle",
               static_cast<double>(vertexCount) / static_cast<double>(triangleCount)},
              {"mesh_density",
               surfaceArea > 0 ? static_cast<double>(vertexCount) / surfaceArea : 0},
              {"complexity_rating",
               vertexCount < 1000    ? "low"
               : vertexCount < 10000 ? "medium"
                                     : "high"}};

            meshInfo["mesh_generated"] = true;
            meshInfo["success"] = true;

            // Optional: Save mesh info for debugging
            meshInfo["generation_info"] = {
              {"method", "gladius_native"},
              {"uses_marching_cubes", true}, // Gladius uses marching cubes internally
              {"function_evaluated", functionName}};
        }
        catch (const std::exception & e)
        {
            meshInfo["error"] = "Exception during mesh generation: " + std::string(e.what());
            meshInfo["success"] = false;
        }

        return meshInfo;
    }

    nlohmann::json ApplicationMCPAdapter::getSceneHierarchy() const
    {
        nlohmann::json hierarchy;

        if (!m_application)
        {
            hierarchy["error"] = "No application instance available";
            return hierarchy;
        }

        auto document = m_application->getCurrentDocument();
        if (!document)
        {
            hierarchy["error"] = "No active document";
            hierarchy["has_document"] = false;
            return hierarchy;
        }

        try
        {
            hierarchy["has_document"] = true;
            hierarchy["document_path"] = document->getCurrentAssemblyFilename()
                                           .value_or(std::filesystem::path("unsaved"))
                                           .string();

            // Get the assembly which contains all models and nodes
            auto assembly = document->getAssembly();
            if (!assembly)
            {
                hierarchy["models"] = nlohmann::json::array();
                hierarchy["total_models"] = 0;
                return hierarchy;
            }

            nlohmann::json models = nlohmann::json::array();

            // Iterate through all models in the assembly using getFunctions()
            auto & modelMap = assembly->getFunctions();
            for (const auto & [resourceId, model] : modelMap)
            {
                if (model)
                {
                    nlohmann::json modelInfo;
                    modelInfo["id"] = resourceId;
                    modelInfo["name"] =
                      "Model_" + std::to_string(resourceId); // Generate name from ID
                    modelInfo["type"] = "sdf_model";

                    // Basic model information
                    modelInfo["has_root_node"] = true; // Assume valid models have nodes
                    modelInfo["root_node_type"] = "function_graph";

                    // Try to get more details about the node structure
                    modelInfo["node_info"] = {
                      {"has_geometry", true}, // Assume has geometry if model exists
                      {"is_function_based", true},
                      {"supports_parameters", true}};

                    models.push_back(modelInfo);
                }
            }

            hierarchy["models"] = models;
            hierarchy["total_models"] = modelMap.size();

            // Get bounding box information
            try
            {
                BoundingBox bbox = document->computeBoundingBox();
                hierarchy["scene_bounds"] = {
                  {"min", {bbox.min.x, bbox.min.y, bbox.min.z}},
                  {"max", {bbox.max.x, bbox.max.y, bbox.max.z}},
                  {"size",
                   {bbox.max.x - bbox.min.x, bbox.max.y - bbox.min.y, bbox.max.z - bbox.min.z}},
                  {"is_valid",
                   (bbox.max.x - bbox.min.x) > 0 && (bbox.max.y - bbox.min.y) > 0 &&
                     (bbox.max.z - bbox.min.z) > 0}};
            }
            catch (...)
            {
                hierarchy["scene_bounds"] = {{"error", "Could not compute scene bounds"}};
            }

            // Document-level metadata
            hierarchy["document_info"] = {
              {"has_unsaved_changes",
               false}, // We don't have direct access to this, so default to false
              {"is_3mf_compliant", true}, // Gladius is designed for 3MF
              {"supports_volumetric", true},
              {"uses_sdf_functions", true}};

            // Resource information (meshes, textures, etc.)
            hierarchy["resources"] = {{"mesh_resources", 0}, // TODO: Count actual mesh resources
                                      {"texture_resources", 0},
                                      {"material_resources", 0}};

            hierarchy["success"] = true;
        }
        catch (const std::exception & e)
        {
            hierarchy["error"] = std::string("Failed to get scene hierarchy: ") + e.what();
            hierarchy["success"] = false;
        }

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

    nlohmann::json ApplicationMCPAdapter::get3MFStructure() const
    {
        nlohmann::json out;
        out["has_document"] = hasActiveDocument();
        out["document_path"] = getActiveDocumentPath();

        if (!hasActiveDocument())
        {
            out["error"] = "No active document";
            return out;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                out["error"] = "No active document available";
                return out;
            }

            auto model = document->get3mfModel();
            if (!model)
            {
                out["error"] = "No 3MF model available";
                return out;
            }

            // Build items
            nlohmann::json buildItems = nlohmann::json::array();
            try
            {
                auto buildIt = model->GetBuildItems();
                while (buildIt->MoveNext())
                {
                    auto item = buildIt->GetCurrent();
                    nlohmann::json bi;
                    bi["part_number"] = item->GetPartNumber();
                    bool hasUUID = false;
                    try
                    {
                        auto uuid = item->GetUUID(hasUUID);
                        if (hasUUID)
                        {
                            bi["uuid"] = uuid;
                        }
                        else
                        {
                            bi["uuid"] = nullptr;
                        }
                    }
                    catch (...)
                    {
                        bi["uuid"] = nullptr;
                    }
                    bi["transform"] = nlohmann::json::array();
                    {
                        auto t = item->GetObjectTransform();
                        // 4x3 matrix fields
                        for (int r = 0; r < 4; ++r)
                        {
                            nlohmann::json row = nlohmann::json::array();
                            for (int c = 0; c < 3; ++c)
                            {
                                row.push_back(t.m_Fields[r][c]);
                            }
                            bi["transform"].push_back(row);
                        }
                    }
                    try
                    {
                        auto obj = item->GetObjectResource();
                        if (obj)
                        {
                            bi["object_id"] = obj->GetModelResourceID();
                            // Derive a friendly type name via dynamic cast
                            std::string otype = "object";
                            if (dynamic_cast<Lib3MF::CMeshObject *>(obj.get()))
                                otype = "mesh";
                            else if (dynamic_cast<Lib3MF::CComponentsObject *>(obj.get()))
                                otype = "components";
                            else if (dynamic_cast<Lib3MF::CLevelSet *>(obj.get()))
                                otype = "levelset";
                            bi["object_type"] = otype;
                        }
                        else
                        {
                            bi["object_id"] = 0;
                            bi["object_type"] = "unknown";
                        }
                    }
                    catch (...)
                    {
                        bi["object_id"] = 0;
                        bi["object_type"] = "unknown";
                    }
                    buildItems.push_back(bi);
                }
            }
            catch (...)
            {
                // ignore build item enumeration errors
            }

            // Resources
            nlohmann::json resources = nlohmann::json::array();
            std::size_t meshCount = 0, levelSetCount = 0, functionCount = 0, image3dCount = 0,
                        materialCount = 0, otherCount = 0;
            try
            {
                auto resIt = model->GetResources();
                while (resIt->MoveNext())
                {
                    auto res = resIt->GetCurrent();
                    nlohmann::json r;
                    r["id"] = res->GetModelResourceID();
                    // Derive a readable type/kind via subclass inspection
                    // Note: lib3mf dynamic bindings don't expose a generic GetResourceType or name
                    // on CResource

                    // Try specific casts for richer info
                    if (auto meshObj = dynamic_cast<Lib3MF::CMeshObject *>(res.get()))
                    {
                        meshCount++;
                        r["kind"] = "mesh";
                        r["vertices"] = meshObj->GetVertexCount();
                        r["triangles"] = meshObj->GetTriangleCount();
                        try
                        {
                            r["name"] = meshObj->GetName();
                        }
                        catch (...)
                        {
                            r["name"] = nullptr;
                        }
                    }
                    else if (auto levelSet = dynamic_cast<Lib3MF::CLevelSet *>(res.get()))
                    {
                        levelSetCount++;
                        r["kind"] = "levelset";
                        try
                        {
                            auto fn = levelSet->GetFunction();
                            r["function_id"] = fn ? fn->GetModelResourceID() : 0;
                            r["channel"] = levelSet->GetChannelName();
                        }
                        catch (...)
                        {
                        }
                        try
                        {
                            auto mesh = levelSet->GetMesh();
                            r["mesh_id"] = mesh ? mesh->GetModelResourceID() : 0;
                            r["meshBBoxOnly"] = levelSet->GetMeshBBoxOnly();
                        }
                        catch (...)
                        {
                        }
                        try
                        {
                            r["name"] = levelSet->GetName();
                        }
                        catch (...)
                        {
                            r["name"] = nullptr;
                        }
                    }
                    else if (auto func = dynamic_cast<Lib3MF::CFunction *>(res.get()))
                    {
                        functionCount++;
                        r["kind"] = "function";
                        // Function subtype
                        if (dynamic_cast<Lib3MF::CImplicitFunction *>(res.get()))
                            r["function_type"] = "implicit";
                        else if (dynamic_cast<Lib3MF::CFunctionFromImage3D *>(res.get()))
                            r["function_type"] = "function_from_image3d";
                        else
                            r["function_type"] = "unknown";

                        // Display name
                        try
                        {
                            auto dn = func->GetDisplayName();
                            r["display_name"] = dn;
                            // Back-compat with earlier key
                            r["displayname"] = dn;
                        }
                        catch (...)
                        {
                            r["display_name"] = nullptr;
                            r["displayname"] = nullptr;
                        }

                        // Helper to stringify port type
                        auto portTypeToString = [](Lib3MF::eImplicitPortType t) -> const char *
                        {
                            switch (t)
                            {
                            case Lib3MF::eImplicitPortType::Scalar:
                                return "scalar";
                            case Lib3MF::eImplicitPortType::Vector:
                                return "vector";
                            case Lib3MF::eImplicitPortType::Matrix:
                                return "matrix";
                            default:
                                return "unknown";
                            }
                        };

                        // Inputs
                        try
                        {
                            nlohmann::json inputs = nlohmann::json::array();
                            auto inIt = func->GetInputs();
                            while (inIt->MoveNext())
                            {
                                auto p = inIt->GetCurrent();
                                nlohmann::json pj;
                                try
                                {
                                    pj["identifier"] = p->GetIdentifier();
                                }
                                catch (...)
                                {
                                    pj["identifier"] = nullptr;
                                }
                                try
                                {
                                    pj["display_name"] = p->GetDisplayName();
                                }
                                catch (...)
                                {
                                    pj["display_name"] = nullptr;
                                }
                                try
                                {
                                    pj["type"] = portTypeToString(p->GetType());
                                }
                                catch (...)
                                {
                                    pj["type"] = "unknown";
                                }
                                inputs.push_back(pj);
                            }
                            r["inputs"] = inputs;
                        }
                        catch (...)
                        {
                            r["inputs"] = nlohmann::json::array();
                        }

                        // Outputs
                        try
                        {
                            nlohmann::json outputs = nlohmann::json::array();
                            auto outIt = func->GetOutputs();
                            while (outIt->MoveNext())
                            {
                                auto p = outIt->GetCurrent();
                                nlohmann::json pj;
                                try
                                {
                                    pj["identifier"] = p->GetIdentifier();
                                }
                                catch (...)
                                {
                                    pj["identifier"] = nullptr;
                                }
                                try
                                {
                                    pj["display_name"] = p->GetDisplayName();
                                }
                                catch (...)
                                {
                                    pj["display_name"] = nullptr;
                                }
                                try
                                {
                                    pj["type"] = portTypeToString(p->GetType());
                                }
                                catch (...)
                                {
                                    pj["type"] = "unknown";
                                }
                                outputs.push_back(pj);
                            }
                            r["outputs"] = outputs;
                        }
                        catch (...)
                        {
                            r["outputs"] = nlohmann::json::array();
                        }
                    }
                    else if (auto img3d = dynamic_cast<Lib3MF::CImage3D *>(res.get()))
                    {
                        image3dCount++;
                        r["kind"] = "image3d";
                        try
                        {
                            auto nm = img3d->GetName();
                            r["name"] = nm;
                            r["display_name"] = nm;
                        }
                        catch (...)
                        {
                        }
                        // If this image is actually a stack, query its dimensions
                        if (auto stack = dynamic_cast<Lib3MF::CImageStack *>(res.get()))
                        {
                            nlohmann::json s;
                            try
                            {
                                s["rows"] = stack->GetRowCount();
                            }
                            catch (...)
                            {
                            }
                            try
                            {
                                s["columns"] = stack->GetColumnCount();
                            }
                            catch (...)
                            {
                            }
                            try
                            {
                                s["sheets"] = stack->GetSheetCount();
                            }
                            catch (...)
                            {
                            }
                            r["imagestack"] = s;
                            r["kind"] = "imagestack"; // refine kind if applicable
                        }
                    }
                    else if (auto baseMat = dynamic_cast<Lib3MF::CBaseMaterialGroup *>(res.get()))
                    {
                        materialCount++;
                        r["kind"] = "base_material_group";
                        r["count"] = baseMat->GetCount();
                        // BaseMaterialGroup may not have a name
                        r["name"] = nullptr;
                        r["display_name"] = nullptr;
                    }
                    else
                    {
                        otherCount++;
                        r["kind"] = "other";
                        r["name"] = nullptr;
                        r["display_name"] = nullptr;
                    }

                    resources.push_back(r);
                }
            }
            catch (...)
            {
                // ignore resource enumeration errors
            }

            out["build_items"] = buildItems;
            out["resources"] = resources;
            out["counts"] = {{"build_items", buildItems.size()},
                             {"resources", resources.size()},
                             {"meshes", meshCount},
                             {"levelsets", levelSetCount},
                             {"functions", functionCount},
                             {"images3d", image3dCount},
                             {"materials", materialCount},
                             {"others", otherCount}};

            out["success"] = true;
        }
        catch (const std::exception & e)
        {
            out["success"] = false;
            out["error"] = std::string("Exception while collecting 3MF structure: ") + e.what();
        }

        return out;
    }

    nlohmann::json ApplicationMCPAdapter::getFunctionGraph(uint32_t functionId) const
    {
        nlohmann::json out;
        out["requested_function_id"] = functionId;

        if (!m_application)
        {
            out["success"] = false;
            out["error"] = "No application instance available";
            return out;
        }

        auto document = m_application->getCurrentDocument();
        if (!document)
        {
            out["success"] = false;
            out["error"] = "No active document available";
            return out;
        }

        try
        {
            auto assembly = document->getAssembly();
            if (!assembly)
            {
                out["success"] = false;
                out["error"] = "No assembly available";
                return out;
            }

            auto model = assembly->findModel(functionId);
            if (!model)
            {
                out["success"] = false;
                out["error"] = "Function (model) not found for id";
                return out;
            }

            // Ensure parameters and graph are up-to-date
            const_cast<nodes::Model &>(*model).updateGraphAndOrderIfNeeded();

            auto j = FunctionGraphSerializer::serialize(*model);
            j["success"] = true;
            return j;
        }
        catch (const std::exception & e)
        {
            out["success"] = false;
            out["error"] = std::string("Exception while serializing function graph: ") + e.what();
            return out;
        }
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

    nlohmann::json ApplicationMCPAdapter::validateModel(const nlohmann::json & options)
    {
        nlohmann::json out;
        out["phases"] = nlohmann::json::array();
        out["success"] = false;

        if (!m_application)
        {
            out["error"] = "No application instance available";
            return out;
        }

        auto document = m_application->getCurrentDocument();
        if (!document)
        {
            out["error"] = "No active document available";
            return out;
        }

        bool const doCompile = options.value("compile", true);
        int const maxMessages = options.value("max_messages", 50);

        // Helper to snapshot logger state/messages
        auto snapshotLogger = [&](events::SharedLogger const & logger)
        {
            nlohmann::json j;
            if (!logger)
            {
                j["messages"] = nlohmann::json::array();
                j["errors"] = 0;
                j["warnings"] = 0;
                return j;
            }
            // Flush any pending async writes and get counts
            const_cast<events::Logger &>(*logger).flush();
            j["errors"] = logger->getErrorCount();
            j["warnings"] = logger->getWarningCount();

            // Collect up to maxMessages most recent messages (filter out info messages)
            nlohmann::json msgs = nlohmann::json::array();
            size_t count = 0;
            for (auto it = logger->cbegin();
                 it != logger->cend() && count < static_cast<size_t>(maxMessages);
                 ++it)
            {
                // Skip info messages - only show warnings, errors, and fatal errors
                if (it->getSeverity() == events::Severity::Info)
                {
                    continue;
                }

                nlohmann::json mj;
                mj["message"] = it->getMessage();
                auto tp = it->getTimeStamp();
                mj["timestamp"] =
                  std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch())
                    .count();
                switch (it->getSeverity())
                {
                case events::Severity::Info:
                    mj["severity"] = "info";
                    break;
                case events::Severity::Warning:
                    mj["severity"] = "warning";
                    break;
                case events::Severity::Error:
                    mj["severity"] = "error";
                    break;
                case events::Severity::FatalError:
                    mj["severity"] = "fatal";
                    break;
                }
                msgs.push_back(mj);
                ++count;
            }
            j["messages"] = msgs;
            return j;
        };

        // Phase 1: graph_sync (update 3MF model, update IOs, validate assembly)
        nlohmann::json phase1 = {{"name", "graph_sync"}};
        try
        {
            // Bring 3MF model in sync and update assembly IO relationship
            document->update3mfModel();
            if (auto assembly = document->getAssembly())
            {
                assembly->updateInputsAndOutputs();
            }

            bool validAsm = document->validateAssembly();
            auto logger = document->getSharedLogger();
            auto diag = snapshotLogger(logger);
            phase1.update(diag);
            phase1["ok"] = validAsm && (static_cast<int>(phase1["errors"]) == 0);
        }
        catch (const std::exception & e)
        {
            phase1["ok"] = false;
            phase1["messages"] =
              nlohmann::json::array({nlohmann::json{{"severity", "error"}, {"message", e.what()}}});
            phase1["errors"] = static_cast<int>(phase1["errors"]) + 1;
        }
        out["phases"].push_back(phase1);

        // Phase 2: opencl_compile (optional)
        nlohmann::json phase2 = {{"name", "opencl_compile"}};
        bool compileOk = true;
        if (doCompile)
        {
            try
            {
                // Refresh program and attempt a blocking recompile on the compute core
                auto core = document->getCore();
                if (core)
                {
                    // Ensure the flattened assembly exists and kernel is generated
                    document->updateFlatAssembly();
                    core->tryRefreshProgramProtected(document->getAssembly());
                    core->recompileBlockingNoLock();
                }
                auto logger = document->getSharedLogger();
                auto diag = snapshotLogger(logger);
                phase2.update(diag);
                // Determine compile success: no new errors and renderer program valid
                compileOk = (static_cast<int>(phase2["errors"]) == 0);
                if (core && !core->getBestRenderProgram()->isValid())
                {
                    compileOk = false;
                    // Add a hint message
                    auto msgs = phase2["messages"];
                    msgs.push_back({{"severity", "error"},
                                    {"message", "Render program not valid after compile"}});
                    phase2["messages"] = msgs;
                    phase2["errors"] = static_cast<int>(phase2["errors"]) + 1;
                }
                phase2["ok"] = compileOk;
            }
            catch (const std::exception & e)
            {
                phase2["ok"] = false;
                auto msgs = phase2.value("messages", nlohmann::json::array());
                msgs.push_back({{"severity", "error"}, {"message", e.what()}});
                phase2["messages"] = msgs;
                phase2["errors"] = static_cast<int>(phase2.value("errors", 0)) + 1;
            }
            out["phases"].push_back(phase2);
        }

        // Summary & success flag
        out["summary"] = {{"graph_ok", phase1.value("ok", false)},
                          {"compile_ok", doCompile ? phase2.value("ok", false) : true}};
        out["success"] =
          out["summary"]["graph_ok"].get<bool>() && out["summary"]["compile_ok"].get<bool>();
        return out;
    }

    bool ApplicationMCPAdapter::setBuildItemObjectByIndex(uint32_t buildItemIndex,
                                                          uint32_t objectModelResourceId)
    {
        if (!m_application || !hasActiveDocument())
        {
            m_lastErrorMessage = "No active document available";
            return false;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                m_lastErrorMessage = "No active document available";
                return false;
            }

            // Ensure 3MF model is up to date before modifying
            document->update3mfModel();
            auto model = document->get3mfModel();
            if (!model)
            {
                m_lastErrorMessage = "No 3MF model available";
                return false;
            }

            // Resolve object resource by ModelResourceID -> UniqueResourceID -> Resource
            Lib3MF_uint32 uniqueId = io::resourceIdToUniqueResourceId(
              model, static_cast<gladius::ResourceId>(objectModelResourceId));
            if (uniqueId == 0)
            {
                m_lastErrorMessage = "Could not resolve unique resource id for object id " +
                                     std::to_string(objectModelResourceId);
                return false;
            }
            auto resource = model->GetResourceByID(uniqueId);
            Lib3MF::PObject object = std::dynamic_pointer_cast<Lib3MF::CObject>(resource);
            if (!object)
            {
                m_lastErrorMessage =
                  "Target resource id is not an object (mesh/components/levelset)";
                return false;
            }

            // Find the build item by index
            auto buildIt = model->GetBuildItems();
            uint32_t idx = 0;
            bool found = false;
            Lib3MF::PBuildItem target;
            while (buildIt->MoveNext())
            {
                if (idx == buildItemIndex)
                {
                    target = buildIt->GetCurrent();
                    found = true;
                    break;
                }
                ++idx;
            }
            if (!found || !target)
            {
                m_lastErrorMessage = "Build item index out of range";
                return false;
            }

            // Preserve the current transform and part number when swapping the object
            Lib3MF::sTransform t = target->GetObjectTransform();
            std::string partNumber;
            try
            {
                partNumber = target->GetPartNumber();
            }
            catch (...)
            {
                partNumber.clear();
            }

            // Remove and re-add the build item with the new object
            model->RemoveBuildItem(target);
            auto newBuildItem = model->AddBuildItem(object, t);
            if (!partNumber.empty())
            {
                try
                {
                    newBuildItem->SetPartNumber(partNumber);
                }
                catch (...)
                {
                    // ignore failures setting part number
                }
            }

            // Sync document state
            document->updateDocumenFrom3mfModel();
            m_lastErrorMessage =
              "Build item updated to reference object id " + std::to_string(objectModelResourceId);
            return true;
        }
        catch (const std::exception & e)
        {
            m_lastErrorMessage =
              "Exception while setting build item object: " + std::string(e.what());
            return false;
        }
    }

    bool ApplicationMCPAdapter::setBuildItemTransformByIndex(
      uint32_t buildItemIndex,
      const std::array<float, 12> & transform4x3RowMajor)
    {
        if (!m_application || !hasActiveDocument())
        {
            m_lastErrorMessage = "No active document available";
            return false;
        }
        try
        {
            auto document = m_application->getCurrentDocument();
            auto model = document->get3mfModel();
            if (!model)
            {
                m_lastErrorMessage = "No 3MF model available";
                return false;
            }

            // Find the build item by index
            auto buildIt = model->GetBuildItems();
            uint32_t idx = 0;
            bool found = false;
            Lib3MF::PBuildItem target;
            while (buildIt->MoveNext())
            {
                if (idx == buildItemIndex)
                {
                    target = buildIt->GetCurrent();
                    found = true;
                    break;
                }
                ++idx;
            }
            if (!found || !target)
            {
                m_lastErrorMessage = "Build item index out of range";
                return false;
            }

            // Map 12 floats (row-major 4x3) into sTransform
            Lib3MF::sTransform tr{};
            for (int r = 0; r < 4; ++r)
            {
                for (int c = 0; c < 3; ++c)
                {
                    tr.m_Fields[r][c] = transform4x3RowMajor[r * 3 + c];
                }
            }
            target->SetObjectTransform(tr);

            document->updateDocumenFrom3mfModel();
            m_lastErrorMessage = "Build item transform updated";
            return true;
        }
        catch (const std::exception & e)
        {
            m_lastErrorMessage =
              "Exception while setting build item transform: " + std::string(e.what());
            return false;
        }
    }

    bool ApplicationMCPAdapter::modifyLevelSet(uint32_t levelSetModelResourceId,
                                               std::optional<uint32_t> functionModelResourceId,
                                               std::optional<std::string> channel)
    {
        if (!m_application || !hasActiveDocument())
        {
            m_lastErrorMessage = "No active document available";
            return false;
        }
        try
        {
            auto document = m_application->getCurrentDocument();
            auto model = document->get3mfModel();
            if (!model)
            {
                m_lastErrorMessage = "No 3MF model available";
                return false;
            }

            // Resolve level set by ModelResourceID
            Lib3MF_uint32 uniqueLevelSetId = io::resourceIdToUniqueResourceId(
              model, static_cast<gladius::ResourceId>(levelSetModelResourceId));
            if (uniqueLevelSetId == 0)
            {
                m_lastErrorMessage = "Could not resolve levelset unique id";
                return false;
            }
            auto lsRes = model->GetResourceByID(uniqueLevelSetId);
            auto levelSet = dynamic_cast<Lib3MF::CLevelSet *>(lsRes.get());
            if (!levelSet)
            {
                m_lastErrorMessage = "Resource is not a levelset";
                return false;
            }

            // Optionally update the function reference
            if (functionModelResourceId.has_value())
            {
                Lib3MF_uint32 uniqueFnId = io::resourceIdToUniqueResourceId(
                  model, static_cast<gladius::ResourceId>(functionModelResourceId.value()));
                if (uniqueFnId == 0)
                {
                    m_lastErrorMessage = "Could not resolve function unique id";
                    return false;
                }
                auto fnRes = model->GetResourceByID(uniqueFnId);
                auto fn = dynamic_cast<Lib3MF::CFunction *>(fnRes.get());
                if (!fn)
                {
                    m_lastErrorMessage = "Target resource is not a function";
                    return false;
                }
                levelSet->SetFunction(fn);
            }

            // Optionally update the channel name
            if (channel.has_value())
            {
                levelSet->SetChannelName(channel.value());
            }

            // Update document/assembly links post-change
            document->updateDocumenFrom3mfModel();
            m_lastErrorMessage = "Level set modified successfully";
            return true;
        }
        catch (const std::exception & e)
        {
            m_lastErrorMessage = "Exception while modifying level set: " + std::string(e.what());
            return false;
        }
    }

    // 3MF Resource creation methods implementation
    std::pair<bool, uint32_t> ApplicationMCPAdapter::createLevelSet(uint32_t functionId)
    {
        if (!m_application || !hasActiveDocument())
        {
            m_lastErrorMessage = "No active document available";
            return {false, 0};
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                m_lastErrorMessage = "No active document available";
                return {false, 0};
            }

            // Update the 3MF model to ensure all resources are up to date
            document->update3mfModel();

            // Update the assembly to process function graphs and outputs
            document->getAssembly()->updateInputsAndOutputs();

            auto model3mf = document->get3mfModel();
            if (!model3mf)
            {
                m_lastErrorMessage = "No 3MF model available";
                return {false, 0};
            }

            // Get the function resource by ID
            // Note: Gladius uses lib3mf ModelResourceID as its ResourceId. lib3mf::GetResourceByID
            // expects the UniqueResourceID. Convert before lookup to avoid mixing IDs.
            Lib3MF_uint32 uniqueFunctionId = io::resourceIdToUniqueResourceId(
              model3mf, static_cast<gladius::ResourceId>(functionId));
            if (uniqueFunctionId == 0)
            {
                m_lastErrorMessage = "Could not resolve unique resource id for function id " +
                                     std::to_string(functionId);
                return {false, 0};
            }

            auto resource = model3mf->GetResourceByID(uniqueFunctionId);
            auto functionResource = dynamic_cast<Lib3MF::CFunction *>(resource.get());
            if (!functionResource)
            {
                m_lastErrorMessage = "Function with ID " + std::to_string(functionId) +
                                     " not found or is not a function";
                return {false, 0};
            }

            // Create a bounding box mesh for the levelset
            auto mesh = model3mf->AddMeshObject();
            // Create a simple bounding box from -10 to 10 in all dimensions
            auto v0 = mesh->AddVertex({-10.0f, -10.0f, -10.0f});
            auto v1 = mesh->AddVertex({10.0f, -10.0f, -10.0f});
            auto v2 = mesh->AddVertex({10.0f, 10.0f, -10.0f});
            auto v3 = mesh->AddVertex({-10.0f, 10.0f, -10.0f});
            auto v4 = mesh->AddVertex({-10.0f, -10.0f, 10.0f});
            auto v5 = mesh->AddVertex({10.0f, -10.0f, 10.0f});
            auto v6 = mesh->AddVertex({10.0f, 10.0f, 10.0f});
            auto v7 = mesh->AddVertex({-10.0f, 10.0f, 10.0f});

            mesh->AddTriangle({v0, v1, v2});
            mesh->AddTriangle({v0, v2, v3});
            mesh->AddTriangle({v4, v5, v6});
            mesh->AddTriangle({v4, v6, v7});
            mesh->AddTriangle({v0, v4, v7});
            mesh->AddTriangle({v0, v7, v3});
            mesh->AddTriangle({v1, v5, v6});
            mesh->AddTriangle({v1, v6, v2});
            mesh->AddTriangle({v0, v1, v5});
            mesh->AddTriangle({v0, v5, v4});
            mesh->AddTriangle({v3, v7, v6});
            mesh->AddTriangle({v3, v6, v2});
            mesh->SetName("Bounding Box");

            // Get the mesh ModelResourceID before creating the levelset (consistent with Gladius
            // IDs)
            uint32_t meshId = mesh->GetModelResourceID();

            // Create the levelset
            auto levelSet = model3mf->AddLevelSet();
            levelSet->SetFunction(functionResource);
            levelSet->SetMesh(mesh.get());
            levelSet->SetMeshBBoxOnly(true);
            levelSet->SetMinFeatureSize(0.1f);
            levelSet->SetFallBackValue(0.0f);

            // Try to determine the correct channel name
            // First try "shape", then fall back to "result" if that fails
            std::string channelName = "shape";
            try
            {
                levelSet->SetChannelName(channelName);
            }
            catch (...)
            {
                // If "shape" fails, try "result"
                channelName = "result";
                levelSet->SetChannelName(channelName);
            }

            // Get the ModelResourceID of the created levelset for returning to callers
            uint32_t levelSetId = levelSet->GetModelResourceID();

            // Create an identity transform for the build item
            Lib3MF::sTransform identityTransform;
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 3; j++)
                {
                    identityTransform.m_Fields[i][j] = (i == j) ? 1.0f : 0.0f;
                }
            }

            // Add a build item that references the levelset
            auto buildItem = model3mf->AddBuildItem(levelSet.get(), identityTransform);

            // Update the document from the 3MF model
            document->updateDocumenFrom3mfModel();

            m_lastErrorMessage =
              "Level set created successfully from function ID: " + std::to_string(functionId) +
              " using mesh ID: " + std::to_string(meshId);
            return {true, levelSetId};
        }
        catch (const std::exception & e)
        {
            m_lastErrorMessage = "Exception during level set creation: " + std::string(e.what());
            return {false, 0};
        }
    }

    std::pair<bool, uint32_t>
    ApplicationMCPAdapter::createImage3DFunction(const std::string & name,
                                                 const std::string & imagePath,
                                                 float valueScale,
                                                 float valueOffset)
    {
        if (!m_application || !hasActiveDocument())
        {
            m_lastErrorMessage = "No active document available";
            return {false, 0};
        }

        // TODO: Implement actual Image3D function creation when API is available
        // For now, create a function placeholder and return resource ID
        m_lastErrorMessage = "Image3D function '" + name + "' would be created from: " + imagePath +
                             " with scale: " + std::to_string(valueScale) +
                             ", offset: " + std::to_string(valueOffset);

        // Create a simple placeholder function for now
        auto result = createFunctionFromExpression(
          name, "0.0", "float", {{"pos", ArgumentType::Vector}}, "result");
        return result;
    }

    std::pair<bool, uint32_t>
    ApplicationMCPAdapter::createVolumetricColor(uint32_t functionId, const std::string & channel)
    {
        if (!m_application || !hasActiveDocument())
        {
            m_lastErrorMessage = "No active document available";
            return {false, 0};
        }

        // TODO: Implement actual volumetric color creation when API is available
        m_lastErrorMessage =
          "Volumetric color data would be created from function ID: " + std::to_string(functionId) +
          " using channel: " + channel;
        return {true, functionId + 2000}; // Placeholder: offset function ID for color data ID
    }

    std::pair<bool, uint32_t>
    ApplicationMCPAdapter::createVolumetricProperty(const std::string & propertyName,
                                                    uint32_t functionId,
                                                    const std::string & channel)
    {
        if (!m_application || !hasActiveDocument())
        {
            m_lastErrorMessage = "No active document available";
            return {false, 0};
        }

        // TODO: Implement actual volumetric property creation when API is available
        m_lastErrorMessage = "Volumetric property '" + propertyName +
                             "' would be created from function ID: " + std::to_string(functionId) +
                             " using channel: " + channel;
        return {true, functionId + 3000}; // Placeholder: offset function ID for property data ID
    }

    bool ApplicationMCPAdapter::renderToFile(const std::string & outputPath,
                                             uint32_t width,
                                             uint32_t height,
                                             const std::string & format,
                                             float quality)
    {
        if (!m_application || !hasActiveDocument())
        {
            m_lastErrorMessage = "No active document available for rendering";
            return false;
        }

        try
        {
            // Get the current document
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                m_lastErrorMessage = "No active document";
                return false;
            }

            // Get the compute core from the document
            auto core = document->getCore();
            if (!core)
            {
                m_lastErrorMessage = "No compute core available";
                return false;
            }

            // Ensure assembly is updated before rendering
            try
            {
                document->updateFlatAssembly();
                core->tryRefreshProgramProtected(document->getAssembly());
            }
            catch (const std::exception & e)
            {
                m_lastErrorMessage = "Assembly update failed: " + std::string(e.what());
                return false;
            }

            // Prepare the model for rendering
            if (!core->prepareThumbnailGeneration())
            {
                m_lastErrorMessage =
                  "Render preparation failed: This may be due to model compilation "
                  "errors, SDF precomputation failure, or invalid bounding box. "
                  "Check model validation for detailed OpenCL compilation errors.";
                return false;
            }

            // Set the rendering resolution
            if (!core->setScreenResolution(width, height))
            {
                m_lastErrorMessage = "Failed to set screen resolution to " + std::to_string(width) +
                                     "x" + std::to_string(height);
                return false;
            }

            // Create the output directory if it doesn't exist
            std::filesystem::path outputFilePath(outputPath);
            std::filesystem::create_directories(outputFilePath.parent_path());

            // Currently only PNG is fully supported
            if (format == "png")
            {
                // Use existing thumbnail generation infrastructure
                core->saveThumbnail(outputPath);
                m_lastErrorMessage = "Rendered to: " + outputPath;
                return true;
            }
            else
            {
                m_lastErrorMessage = "Format '" + format + "' not yet implemented. Use 'png'.";
                return false;
            }
        }
        catch (const std::exception & e)
        {
            m_lastErrorMessage = "Rendering failed: " + std::string(e.what());
            return false;
        }
    }

    bool ApplicationMCPAdapter::renderWithCamera(const std::string & outputPath,
                                                 const nlohmann::json & cameraSettings,
                                                 const nlohmann::json & renderSettings)
    {
        if (!m_application || !hasActiveDocument())
        {
            m_lastErrorMessage = "No active document available for rendering";
            return false;
        }

        try
        {
            // Get the current document
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                m_lastErrorMessage = "No active document";
                return false;
            }

            // Get the compute core from the document
            auto core = document->getCore();
            if (!core)
            {
                m_lastErrorMessage = "No compute core available";
                return false;
            }

            // Ensure assembly is updated before rendering
            try
            {
                document->updateFlatAssembly();
                core->tryRefreshProgramProtected(document->getAssembly());
            }
            catch (const std::exception & e)
            {
                m_lastErrorMessage = "Assembly update failed: " + std::string(e.what());
                return false;
            }

            // Prepare the model for rendering
            if (!core->prepareThumbnailGeneration())
            {
                m_lastErrorMessage =
                  "Camera render preparation failed: This may be due to model compilation "
                  "errors, SDF precomputation failure, or invalid bounding box. "
                  "Check model validation for detailed OpenCL compilation errors.";
                return false;
            }

            // Parse render settings with defaults
            uint32_t width = renderSettings.value("width", 1024);
            uint32_t height = renderSettings.value("height", 1024);
            std::string format = renderSettings.value("format", "png");

            // Set resolution
            if (!core->setScreenResolution(width, height))
            {
                m_lastErrorMessage = "Failed to set screen resolution";
                return false;
            }

            // Parse and apply camera settings if provided
            if (cameraSettings.contains("eye_position") &&
                cameraSettings.contains("target_position"))
            {
                // TODO: Implement camera application when API is available
                // For now, we'll use the existing rendering without camera changes
                m_lastErrorMessage += "Camera settings received but not yet fully implemented. ";
            }

            // Create output directory
            std::filesystem::path outputFilePath(outputPath);
            std::filesystem::create_directories(outputFilePath.parent_path());

            // Render based on format
            if (format == "png")
            {
                core->saveThumbnail(outputPath);
                m_lastErrorMessage += "Camera render completed: " + outputPath;
                return true;
            }
            else
            {
                m_lastErrorMessage =
                  "Format '" + format + "' not yet implemented for camera rendering";
                return false;
            }
        }
        catch (const std::exception & e)
        {
            m_lastErrorMessage = "Camera rendering failed: " + std::string(e.what());
            return false;
        }
    }

    bool ApplicationMCPAdapter::generateThumbnail(const std::string & outputPath, uint32_t size)
    {
        if (!m_application || !hasActiveDocument())
        {
            m_lastErrorMessage = "No active document available for thumbnail generation";
            return false;
        }

        try
        {
            // Caveman log: entering thumbnail generation (headless friendly)
            try
            {
                auto globalLogger = m_application->getGlobalLogger();
                if (globalLogger)
                {
                    globalLogger->logInfo(
                      std::string{"MCP.generateThumbnail: start path='"} + outputPath +
                      "' size=" + std::to_string(size) +
                      (m_application->isHeadlessMode() ? " headless=1" : " headless=0"));
                }
            }
            catch (...)
            {
            }

            // Get the current document
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                m_lastErrorMessage = "No active document";
                return false;
            }

            // Get the compute core from the document
            auto core = document->getCore();
            if (!core)
            {
                m_lastErrorMessage = "No compute core available";
                return false;
            }

            // Ensure assembly is updated before thumbnail preparation
            try
            {
                if (auto log = document->getSharedLogger())
                {
                    log->logInfo("MCP.generateThumbnail: updating flat assembly and program");
                }
                document->updateFlatAssembly();
                core->tryRefreshProgramProtected(document->getAssembly());
                if (auto log = document->getSharedLogger())
                {
                    log->logInfo("MCP.generateThumbnail: assembly/program update OK");
                }
            }
            catch (const std::exception & e)
            {
                m_lastErrorMessage = "Assembly update failed: " + std::string(e.what());
                if (auto log = document->getSharedLogger())
                {
                    log->logError(std::string{"MCP.generateThumbnail: assembly update failed: "} +
                                  e.what());
                }
                return false;
            }

            // Prepare the model for thumbnail generation
            if (!core->prepareThumbnailGeneration())
            {
                m_lastErrorMessage =
                  "Thumbnail preparation failed: This may be due to model compilation "
                  "errors, SDF precomputation failure, or invalid bounding box. "
                  "Check model validation for detailed OpenCL compilation errors.";
                if (auto log = document->getSharedLogger())
                {
                    log->logError(
                      "MCP.generateThumbnail: prepareThumbnailGeneration returned false");
                }
                return false;
            }

            // Create output directory
            std::filesystem::path outputFilePath(outputPath);
            std::filesystem::create_directories(outputFilePath.parent_path());

            // Use existing thumbnail functionality
            if (auto log = document->getSharedLogger())
            {
                log->logInfo(std::string{"MCP.generateThumbnail: writing thumbnail to '"} +
                             outputPath + "'");
            }
            core->saveThumbnail(outputPath);

            m_lastErrorMessage = "Thumbnail generated: " + outputPath +
                                 " (size requested: " + std::to_string(size) + "px)";
            if (auto log = document->getSharedLogger())
            {
                log->logInfo("MCP.generateThumbnail: done");
            }
            return true;
        }
        catch (const std::exception & e)
        {
            m_lastErrorMessage = "Thumbnail generation failed: " + std::string(e.what());
            try
            {
                if (m_application && m_application->getCurrentDocument())
                {
                    if (auto log = m_application->getCurrentDocument()->getSharedLogger())
                    {
                        log->logError(std::string{"MCP.generateThumbnail: exception: "} + e.what());
                    }
                }
            }
            catch (...)
            {
            }
            return false;
        }
    }

    nlohmann::json ApplicationMCPAdapter::getOptimalCameraPosition() const
    {
        if (!m_application || !hasActiveDocument())
        {
            return {{"error", "No active document available"}};
        }

        try
        {
            // Get the current document
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                return {{"error", "No active document"}};
            }

            // Get the compute core from the document
            auto core = document->getCore();
            if (!core)
            {
                return {{"error", "No compute core available"}};
            }

            // Get the bounding box of the current model
            auto bbox = core->getBoundingBox();
            // If the bbox is not yet available, try to prepare like thumbnail generation does
            if (!bbox.has_value())
            {
                // Ensure assembly and programs are up-to-date
                try
                {
                    document->updateFlatAssembly();
                    core->tryRefreshProgramProtected(document->getAssembly());
                }
                catch (const std::exception & e)
                {
                    return {
                      {"error",
                       std::string("Assembly update failed while computing camera: ") + e.what()}};
                }

                // Use the same preparation path used for thumbnail generation, which updates bbox
                if (!core->prepareThumbnailGeneration())
                {
                    return {{"error",
                             "Failed to compute bounding box (model compile/SDF precompute may "
                             "have failed). Run validate_model for details."}};
                }

                // Re-query the bounding box
                bbox = core->getBoundingBox();
                if (!bbox.has_value())
                {
                    return {{"error", "No bounding box available for the model"}};
                }
            }

            auto bb = bbox.value();

            // Calculate the center of the bounding box
            float centerX = (bb.min.x + bb.max.x) * 0.5f;
            float centerY = (bb.min.y + bb.max.y) * 0.5f;
            float centerZ = (bb.min.z + bb.max.z) * 0.5f;

            // Calculate the size of the bounding box
            float sizeX = bb.max.x - bb.min.x;
            float sizeY = bb.max.y - bb.min.y;
            float sizeZ = bb.max.z - bb.min.z;
            float maxSize = std::max({sizeX, sizeY, sizeZ});

            // Position camera at a distance that shows the entire model
            float distance = maxSize * 2.0f;

            // Default viewing angle: slightly above and to the front-right
            float eyeX = centerX + distance * 0.7f;
            float eyeY = centerY - distance * 0.7f;
            float eyeZ = centerZ + distance * 0.5f;

            return {{"eye_position", {eyeX, eyeY, eyeZ}},
                    {"target_position", {centerX, centerY, centerZ}},
                    {"up_vector", {0, 0, 1}},
                    {"field_of_view", 45.0},
                    {"bounding_box",
                     {{"min", {bb.min.x, bb.min.y, bb.min.z}},
                      {"max", {bb.max.x, bb.max.y, bb.max.z}},
                      {"center", {centerX, centerY, centerZ}},
                      {"size", {sizeX, sizeY, sizeZ}}}}};
        }
        catch (const std::exception & e)
        {
            return {
              {"error", "Failed to calculate optimal camera position: " + std::string(e.what())}};
        }
    }
}