#include "LevelSetView.h"

#include "Document.h"
#include "MeshResource.h"
#include "ResourceManager.h"
#include "Widgets.h"
#include "imgui.h"
#include <io/3mf/ResourceIdUtil.h>
#include <lib3mf/Cpp/lib3mf_implicit.hpp>
#include <nodes/ModelUtils.h>

namespace gladius::ui
{
    namespace
    {
        std::string getLevelSetName(const Lib3MF::PLevelSet & levelSet)
        {
            try
            {
                auto metaDataGroup = levelSet->GetMetaDataGroup();
                if (metaDataGroup)
                {
                    auto count = metaDataGroup->GetMetaDataCount();
                    for (Lib3MF_uint32 i = 0; i < count; i++)
                    {
                        auto metaData = metaDataGroup->GetMetaData(i);
                        if (metaData->GetName() == "name")
                        {
                            return metaData->GetValue();
                        }
                    }
                }
            }
            catch (...)
            {
                // Ignore metadata errors
            }
            return "";
        }

        void renderLevelSetProperties(const Lib3MF::PLevelSet & levelSet,
                                      SharedDocument document,
                                      Lib3MF::PModel model3mf)
        {
            if (ImGui::BeginTable(
                  "LevelSetProperties", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Function:");
                ImGui::TableNextColumn();
                LevelSetView::renderFunctionDropdown(
                  document, model3mf, levelSet, levelSet->GetFunction());

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Channel:");
                ImGui::TableNextColumn();
                LevelSetView::renderChannelDropdown(document, model3mf, levelSet);

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Min Feature Size:");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(fmt::format("{}", levelSet->GetMinFeatureSize()).c_str());

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Mesh BBox Only:");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(levelSet->GetMeshBBoxOnly() ? "true" : "false");

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Fallback Value:");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(fmt::format("{}", levelSet->GetFallBackValue()).c_str());

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Mesh:");
                ImGui::TableNextColumn();
                LevelSetView::renderMeshDropdown(document, model3mf, levelSet);

                ImGui::EndTable();
            }
        }
    }

    void LevelSetView::render(SharedDocument document) const
    {
        if (!document)
        {
            return;
        }

        auto model3mf = document->get3mfModel();
        if (!model3mf)
        {
            return;
        }

        auto resourceIterator = model3mf->GetResources();

        ImGuiTreeNodeFlags const baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                             ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                             ImGuiTreeNodeFlags_SpanAvailWidth;

        while (resourceIterator->MoveNext())
        {
            auto resource = resourceIterator->GetCurrent();
            if (!resource)
            {
                continue;
            }

            Lib3MF::PLevelSet levelSet = std::dynamic_pointer_cast<Lib3MF::CLevelSet>(resource);
            if (!levelSet)
            {
                continue;
            }

            std::string levelSetName = getLevelSetName(levelSet);
            auto name =
              levelSetName.empty()
                ? fmt::format("LevelSet #{}", levelSet->GetResourceID())
                : fmt::format("{} (LevelSet #{})", levelSetName, levelSet->GetResourceID());

            ImGui::BeginGroup();
            if (ImGui::TreeNodeEx(name.c_str(), baseFlags))
            {
                renderLevelSetProperties(levelSet, document, model3mf);
                ImGui::TreePop();
            }
            ImGui::EndGroup();
            frameOverlay(ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
        }
    }

    void LevelSetView::renderFunctionDropdown(SharedDocument document,
                                              Lib3MF::PModel model3mf,
                                              Lib3MF::PLevelSet levelSet,
                                              Lib3MF::PFunction function)
    {
        ImGui::PushID("FunctionDropdown");
        if (ImGui::BeginCombo("", function->GetDisplayName().c_str()))
        {
            // Get available functions from the document's assembly
            auto assembly = document->getAssembly();
            if (assembly)
            {
                auto & functions = assembly->getFunctions();

                // Iterate through available functions/models
                for (auto & [functionId, functionModel] : functions)
                {
                    if (!functionModel)
                    {
                        continue;
                    }

                    // Skip the assembly model
                    if (functionModel->getResourceId() ==
                        assembly->assemblyModel()->getResourceId())
                    {
                        continue;
                    }

                    // Skip functions that are not qualified as level sets
                    if (!nodes::isQualifiedForLevelset(*functionModel))
                    {
                        continue;
                    }

                    // Get display name or model name as fallback
                    std::string displayName =
                      functionModel->getDisplayName().value_or(functionModel->getModelName());

                    // find the unique resource id of the function in 3MF model
                    auto uniqueFunctionResourceId =
                      gladius::io::resourceIdToUniqueResourceId(model3mf, functionId);
                    if (uniqueFunctionResourceId == 0)
                    {
                        continue;
                    }

                    bool isSelected = (uniqueFunctionResourceId == function->GetResourceID());
                    if (ImGui::Selectable(fmt::format("#{} - {}", functionId, displayName).c_str(),
                                          isSelected))
                    {
                        // Update the function in the levelset
                        try
                        {
                            // Find the model in the 3MF
                            auto resource = model3mf->GetResourceByID(uniqueFunctionResourceId);

                            // try to cast it to a function
                            auto functionResource =
                              dynamic_cast<Lib3MF::CFunction *>(resource.get());
                            if (functionResource)
                            {
                                levelSet->SetFunction(functionResource);
                                document->markFileAsChanged();
                            }
                        }
                        catch (...)
                        {
                            // Handle errors silently
                        }
                    }

                    // Set the initial focus when opening the combo
                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }

            ImGui::EndCombo();
        }
        ImGui::PopID();
    }

    void LevelSetView::renderChannelDropdown(SharedDocument document,
                                             Lib3MF::PModel model3mf,
                                             Lib3MF::PLevelSet levelSet)
    {
        auto function = levelSet->GetFunction();
        if (!function)
        {
            return;
        }

        auto functionModel = document->getAssembly()->findModel(
          io::uniqueResourceIdToResourceId(model3mf, function->GetResourceID()));
        if (!functionModel)
        {
            return;
        }

        auto & endNodeParameters = functionModel->getOutputs();

        ImGui::PushID("ChannelDropdown");
        if (ImGui::BeginCombo("", levelSet->GetChannelName().c_str()))
        {
            for (const auto & [paramName, param] : endNodeParameters)
            {
                bool isSelected = (paramName == levelSet->GetChannelName());
                if (ImGui::Selectable(paramName.c_str(), isSelected))
                {
                    levelSet->SetChannelName(paramName);
                    document->markFileAsChanged();
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        ImGui::PopID();
    }

    void LevelSetView::renderMeshDropdown(SharedDocument document,
                                          Lib3MF::PModel model3mf,
                                          Lib3MF::PLevelSet levelSet)
    {
        ImGui::PushID("MeshDropdown");
        auto currentMesh = levelSet->GetMesh();
        std::string currentMeshName =
          currentMesh ? fmt::format("Mesh #{}", currentMesh->GetModelResourceID()) : "None";

        if (ImGui::BeginCombo("", currentMeshName.c_str()))
        {
            // Iterate through available meshes in the document's resource manager
            auto & resourceManager = document->getResourceManager();
            for (auto const & [key, resource] : resourceManager.getResourceMap())
            {
                auto const * meshResource = dynamic_cast<MeshResource const *>(resource.get());
                if (!meshResource)
                {
                    continue;
                }

                auto meshName = fmt::format("Mesh #{}", key.getResourceId().value_or(-1));
                bool isSelected =
                  (currentMesh && key.getResourceId() == currentMesh->GetResourceID());

                if (ImGui::Selectable(meshName.c_str(), isSelected))
                {
                    try
                    {
                        auto lib3mfUniqueResourceId = gladius::io::resourceIdToUniqueResourceId(
                          model3mf, key.getResourceId().value());

                        auto mesh = model3mf->GetResourceByID(lib3mfUniqueResourceId);
                        auto meshResource = dynamic_cast<Lib3MF::CMeshObject *>(mesh.get());
                        if (meshResource)
                        {
                            levelSet->SetMesh(meshResource);
                            document->markFileAsChanged();
                        }
                    }
                    catch (...)
                    {
                        // Handle errors silently
                    }
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
        ImGui::PopID();
    }
}
