/**
 * @file ResourceManagementTool.cpp
 * @brief Implementation of ResourceManagementTool for 3MF resource operations
 */

#include "ResourceManagementTool.h"
#include "../../Application.h"
#include "../../Document.h"
#include "../../io/3mf/ResourceIdUtil.h"
#include <array>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace gladius
{
    namespace mcp::tools
    {
        ResourceManagementTool::ResourceManagementTool(Application * app)
            : MCPToolBase(app)
        {
        }

        std::pair<bool, uint32_t> ResourceManagementTool::createLevelSet(uint32_t functionId)
        {
            if (!validateActiveDocument())
            {
                return {false, 0};
            }

            try
            {
                auto document = m_application->getCurrentDocument();
                if (!document)
                {
                    setErrorMessage("No active document available");
                    return {false, 0};
                }

                // Update the 3MF model to ensure all resources are up to date
                document->update3mfModel();

                // Update the assembly to process function graphs and outputs
                document->getAssembly()->updateInputsAndOutputs();

                auto model3mf = document->get3mfModel();
                if (!model3mf)
                {
                    setErrorMessage("No 3MF model available");
                    return {false, 0};
                }

                // Get the function resource by ID
                // Note: Gladius uses lib3mf ModelResourceID as its ResourceId.
                // lib3mf::GetResourceByID expects the UniqueResourceID. Convert before lookup to
                // avoid mixing IDs.
                Lib3MF_uint32 uniqueFunctionId = io::resourceIdToUniqueResourceId(
                  model3mf, static_cast<gladius::ResourceId>(functionId));
                if (uniqueFunctionId == 0)
                {
                    setErrorMessage("Could not resolve unique resource id for function id " +
                                    std::to_string(functionId));
                    return {false, 0};
                }

                auto resource = model3mf->GetResourceByID(uniqueFunctionId);
                auto functionResource = dynamic_cast<Lib3MF::CFunction *>(resource.get());
                if (!functionResource)
                {
                    setErrorMessage("Function with ID " + std::to_string(functionId) +
                                    " not found or is not a function");
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

                // Get the mesh ModelResourceID before creating the levelset (consistent with
                // Gladius IDs)
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
                document->updateDocumentFrom3mfModel();

                setErrorMessage(
                  "Level set created successfully from function ID: " + std::to_string(functionId) +
                  " using mesh ID: " + std::to_string(meshId));
                return {true, levelSetId};
            }
            catch (const std::exception & e)
            {
                setErrorMessage("Exception during level set creation: " + std::string(e.what()));
                return {false, 0};
            }
        }

        std::pair<bool, uint32_t>
        ResourceManagementTool::createImage3DFunction(const std::string & name,
                                                      const std::string & imagePath,
                                                      float valueScale,
                                                      float valueOffset)
        {
            if (!validateActiveDocument())
            {
                return {false, 0};
            }

            // TODO: Implement actual Image3D function creation when API is available
            // For now, create a function placeholder and return resource ID
            setErrorMessage("Image3D function '" + name + "' would be created from: " + imagePath +
                            " with scale: " + std::to_string(valueScale) +
                            ", offset: " + std::to_string(valueOffset));

            // Create a simple placeholder function for now
            // Note: This would need access to FunctionOperationsTool, but we'll keep it simple for
            // now
            return {false, 0}; // Placeholder - needs proper implementation
        }

        std::pair<bool, uint32_t>
        ResourceManagementTool::createVolumetricColor(uint32_t functionId,
                                                      const std::string & channel)
        {
            if (!validateActiveDocument())
            {
                return {false, 0};
            }

            // TODO: Implement actual volumetric color creation when API is available
            setErrorMessage("Volumetric color data would be created from function ID: " +
                            std::to_string(functionId) + " using channel: " + channel);
            return {true, functionId + 2000}; // Placeholder: offset function ID for color data ID
        }

        std::pair<bool, uint32_t>
        ResourceManagementTool::createVolumetricProperty(const std::string & propertyName,
                                                         uint32_t functionId,
                                                         const std::string & channel)
        {
            if (!validateActiveDocument())
            {
                return {false, 0};
            }

            // TODO: Implement actual volumetric property creation when API is available
            setErrorMessage("Volumetric property '" + propertyName +
                            "' would be created from function ID: " + std::to_string(functionId) +
                            " using channel: " + channel);
            return {true,
                    functionId + 3000}; // Placeholder: offset function ID for property data ID
        }

        bool ResourceManagementTool::modifyLevelSet(uint32_t levelSetModelResourceId,
                                                    std::optional<uint32_t> functionModelResourceId,
                                                    std::optional<std::string> channel)
        {
            if (!validateActiveDocument())
            {
                return false;
            }

            try
            {
                auto document = m_application->getCurrentDocument();
                auto model = document->get3mfModel();
                if (!model)
                {
                    setErrorMessage("No 3MF model available");
                    return false;
                }

                // Resolve level set by ModelResourceID
                Lib3MF_uint32 uniqueLevelSetId = io::resourceIdToUniqueResourceId(
                  model, static_cast<gladius::ResourceId>(levelSetModelResourceId));
                if (uniqueLevelSetId == 0)
                {
                    setErrorMessage("Could not resolve levelset unique id");
                    return false;
                }
                auto lsRes = model->GetResourceByID(uniqueLevelSetId);
                auto levelSet = dynamic_cast<Lib3MF::CLevelSet *>(lsRes.get());
                if (!levelSet)
                {
                    setErrorMessage("Resource is not a levelset");
                    return false;
                }

                // Optionally update the function reference
                if (functionModelResourceId.has_value())
                {
                    Lib3MF_uint32 uniqueFnId = io::resourceIdToUniqueResourceId(
                      model, static_cast<gladius::ResourceId>(functionModelResourceId.value()));
                    if (uniqueFnId == 0)
                    {
                        setErrorMessage("Could not resolve function unique id");
                        return false;
                    }
                    auto fnRes = model->GetResourceByID(uniqueFnId);
                    auto fn = dynamic_cast<Lib3MF::CFunction *>(fnRes.get());
                    if (!fn)
                    {
                        setErrorMessage("Target resource is not a function");
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
                document->updateDocumentFrom3mfModel();
                setErrorMessage("Level set modified successfully");
                return true;
            }
            catch (const std::exception & e)
            {
                setErrorMessage("Exception while modifying level set: " + std::string(e.what()));
                return false;
            }
        }

        nlohmann::json ResourceManagementTool::removeUnusedResources()
        {
            nlohmann::json out;
            if (!validateActiveDocument())
            {
                out["success"] = false;
                out["removed_count"] = 0;
                out["error"] = getLastErrorMessage();
                return out;
            }

            try
            {
                auto document = m_application->getCurrentDocument();
                if (!document)
                {
                    out["success"] = false;
                    out["removed_count"] = 0;
                    out["error"] = "No active document";
                    return out;
                }

                // Ensure resource dependency graph is up-to-date, then delete all unused resources
                // Document::removeUnusedResources() already updates and logs internally
                std::size_t removed = document->removeUnusedResources();
                out["success"] = true;
                out["removed_count"] = static_cast<uint32_t>(removed);
                if (removed == 0)
                {
                    out["message"] = "No unused resources found";
                }
                return out;
            }
            catch (const std::exception & e)
            {
                out["success"] = false;
                out["removed_count"] = 0;
                out["error"] = std::string("Failed to remove unused resources: ") + e.what();
                return out;
            }
        }
    }
}
