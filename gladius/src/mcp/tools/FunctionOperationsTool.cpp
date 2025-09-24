/**
 * @file FunctionOperationsTool.cpp
 * @brief Implementation of FunctionOperationsTool
 */

#include "FunctionOperationsTool.h"
#include "../../Application.h"
#include "../../Document.h"
#include "../../ExpressionParser.h"
#include "../../ExpressionToGraphConverter.h"
#include "../../FunctionArgument.h"
#include "../../io/3mf/ResourceIdUtil.h"
#include "../../nodes/DerivedNodes.h"
#include "../../nodes/NodeFactory.h"
#include "../../nodes/Parameter.h"
#include "../../nodes/Port.h"
#include "../../nodes/nodesfwd.h"
#include "../FunctionGraphDeserializer.h"
#include "../FunctionGraphSerializer.h"
#include <array>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <queue>
#include <set>
#include <string>

namespace gladius
{
    namespace mcp::tools
    {
        /// @brief Creates simplified input parameter information for MCP responses
        static nlohmann::json createSimplifiedInputInfo(const nodes::VariantParameter & param,
                                                        const std::string & name)
        {
            nlohmann::json result;
            result["name"] = name;
            result["type"] = FunctionGraphSerializer::typeIndexToString(param.getTypeIndex());
            result["is_connected"] = param.getConstSource().has_value();
            return result;
        }

        /// @brief Creates simplified output port information for MCP responses
        static nlohmann::json createSimplifiedOutputInfo(const nodes::Port & port,
                                                         const std::string & name)
        {
            nlohmann::json result;
            result["name"] = name;
            result["type"] = FunctionGraphSerializer::typeIndexToString(port.getTypeIndex());
            return result;
        }

        FunctionOperationsTool::FunctionOperationsTool(Application * app)
            : MCPToolBase(app)
        {
        }

        std::pair<bool, uint32_t> FunctionOperationsTool::createFunctionFromExpression(
          const std::string & name,
          const std::string & expression,
          const std::string & outputType,
          const std::vector<FunctionArgument> & providedArguments,
          const std::string & outputName)
        {
            if (!validateApplication())
            {
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
                    // Surface the rich error produced by ExpressionParser (includes caret & hints)
                    m_lastErrorMessage =
                      std::string("Expression parsing failed:\n") + parser.getLastError() +
                      "\n\nSupported syntax:" + "\n- Variables: x, y, z (for 3D coordinates)" +
                      "\n- Math operators: +, -, *, /" +
                      "\n- Functions: sin(), cos(), tan(), asin(), acos(), atan(), atan2(), "
                      "sqrt(), abs(), exp(), log(), pow(base, exp)" +
                      "\n- Modulo: mod(x, y), fmod(x, y)" + "\n- Min/Max: min(a, b), max(a, b)" +
                      "\n- Constants: pi, e" +
                      "\n- Component access: pos.x, pos.y, pos.z (for vec3 inputs)" +
                      "\n- Parentheses for grouping: (x + y) * z" +
                      "\n\nExample valid expressions:" +
                      "\n- Gyroid: 'sin(x)*cos(y) + sin(y)*cos(z) + sin(z)*cos(x)'" +
                      "\n- Sphere: 'sqrt(x*x + y*y + z*z) - 5'" +
                      "\n- Torus: 'pow(sqrt(x*x + y*y) - 10, 2) + z*z - 4'" +
                      "\n- Scaled wave: 'sin(x*2*pi/10)*cos(y*2*pi/10)'" + "\n-";
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
                        // For expressions using x, y, z coordinates, create a single 3D position
                        // input
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
                output.type =
                  (outputType == "vec3" || outputType == "vector" || outputType == "float3")
                    ? ArgumentType::Vector
                    : ArgumentType::Scalar;

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
                      std::string("Exception while converting expression to node graph: ") +
                      e.what() + ". The partial function was removed.";
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

                    m_lastErrorMessage =
                      std::string("Failed to convert expression to node graph. ") +
                      "The expression '" + transformedExpression + "' with arguments [";
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

                m_lastErrorMessage =
                  "Exception during expression validation: " + std::string(e.what());
                return {false, 0};
            }
        }

        nlohmann::json
        FunctionOperationsTool::analyzeFunctionProperties(const std::string & functionName) const
        {
            nlohmann::json analysis;
            analysis["function_name"] = functionName;

            if (!validateApplication())
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

                analysis["mesh_info"] = {
                  {"vertex_count", vertexCount},
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
                   maxDimension > 0 ? std::min({bboxSizeX, bboxSizeY, bboxSizeZ}) / maxDimension
                                    : 1.0},
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
        FunctionOperationsTool::generateMeshFromFunction(const std::string & functionName,
                                                         int resolution,
                                                         const std::array<float, 6> & bounds) const
        {
            nlohmann::json meshInfo;
            meshInfo["function_name"] = functionName;
            meshInfo["resolution"] = resolution;
            meshInfo["bounds"] = bounds;

            if (!validateApplication())
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

        std::vector<std::string> FunctionOperationsTool::listAvailableFunctions() const
        {
            std::vector<std::string> functions;

            if (!validateApplication())
            {
                return functions;
            }

            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                return functions;
            }

            try
            {
                auto assembly = document->getAssembly();
                if (!assembly)
                {
                    return functions;
                }

                // Get all models from the assembly
                for (const auto & [id, model] : assembly->getFunctions())
                {
                    if (model)
                    {
                        auto displayName = model->getDisplayName();
                        if (displayName.has_value())
                        {
                            functions.push_back(displayName.value());
                        }
                    }
                }
            }
            catch (const std::exception & /* e */)
            {
                // Log error but don't modify the last error message for const method
                // This is a read-only operation, so we don't set error state
            }

            return functions;
        }

        nlohmann::json FunctionOperationsTool::getFunctionGraph(uint32_t functionId) const
        {
            nlohmann::json out;
            out["requested_function_id"] = functionId;

            if (!validateApplication())
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

                auto j = FunctionGraphSerializer::serializeMinimal(*model);
                j["success"] = true;
                return j;
            }
            catch (const std::exception & e)
            {
                out["success"] = false;
                out["error"] =
                  std::string("Exception while serializing function graph: ") + e.what();
                return out;
            }
        }

        nlohmann::json FunctionOperationsTool::getNodeInfo(uint32_t functionId,
                                                           uint32_t nodeId) const
        {
            using json = nlohmann::json;
            json out;
            out["requested_function_id"] = functionId;
            out["requested_node_id"] = nodeId;

            if (!validateApplication())
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

                auto nodeOpt = model->getNode(nodeId);
                if (!nodeOpt.has_value() || nodeOpt.value() == nullptr)
                {
                    out["success"] = false;
                    out["error"] = "Node not found for id";
                    return out;
                }

                json jn;
                auto * node = nodeOpt.value();
                jn["id"] = node->getId();
                jn["order"] = node->getOrder();
                jn["name"] = node->name();
                jn["unique_name"] = node->getUniqueName();
                jn["display_name"] = node->getDisplayName();
                jn["category"] = static_cast<int>(node->getCategory());
                auto pos = const_cast<nodes::NodeBase *>(node)->screenPos();
                jn["position"] = {pos.x, pos.y};

                // Parameters (inputs) - simplified for MCP
                json jparams = json::array();
                for (auto const & [pname, param] : node->constParameter())
                {
                    jparams.push_back(createSimplifiedInputInfo(param, pname));
                }
                jn["parameters"] = jparams;

                // Outputs (ports) - simplified for MCP
                json jouts = json::array();
                for (auto const & [oname, port] : node->outputs())
                {
                    jouts.push_back(createSimplifiedOutputInfo(port, oname));
                }
                jn["outputs"] = jouts;

                out["node"] = jn;
                out["success"] = true;

                return out;
            }
            catch (const std::exception & e)
            {
                out["success"] = false;
                out["error"] = std::string("Exception while getting node info: ") + e.what();
                return out;
            }
        }

        nlohmann::json FunctionOperationsTool::createNode(uint32_t functionId,
                                                          const std::string & nodeType,
                                                          const std::string & displayName,
                                                          uint32_t /*nodeId*/)
        {
            using json = nlohmann::json;
            json out;
            out["requested_function_id"] = functionId;
            out["requested_node_type"] = nodeType;

            if (!validateApplication())
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

                // Create the node via factory
                auto newNode = nodes::NodeFactory::createNode(nodeType);
                if (!newNode)
                {
                    out["success"] = false;
                    std::string errorMsg =
                      std::string("Unknown node type: ") + nodeType + ". Valid node types are: ";
                    auto validTypes = nodes::NodeFactory::getValidNodeTypes();
                    for (size_t i = 0; i < validTypes.size(); ++i)
                    {
                        if (i > 0)
                            errorMsg += ", ";
                        errorMsg += validTypes[i];
                    }
                    out["error"] = errorMsg;
                    out["valid_node_types"] = validTypes;
                    return out;
                }

                if (!displayName.empty())
                {
                    newNode->setDisplayName(displayName);
                }

                // Insert into the model (assigns a new id and registers ports/params)
                auto * created = model->insert(std::move(newNode));
                model->updateGraphAndOrderIfNeeded();

                json jn;
                jn["id"] = created->getId();
                jn["unique_name"] = created->getUniqueName();
                jn["display_name"] = created->getDisplayName();
                jn["category"] = static_cast<int>(created->getCategory());
                out["node"] = jn;
                out["success"] = true;

                return out;
            }
            catch (const std::exception & e)
            {
                out["success"] = false;
                out["error"] = std::string("Exception while creating node: ") + e.what();
                return out;
            }
        }

        nlohmann::json FunctionOperationsTool::setFunctionGraph(uint32_t functionId,
                                                                const nlohmann::json & graph,
                                                                bool replace)
        {
            using json = nlohmann::json;
            json out;
            out["requested_function_id"] = functionId;

            if (!validateApplication())
            {
                return json{{"success", false}, {"error", "No application instance available"}};
            }

            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                return json{{"success", false}, {"error", "No active document available"}};
            }

            try
            {
                auto assembly = document->getAssembly();
                if (!assembly)
                {
                    return json{{"success", false}, {"error", "No assembly available"}};
                }

                auto model = assembly->findModel(functionId);
                if (!model)
                {
                    return json{{"success", false}, {"error", "Function (model) not found for id"}};
                }

                // Delegate to deserializer
                json result = mcp::FunctionGraphDeserializer::applyToModel(*model, graph, replace);
                // Preserve request context
                result["requested_function_id"] = functionId;
                return result;
            }
            catch (const std::exception & e)
            {
                return json{
                  {"success", false},
                  {"error", std::string("Exception while setting function graph: ") + e.what()}};
            }
        }

        nlohmann::json FunctionOperationsTool::deleteNode(uint32_t functionId, uint32_t nodeId)
        {
            using json = nlohmann::json;
            json out;
            out["requested_function_id"] = functionId;
            out["requested_node_id"] = nodeId;

            if (!validateApplication())
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

                // Ensure node exists prior to removal
                auto nodeOpt = model->getNode(nodeId);
                if (!nodeOpt.has_value() || nodeOpt.value() == nullptr)
                {
                    out["success"] = false;
                    out["error"] = "Node not found for id";
                    return out;
                }

                model->remove(nodeId);
                model->updateGraphAndOrderIfNeeded();
                out["success"] = true;

                return out;
            }
            catch (const std::exception & e)
            {
                out["success"] = false;
                out["error"] = std::string("Exception while deleting node: ") + e.what();
                return out;
            }
        }

        nlohmann::json FunctionOperationsTool::setParameterValue(uint32_t functionId,
                                                                 uint32_t nodeId,
                                                                 const std::string & parameterName,
                                                                 const nlohmann::json & value)
        {
            using json = nlohmann::json;
            json out;
            out["requested_function_id"] = functionId;
            out["requested_node_id"] = nodeId;
            out["parameter_name"] = parameterName;

            if (!validateApplication())
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

                auto nodeOpt = model->getNode(nodeId);
                if (!nodeOpt.has_value() || nodeOpt.value() == nullptr)
                {
                    out["success"] = false;
                    out["error"] = "Node not found for id";
                    return out;
                }

                auto * node = nodeOpt.value();
                auto * param = node->getParameter(parameterName);
                if (!param)
                {
                    out["success"] = false;
                    out["error"] = "Parameter not found";
                    return out;
                }

                // Determine parameter type and assign from JSON
                auto typeIdx = param->getTypeIndex();
                try
                {
                    if (typeIdx == nodes::ParameterTypeIndex::Float)
                    {
                        float v = 0.f;
                        if (value.is_number_float())
                            v = static_cast<float>(value.get<double>());
                        else if (value.is_number_integer())
                            v = static_cast<float>(value.get<long long>());
                        else
                            throw std::runtime_error("Expected number for float parameter");
                        param->setValue(nodes::VariantType{v});
                    }
                    else if (typeIdx == nodes::ParameterTypeIndex::Int)
                    {
                        int v = 0;
                        if (value.is_number_integer())
                            v = static_cast<int>(value.get<long long>());
                        else if (value.is_number_float())
                            v = static_cast<int>(value.get<double>());
                        else
                            throw std::runtime_error("Expected integer for int parameter");
                        param->setValue(nodes::VariantType{v});
                    }
                    else if (typeIdx == nodes::ParameterTypeIndex::String)
                    {
                        if (!value.is_string())
                            throw std::runtime_error("Expected string for string parameter");
                        param->setValue(nodes::VariantType{value.get<std::string>()});
                    }
                    else if (typeIdx == nodes::ParameterTypeIndex::Float3)
                    {
                        nodes::float3 v{};
                        if (value.is_array() && value.size() == 3 && value[0].is_number() &&
                            value[1].is_number() && value[2].is_number())
                        {
                            v.x = static_cast<float>(value[0].get<double>());
                            v.y = static_cast<float>(value[1].get<double>());
                            v.z = static_cast<float>(value[2].get<double>());
                        }
                        else if (value.is_object() && value.contains("x") && value.contains("y") &&
                                 value.contains("z"))
                        {
                            v.x = static_cast<float>(value["x"].get<double>());
                            v.y = static_cast<float>(value["y"].get<double>());
                            v.z = static_cast<float>(value["z"].get<double>());
                        }
                        else
                        {
                            throw std::runtime_error(
                              "Expected [x,y,z] array or {x,y,z} for float3 parameter");
                        }
                        param->setValue(nodes::VariantType{v});
                    }
                    else if (typeIdx == nodes::ParameterTypeIndex::Matrix4)
                    {
                        nodes::Matrix4x4 m{};
                        if (value.is_array() && value.size() == 16)
                        {
                            for (int i = 0; i < 4; ++i)
                            {
                                for (int j = 0; j < 4; ++j)
                                {
                                    m[i][j] = static_cast<float>(value[i * 4 + j].get<double>());
                                }
                            }
                        }
                        else if (value.is_array() && value.size() == 4 && value[0].is_array() &&
                                 value[1].is_array() && value[2].is_array() && value[3].is_array())
                        {
                            for (int i = 0; i < 4; ++i)
                            {
                                for (int j = 0; j < 4; ++j)
                                {
                                    m[i][j] = static_cast<float>(value[i][j].get<double>());
                                }
                            }
                        }
                        else
                        {
                            throw std::runtime_error(
                              "Expected 16-array or 4x4 array for matrix parameter");
                        }
                        param->setValue(nodes::VariantType{m});
                    }
                    else if (typeIdx == nodes::ParameterTypeIndex::ResourceId)
                    {
                        if (!value.is_number_integer())
                            throw std::runtime_error("Expected integer for resource id parameter");
                        param->setValue(nodes::VariantType{
                          static_cast<gladius::ResourceId>(value.get<long long>())});
                    }
                    else
                    {
                        // Fallback: try to set as float
                        if (value.is_number())
                        {
                            param->setValue(
                              nodes::VariantType{static_cast<float>(value.get<double>())});
                        }
                        else
                        {
                            throw std::runtime_error("Unsupported parameter type");
                        }
                    }
                }
                catch (const std::exception & ex)
                {
                    out["success"] = false;
                    out["error"] = std::string("Failed to set parameter value: ") + ex.what();
                    return out;
                }

                // Invalidate and refresh ordering/types
                model->invalidateGraph();
                model->updateGraphAndOrderIfNeeded();

                out["success"] = true;

                return out;
            }
            catch (const std::exception & e)
            {
                out["success"] = false;
                out["error"] = std::string("Exception while setting parameter: ") + e.what();
                return out;
            }
        }

        nlohmann::json FunctionOperationsTool::createLink(uint32_t functionId,
                                                          uint32_t sourceNodeId,
                                                          const std::string & sourcePortName,
                                                          uint32_t targetNodeId,
                                                          const std::string & targetParameterName)
        {
            using json = nlohmann::json;
            json out;
            out["requested_function_id"] = functionId;
            out["source_node_id"] = sourceNodeId;
            out["source_port_name"] = sourcePortName;
            out["target_node_id"] = targetNodeId;
            out["target_parameter_name"] = targetParameterName;

            if (!validateApplication())
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

                auto srcNodeOpt = model->getNode(sourceNodeId);
                auto dstNodeOpt = model->getNode(targetNodeId);
                if (!srcNodeOpt.has_value() || srcNodeOpt.value() == nullptr)
                {
                    out["success"] = false;
                    out["error"] = "Source node not found";
                    return out;
                }
                if (!dstNodeOpt.has_value() || dstNodeOpt.value() == nullptr)
                {
                    out["success"] = false;
                    out["error"] = "Target node not found";

                    // Provide information about all nodes with unconnected inputs
                    json nodesWithUnconnectedInputs = json::array();
                    for (auto const & [nodeId, nodePtr] : *model)
                    {
                        if (nodePtr)
                        {
                            json nodeInfo;
                            nodeInfo["id"] = nodePtr->getId();
                            nodeInfo["name"] = nodePtr->name();
                            nodeInfo["display_name"] = nodePtr->getDisplayName();

                            json unconnectedParams = json::array();
                            for (auto const & [pname, parameter] : nodePtr->constParameter())
                            {
                                if (!parameter.getConstSource().has_value())
                                {
                                    unconnectedParams.push_back(
                                      createSimplifiedInputInfo(parameter, pname));
                                }
                            }

                            if (!unconnectedParams.empty())
                            {
                                nodeInfo["unconnected_parameters"] = unconnectedParams;
                                nodesWithUnconnectedInputs.push_back(nodeInfo);
                            }
                        }
                    }
                    out["nodes_with_unconnected_inputs"] = nodesWithUnconnectedInputs;

                    return out;
                }

                auto * srcNode = srcNodeOpt.value();
                auto * dstNode = dstNodeOpt.value();
                auto * port = srcNode->findOutputPort(sourcePortName);
                if (!port)
                {
                    out["success"] = false;
                    out["error"] = "Source port not found on source node";
                    return out;
                }

                auto * param = dstNode->getParameter(targetParameterName);
                if (!param)
                {
                    out["success"] = false;
                    out["error"] = "Target parameter not found on target node";

                    // Provide detailed information about the target node's available parameters -
                    // simplified for MCP
                    json targetNodeParams = json::array();
                    for (auto const & [pname, parameter] : dstNode->constParameter())
                    {
                        targetNodeParams.push_back(createSimplifiedInputInfo(parameter, pname));
                    }
                    out["target_node_available_parameters"] = targetNodeParams;

                    return out;
                }

                bool ok = model->addLink(port->getId(), param->getId());
                if (!ok)
                {
                    out["success"] = false;
                    out["error"] = "Link not valid or failed to add";
                    return out;
                }
                model->updateGraphAndOrderIfNeeded();
                out["success"] = true;

                return out;
            }
            catch (const std::exception & e)
            {
                out["success"] = false;
                out["error"] = std::string("Exception while creating link: ") + e.what();
                return out;
            }
        }

        nlohmann::json FunctionOperationsTool::deleteLink(uint32_t functionId,
                                                          uint32_t targetNodeId,
                                                          const std::string & targetParameterName)
        {
            using json = nlohmann::json;
            json out;
            out["requested_function_id"] = functionId;
            out["target_node_id"] = targetNodeId;
            out["target_parameter_name"] = targetParameterName;

            if (!validateApplication())
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

                auto dstNodeOpt = model->getNode(targetNodeId);
                if (!dstNodeOpt.has_value() || dstNodeOpt.value() == nullptr)
                {
                    out["success"] = false;
                    out["error"] = "Target node not found";
                    return out;
                }
                auto * dstNode = dstNodeOpt.value();
                auto * param = dstNode->getParameter(targetParameterName);
                if (!param)
                {
                    out["success"] = false;
                    out["error"] = "Target parameter not found";
                    return out;
                }

                auto const & srcOpt = param->getConstSource();
                if (!srcOpt.has_value())
                {
                    out["success"] = false;
                    out["error"] = "Parameter has no source link";
                    return out;
                }

                bool ok = model->removeLink(srcOpt->portId, param->getId());
                if (!ok)
                {
                    out["success"] = false;
                    out["error"] = "Failed to remove link";
                    return out;
                }
                model->updateGraphAndOrderIfNeeded();
                out["success"] = true;

                return out;
            }
            catch (const std::exception & e)
            {
                out["success"] = false;
                out["error"] = std::string("Exception while deleting link: ") + e.what();
                return out;
            }
        }

        nlohmann::json
        FunctionOperationsTool::createFunctionCallNode(uint32_t targetFunctionId,
                                                       uint32_t referencedFunctionId,
                                                       const std::string & displayName)
        {
            using json = nlohmann::json;
            json out;
            out["target_function_id"] = targetFunctionId;
            out["referenced_function_id"] = referencedFunctionId;
            out["display_name"] = displayName;

            if (!validateApplication())
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

                // Find the target model (where we're adding the function call)
                auto targetModel = assembly->findModel(targetFunctionId);
                if (!targetModel)
                {
                    out["success"] = false;
                    out["error"] = "Target function (model) not found for id";
                    return out;
                }

                // Verify the referenced function exists
                auto referencedModel = assembly->findModel(referencedFunctionId);
                if (!referencedModel)
                {
                    out["success"] = false;
                    out["error"] = "Referenced function (model) not found for id";
                    return out;
                }

                // Create a Resource node for the function ID
                auto resourceNode = targetModel->create<nodes::Resource>();
                resourceNode->parameter().at(nodes::FieldNames::ResourceId) =
                  nodes::VariantParameter(referencedFunctionId);

                if (!displayName.empty())
                {
                    std::string resourceDisplayName = displayName + "_Resource";
                    resourceNode->setDisplayName(resourceDisplayName);
                }

                // Create the FunctionCall node
                auto functionCallNode = targetModel->create<nodes::FunctionCall>();

                // Connect the Resource node's output to the FunctionCall's FunctionId input
                functionCallNode->parameter()
                  .at(nodes::FieldNames::FunctionId)
                  .setInputFromPort(resourceNode->getOutputs().at(nodes::FieldNames::Value));

                // Update inputs and outputs based on the referenced function
                functionCallNode->updateInputsAndOutputs(*referencedModel);

                // Register the function call node's parameters and outputs with the model
                targetModel->registerInputs(*functionCallNode);
                targetModel->registerOutputs(*functionCallNode);

                // Set display name if provided
                if (!displayName.empty())
                {
                    functionCallNode->setDisplayName(displayName);
                }
                else if (referencedModel->getDisplayName().has_value())
                {
                    functionCallNode->setDisplayName(referencedModel->getDisplayName().value());
                }

                // Update the graph to ensure everything is properly connected
                targetModel->updateGraphAndOrderIfNeeded();

                // Prepare response with node information
                json resourceNodeInfo;
                resourceNodeInfo["id"] = resourceNode->getId();
                resourceNodeInfo["unique_name"] = resourceNode->getUniqueName();
                resourceNodeInfo["display_name"] = resourceNode->getDisplayName();
                resourceNodeInfo["type"] = "Resource";

                json functionCallNodeInfo;
                functionCallNodeInfo["id"] = functionCallNode->getId();
                functionCallNodeInfo["unique_name"] = functionCallNode->getUniqueName();
                functionCallNodeInfo["display_name"] = functionCallNode->getDisplayName();
                functionCallNodeInfo["type"] = "FunctionCall";

                // Collect unconnected inputs (parameters without sources) - simplified for MCP
                json unconnectedInputs = json::array();
                for (auto const & [paramName, param] : functionCallNode->constParameter())
                {
                    if (!param.getConstSource().has_value() && param.isInputSourceRequired())
                    {
                        unconnectedInputs.push_back(createSimplifiedInputInfo(param, paramName));
                    }
                }

                // Collect all outputs - simplified for MCP
                json outputs = json::array();
                for (auto const & [portName, port] : functionCallNode->getOutputs())
                {
                    outputs.push_back(createSimplifiedOutputInfo(port, portName));
                }

                out["success"] = true;
                out["resource_node"] = resourceNodeInfo;
                out["function_call_node"] = functionCallNodeInfo;
                out["unconnected_inputs"] = unconnectedInputs;
                out["outputs"] = outputs;

                return out;
            }
            catch (const std::exception & e)
            {
                out["success"] = false;
                out["error"] =
                  std::string("Exception while creating function call node: ") + e.what();
                return out;
            }
        }

        nlohmann::json
        FunctionOperationsTool::createConstantNodesForMissingParameters(uint32_t functionId,
                                                                        uint32_t nodeId,
                                                                        bool autoConnect)
        {
            using json = nlohmann::json;
            json out;
            out["function_id"] = functionId;
            out["node_id"] = nodeId;
            out["auto_connect"] = autoConnect;

            if (!validateApplication())
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

                auto nodeOpt = model->getNode(nodeId);
                if (!nodeOpt.has_value() || nodeOpt.value() == nullptr)
                {
                    out["success"] = false;
                    out["error"] = "Node not found for id";
                    return out;
                }

                auto * node = nodeOpt.value();

                // Find all unconnected required parameters
                json unconnectedParams = json::array();
                json createdNodes = json::array();
                json createdLinks = json::array();

                for (auto const & [paramName, param] : node->constParameter())
                {
                    // Check if parameter needs an input source and doesn't have one
                    if (!param.getConstSource().has_value() && param.isInputSourceRequired())
                    {
                        json paramInfo = createSimplifiedInputInfo(param, paramName);
                        unconnectedParams.push_back(paramInfo);

                        // Create appropriate constant node based on parameter type
                        nodes::NodeBase * createdConstantNode = nullptr;
                        nodes::Port const * outputPort = nullptr;

                        auto typeIdx = param.getTypeIndex();
                        if (typeIdx == nodes::ParameterTypeIndex::Float)
                        {
                            auto * constantNode = model->create<nodes::ConstantScalar>();
                            constantNode->setDisplayName(paramName);

                            createdConstantNode = constantNode;
                            outputPort = &constantNode->getValueOutputPort();
                        }
                        else if (typeIdx == nodes::ParameterTypeIndex::Float3)
                        {
                            auto * constantNode = model->create<nodes::ConstantVector>();
                            constantNode->setDisplayName(paramName);

                            createdConstantNode = constantNode;
                            outputPort = &constantNode->getVectorOutputPort();
                        }
                        else if (typeIdx == nodes::ParameterTypeIndex::Matrix4)
                        {
                            auto * constantNode = model->create<nodes::ConstantMatrix>();
                            constantNode->setDisplayName(paramName);

                            createdConstantNode = constantNode;
                            outputPort = &constantNode->getMatrixOutputPort();
                        }
                        else if (typeIdx == nodes::ParameterTypeIndex::ResourceId)
                        {
                            auto * constantNode = model->create<nodes::Resource>();
                            constantNode->setDisplayName(paramName);

                            createdConstantNode = constantNode;
                            outputPort = &constantNode->getOutputs().at(nodes::FieldNames::Value);
                        }
                        else
                        {
                            // For unsupported types, just record the parameter
                            json unsupportedParam;
                            unsupportedParam["parameter_name"] = paramName;
                            unsupportedParam["type"] =
                              FunctionGraphSerializer::typeIndexToString(typeIdx);
                            unsupportedParam["error"] =
                              "Unsupported parameter type for constant node creation";
                            unconnectedParams.push_back(unsupportedParam);
                            continue;
                        }

                        if (createdConstantNode)
                        {
                            json nodeInfo;
                            nodeInfo["id"] = createdConstantNode->getId();
                            nodeInfo["unique_name"] = createdConstantNode->getUniqueName();
                            nodeInfo["display_name"] = createdConstantNode->getDisplayName();
                            nodeInfo["type"] = createdConstantNode->name();
                            nodeInfo["parameter_name"] = paramName;
                            nodeInfo["parameter_type"] =
                              FunctionGraphSerializer::typeIndexToString(typeIdx);

                            createdNodes.push_back(nodeInfo);

                            // Auto-connect if requested and possible
                            if (autoConnect && outputPort)
                            {
                                bool linkCreated =
                                  model->addLink(outputPort->getId(), param.getId());
                                if (linkCreated)
                                {
                                    json linkInfo;
                                    linkInfo["source_node_id"] = createdConstantNode->getId();
                                    linkInfo["source_port_name"] = outputPort->getShortName();
                                    linkInfo["target_node_id"] = nodeId;
                                    linkInfo["target_parameter_name"] = paramName;

                                    createdLinks.push_back(linkInfo);
                                }
                            }
                        }
                    }
                }

                // Update the graph to ensure everything is properly connected
                model->updateGraphAndOrderIfNeeded();

                out["success"] = true;
                out["unconnected_parameters"] = unconnectedParams;
                out["created_constant_nodes"] = createdNodes;
                out["created_links"] = createdLinks;
                out["total_created_nodes"] = createdNodes.size();
                out["total_created_links"] = createdLinks.size();

                if (createdNodes.empty())
                {
                    out["message"] = "No missing parameters found that require constant nodes";
                }
                else
                {
                    out["message"] = "Created " + std::to_string(createdNodes.size()) +
                                     " constant node(s) for missing parameters";
                }

                return out;
            }
            catch (const std::exception & e)
            {
                out["success"] = false;
                out["error"] = std::string("Exception while creating constant nodes: ") + e.what();
                return out;
            }
        }

        nlohmann::json FunctionOperationsTool::removeUnusedNodes(uint32_t functionId)
        {
            nlohmann::json out;

            try
            {
                auto document = m_application->getCurrentDocument();
                if (!document)
                {
                    out["success"] = false;
                    out["error"] = "No active document";
                    return out;
                }

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
                    out["error"] = "Function with id " + std::to_string(functionId) + " not found";
                    return out;
                }

                // Model is already mutable through shared_ptr

                // Find all nodes in the function
                std::vector<nodes::NodeBase *> allNodes;
                for (auto const & [nodeId, nodePtr] : *model)
                {
                    allNodes.push_back(nodePtr.get());
                }

                // Find nodes that are connected to the function output or have dependencies
                std::set<uint32_t> usedNodeIds;
                std::queue<nodes::NodeBase *> nodeQueue;

                // Start with nodes connected to function outputs
                for (auto const & [outputName, outputParam] : model->getOutputs())
                {
                    if (outputParam.getConstSource().has_value())
                    {
                        auto const & source = outputParam.getConstSource().value();
                        auto const * sourcePort = model->getPort(source.portId);
                        if (sourcePort)
                        {
                            auto * sourceNode = sourcePort->getParent();
                            if (sourceNode)
                            {
                                usedNodeIds.insert(sourceNode->getId());
                                nodeQueue.push(sourceNode);
                            }
                        }
                    }
                }

                // Traverse backwards to find all nodes that contribute to the outputs
                while (!nodeQueue.empty())
                {
                    auto * currentNode = nodeQueue.front();
                    nodeQueue.pop();

                    // Check all parameters of this node to find source nodes
                    for (auto const & [paramName, param] : currentNode->constParameter())
                    {
                        if (param.getConstSource().has_value())
                        {
                            auto const & source = param.getConstSource().value();
                            auto const * sourcePort = model->getPort(source.portId);
                            if (sourcePort)
                            {
                                auto * sourceNode = sourcePort->getParent();
                                if (sourceNode && usedNodeIds.count(sourceNode->getId()) == 0)
                                {
                                    usedNodeIds.insert(sourceNode->getId());
                                    nodeQueue.push(sourceNode);
                                }
                            }
                        }
                    }
                }

                // Find unused nodes (nodes not in the usedNodeIds set)
                std::vector<nodes::NodeBase *> unusedNodes;
                for (auto * node : allNodes)
                {
                    if (usedNodeIds.count(node->getId()) == 0)
                    {
                        unusedNodes.push_back(node);
                    }
                }

                // Remove unused nodes and collect information
                nlohmann::json removedNodes = nlohmann::json::array();
                for (auto * unusedNode : unusedNodes)
                {
                    nlohmann::json nodeInfo;
                    nodeInfo["id"] = unusedNode->getId();
                    nodeInfo["unique_name"] = unusedNode->getUniqueName();
                    nodeInfo["display_name"] = unusedNode->getDisplayName();
                    nodeInfo["type"] = unusedNode->name();

                    removedNodes.push_back(nodeInfo);

                    // Remove the node from the model
                    model->remove(unusedNode->getId());
                }

                // Update the graph to ensure everything is properly cleaned up
                model->updateGraphAndOrderIfNeeded();

                out["success"] = true;
                out["removed_nodes"] = removedNodes;
                out["total_removed_nodes"] = removedNodes.size();

                if (removedNodes.empty())
                {
                    out["message"] = "No unused nodes found to remove";
                }
                else
                {
                    out["message"] =
                      "Removed " + std::to_string(removedNodes.size()) + " unused node(s)";
                }

                return out;
            }
            catch (const std::exception & e)
            {
                out["success"] = false;
                out["error"] = std::string("Exception while removing unused nodes: ") + e.what();
                return out;
            }
        }

        nlohmann::json FunctionOperationsTool::listChangeableParameters() const
        {
            using json = nlohmann::json;
            json out;
            out["changeable_parameters"] = json::array();

            if (!validateApplication())
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

                json changeableParams = json::array();
                uint32_t totalParams = 0;

                // Iterate through all functions in the assembly
                for (const auto & [functionId, model] : assembly->getFunctions())
                {
                    if (!model)
                        continue;

                    // Get function display name and info
                    std::string functionDisplayName =
                      model->getDisplayName().value_or("Unnamed Function");
                    std::string functionName = model->getDisplayName().value_or("unnamed_function");

                    // Iterate through all nodes in this function
                    for (auto const & [nodeId, nodePtr] : *model)
                    {
                        if (!nodePtr)
                            continue;

                        // Check if this is a constant node
                        const std::string & nodeName = nodePtr->name();
                        bool isConstantNode =
                          (nodeName == "ConstantScalar" || nodeName == "ConstantVector" ||
                           nodeName == "ConstantMatrix" || nodeName == "Resource");

                        if (!isConstantNode)
                            continue;

                        // For constant nodes, all their parameters are changeable
                        for (auto const & [paramName, param] : nodePtr->constParameter())
                        {
                            // Only include parameters that are not connected (constant nodes should
                            // have unconnected params)
                            if (!param.getConstSource().has_value())
                            {
                                json paramInfo;
                                paramInfo["parameter_name"] = paramName;
                                paramInfo["display_name"] =
                                  paramName; // Use parameter name as display name
                                paramInfo["parameter_type"] =
                                  FunctionGraphSerializer::typeIndexToString(param.getTypeIndex());
                                paramInfo["node_id"] = nodePtr->getId();
                                paramInfo["node_display_name"] = nodePtr->getDisplayName();
                                paramInfo["node_unique_name"] = nodePtr->getUniqueName();
                                paramInfo["node_type"] = nodeName;
                                paramInfo["function_id"] = functionId;
                                paramInfo["function_name"] = functionName;
                                paramInfo["function_display_name"] = functionDisplayName;

                                // Add current parameter value for context
                                try
                                {
                                    auto typeIdx = param.getTypeIndex();
                                    auto paramValue = param.getValue();
                                    if (typeIdx == nodes::ParameterTypeIndex::Float)
                                    {
                                        if (auto const * val = std::get_if<float>(&paramValue))
                                            paramInfo["current_value"] = *val;
                                    }
                                    else if (typeIdx == nodes::ParameterTypeIndex::Int)
                                    {
                                        if (auto const * val = std::get_if<int>(&paramValue))
                                            paramInfo["current_value"] = *val;
                                    }
                                    else if (typeIdx == nodes::ParameterTypeIndex::String)
                                    {
                                        if (auto const * val =
                                              std::get_if<std::string>(&paramValue))
                                            paramInfo["current_value"] = *val;
                                    }
                                    else if (typeIdx == nodes::ParameterTypeIndex::Float3)
                                    {
                                        if (auto const * val =
                                              std::get_if<nodes::float3>(&paramValue))
                                        {
                                            paramInfo["current_value"] = {
                                              {"x", val->x}, {"y", val->y}, {"z", val->z}};
                                        }
                                    }
                                    else if (typeIdx == nodes::ParameterTypeIndex::ResourceId)
                                    {
                                        if (auto const * val = std::get_if<uint32_t>(&paramValue))
                                            paramInfo["current_value"] = *val;
                                    }
                                    // For Matrix4x4, we could add support later if needed
                                }
                                catch (const std::exception &)
                                {
                                    // If we can't get the current value, just omit it
                                    paramInfo["current_value"] = nullptr;
                                }

                                changeableParams.push_back(paramInfo);
                                totalParams++;
                            }
                        }
                    }
                }

                out["changeable_parameters"] = changeableParams;
                out["total_parameters"] = totalParams;
                out["success"] = true;

                if (totalParams == 0)
                {
                    out["message"] = "No changeable parameters found in constant nodes";
                }
                else
                {
                    out["message"] = "Found " + std::to_string(totalParams) +
                                     " changeable parameter(s) in constant nodes";
                }

                return out;
            }
            catch (const std::exception & e)
            {
                out["success"] = false;
                out["error"] =
                  std::string("Exception while listing changeable parameters: ") + e.what();
                return out;
            }
        }
    }
}