/**
 * @file SceneHierarchyTool.cpp
 * @brief Implementation of MCP tool for scene inspection and hierarchy operations
 */

#include "SceneHierarchyTool.h"
#include "../../Application.h"
#include "../../Document.h"
#include <filesystem>

namespace gladius::mcp::tools
{
    SceneHierarchyTool::SceneHierarchyTool(Application * app)
        : MCPToolBase(app)
    {
    }

    nlohmann::json SceneHierarchyTool::getSceneHierarchy() const
    {
        nlohmann::json hierarchy;

        if (!validateApplication())
        {
            hierarchy["error"] = getLastErrorMessage();
            return hierarchy;
        }

        auto document = m_application->getCurrentDocument();
        if (!document)
        {
            setErrorMessage("No active document");
            hierarchy["error"] = getLastErrorMessage();
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
            setErrorMessage("Failed to get scene hierarchy: " + std::string(e.what()));
            hierarchy["error"] = getLastErrorMessage();
            hierarchy["success"] = false;
        }

        return hierarchy;
    }

    nlohmann::json SceneHierarchyTool::getDocumentInfo() const
    {
        nlohmann::json info;

        // Use the validation from base class and application instance
        bool hasDocument = validateActiveDocument();
        info["has_document"] = hasDocument;

        if (validateApplication())
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                auto filename = document->getCurrentAssemblyFilename();
                info["document_path"] = filename.has_value() ? filename.value().string() : "";
            }
            else
            {
                info["document_path"] = "";
            }

            // Get application info (these would delegate to ApplicationLifecycleTool if we had
            // access)
            info["application_name"] = "Gladius";
            info["application_version"] = "1.0.0"; // TODO: Get actual version
            info["application_status"] = "running";
        }
        else
        {
            info["document_path"] = "";
            info["application_name"] = "Gladius";
            info["application_version"] = "Unknown";
            info["application_status"] = "not_running";
        }

        if (hasDocument)
        {
            std::string path = info["document_path"];
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

    nlohmann::json SceneHierarchyTool::getModelBoundingBox() const
    {
        nlohmann::json out;
        if (!validateActiveDocument())
        {
            out["success"] = false;
            out["error"] = getLastErrorMessage();
            return out;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                out["success"] = false;
                out["error"] = "No active document";
                return out;
            }

            // Prefer the ComputeCore bbox if present; otherwise fall back to Document helper
            auto core = document->getCore();
            std::optional<BoundingBox> bboxOpt;
            if (core)
            {
                bboxOpt = core->getBoundingBox();
                if (!bboxOpt.has_value())
                {
                    // Ensure assembly/program are current and trigger preparation which updates
                    // bbox
                    try
                    {
                        document->updateFlatAssembly();
                        core->tryRefreshProgramProtected(document->getAssembly());
                    }
                    catch (...)
                    {
                        // Ignore, we'll try the document path below
                    }
                    if (!core->prepareImageRendering())
                    {
                        // Fall back to document's compute path
                        bboxOpt.reset();
                    }
                    else
                    {
                        bboxOpt = core->getBoundingBox();
                    }
                }
            }

            BoundingBox bbox{};
            if (bboxOpt.has_value())
            {
                bbox = bboxOpt.value();
            }
            else
            {
                // Document path computes/updates bbox as needed
                bbox = document->computeBoundingBox();
            }

            const float minx = bbox.min.x;
            const float miny = bbox.min.y;
            const float minz = bbox.min.z;
            const float maxx = bbox.max.x;
            const float maxy = bbox.max.y;
            const float maxz = bbox.max.z;
            const float sx = maxx - minx;
            const float sy = maxy - miny;
            const float sz = maxz - minz;
            const float cx = (minx + maxx) * 0.5f;
            const float cy = (miny + maxy) * 0.5f;
            const float cz = (minz + maxz) * 0.5f;
            const float diag = std::sqrt(sx * sx + sy * sy + sz * sz);
            const bool isValid = sx > 0 && sy > 0 && sz > 0;

            out["success"] = true;
            out["bounding_box"] = {{"min", {minx, miny, minz}},
                                   {"max", {maxx, maxy, maxz}},
                                   {"size", {sx, sy, sz}},
                                   {"center", {cx, cy, cz}},
                                   {"diagonal", diag},
                                   {"units", "mm"},
                                   {"is_valid", isValid}};
            return out;
        }
        catch (const std::exception & e)
        {
            out["success"] = false;
            out["error"] = std::string("Failed to get bounding box: ") + e.what();
            return out;
        }
    }

    std::vector<std::string> SceneHierarchyTool::listAvailableFunctions() const
    {
        std::vector<std::string> functions;

        if (!validateActiveDocument())
        {
            return functions; // Return empty vector if no active document
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                return functions; // Return empty vector if no document
            }

            // Get the assembly which contains all function resources
            auto assembly = document->getAssembly();
            if (!assembly)
            {
                return functions; // Return empty vector if no assembly
            }

            // Iterate through all functions in the assembly
            auto & modelMap = assembly->getFunctions();
            for (const auto & [resourceId, model] : modelMap)
            {
                if (!model)
                {
                    continue; // Skip null models
                }

                // Get function name, preferring display name over internal name
                std::string functionName;
                auto displayName = model->getDisplayName();
                if (displayName.has_value() && !displayName.value().empty())
                {
                    functionName = displayName.value();
                }
                else
                {
                    // Fallback to model name or generate from resource ID
                    auto & modelName = model->getModelName();
                    if (!modelName.empty())
                    {
                        functionName = modelName;
                    }
                    else
                    {
                        functionName = "function_" + std::to_string(resourceId);
                    }
                }

                functions.push_back(functionName);
            }
        }
        catch (const std::exception & /* e */)
        {
            // Log error if we had access to logging, but for now just return empty vector
            // Could potentially set an error message that could be retrieved later
            return functions;
        }

        return functions;
    }
} // namespace gladius::mcp::tools
