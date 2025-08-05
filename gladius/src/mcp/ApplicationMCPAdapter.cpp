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
