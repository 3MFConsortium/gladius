#include "LevelSetView.h"

#include "Document.h"
#include "MeshResource.h"
#include "ResourceManager.h"
#include "Widgets.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <io/3mf/ResourceIdUtil.h>
#include <lib3mf_implicit.hpp>
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

        bool renderLevelSetProperties(const Lib3MF::PLevelSet & levelSet,
                                      SharedDocument document,
                                      Lib3MF::PModel model3mf)
        {
            bool propertiesChanged = false;

            if (ImGui::BeginTable(
                  "LevelSetProperties", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Function");
                ImGui::TableNextColumn();
                propertiesChanged |= LevelSetView::renderFunctionDropdown(
                  document, model3mf, levelSet, levelSet->GetFunction());

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Channel");
                ImGui::TableNextColumn();
                propertiesChanged |= LevelSetView::renderChannelDropdown(document, model3mf, levelSet);

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Min Feature Size");
                ImGui::TableNextColumn();
                {
                    float minFeatureSize = levelSet->GetMinFeatureSize();
                    if (ImGui::InputFloat("##MinFeatureSize", &minFeatureSize))
                    {
                        levelSet->SetMinFeatureSize(minFeatureSize);
                        propertiesChanged = true;
                    }
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Use Mesh only as Bounding Box");
                ImGui::TableNextColumn();
                {
                    bool meshBBoxOnly = levelSet->GetMeshBBoxOnly();
                    if (ImGui::Checkbox("##MeshBBoxOnly", &meshBBoxOnly))
                    {
                        levelSet->SetMeshBBoxOnly(meshBBoxOnly);
                        propertiesChanged = true;
                    }
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Fallback Value");
                ImGui::TableNextColumn();
                {
                    float fallbackValue = levelSet->GetFallBackValue();
                    if (ImGui::InputFloat("##FallbackValue", &fallbackValue))
                    {
                        levelSet->SetFallBackValue(fallbackValue);
                        propertiesChanged = true;
                    }
                }                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Mesh");
                ImGui::TableNextColumn();
                propertiesChanged |= LevelSetView::renderMeshDropdown(document, model3mf, levelSet);
                
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Volume Data");
                ImGui::TableNextColumn();
                propertiesChanged |= LevelSetView::renderVolumeDataDropdown(document, model3mf, levelSet);

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Part Number");
                ImGui::TableNextColumn();
                {                    // Cast to Object since only Objects have part numbers
                    auto object = std::dynamic_pointer_cast<Lib3MF::CObject>(levelSet);
                    if (object)
                    {
                        std::string partNumber = object->GetPartNumber();
                        if (ImGui::InputText("##PartNumber", &partNumber))
                        {
                            try
                            {
                                document->update3mfModel();
                                object->SetPartNumber(partNumber);
                                document->markFileAsChanged();
                            }
                            catch (...)
                            {
                                // Handle errors silently
                            }
                        }
                    }
                }

                ImGui::EndTable();
            }

            return propertiesChanged;
        }
    }

    bool LevelSetView::render(SharedDocument document) const
    {
        if (!document)
        {
            return false;
        }

        auto model3mf = document->get3mfModel();
        if (!model3mf)
        {
            return false;
        }

        bool propertiesChanged = false;

        ImGui::Indent();
        // Add "Add Levelset" button
        if (ImGui::Button("Add Levelset"))
        {
            try
            {
                document->update3mfModel();
                auto newLevelSet = model3mf->AddLevelSet();
                newLevelSet->SetMeshBBoxOnly(true);
                newLevelSet->SetMinFeatureSize(0.1f);
                newLevelSet->SetFallBackValue(0.0f);
                
                document->updateDocumenFrom3mfModel();
                propertiesChanged = true;
            }
            catch (...)
            {
                // Handle errors silently
            }
        }

        ImGui::Unindent();

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
                bool currentPropertiesChanged = renderLevelSetProperties(levelSet, document, model3mf);
                propertiesChanged = propertiesChanged || currentPropertiesChanged;
                ImGui::TreePop();
            }
            ImGui::EndGroup();
            frameOverlay(ImVec4(1.0f, 1.0f, 1.0f, 0.2f),
                        "Level Set Details\n\n"
                        "Configure this level set's mathematical properties and transforms.\n"
                        "Level sets define shapes using math functions instead of triangles,\n"
                        "which gives them smooth surfaces at any resolution.");
        }

        return propertiesChanged;
    }

    bool LevelSetView::renderFunctionDropdown(SharedDocument document,
                                              Lib3MF::PModel model3mf,
                                              Lib3MF::PLevelSet levelSet,
                                              Lib3MF::PFunction function)
    {
        bool propertiesChanged = false;

        ImGui::PushID("FunctionDropdown");
        std::string functionDisplayName =
          function ? function->GetDisplayName() : "Please select";
        if (ImGui::BeginCombo("", functionDisplayName.c_str()))
        {
            auto assembly = document->getAssembly();
            if (assembly)
            {
                auto & functions = assembly->getFunctions();

                for (auto & [functionId, functionModel] : functions)
                {
                    if (!functionModel)
                    {
                        continue;
                    }

                    if (functionModel->getResourceId() ==
                        assembly->assemblyModel()->getResourceId())
                    {
                        continue;
                    }

                    if (!nodes::isQualifiedForLevelset(*functionModel))
                    {
                        continue;
                    }

                    std::string displayName =
                      functionModel->getDisplayName().value_or(functionModel->getModelName());

                    auto uniqueFunctionResourceId =
                      gladius::io::resourceIdToUniqueResourceId(model3mf, functionId);
                    if (uniqueFunctionResourceId == 0)
                    {
                        continue;
                    }

                    bool isSelected = function ? (uniqueFunctionResourceId == function->GetResourceID()) : false;
                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                    if (ImGui::Selectable(fmt::format("#{} - {}", functionId, displayName).c_str(),
                                          isSelected))
                    {
                        try
                        {
                            document->update3mfModel();

                            auto resource = model3mf->GetResourceByID(uniqueFunctionResourceId);
                            auto functionResource =
                              dynamic_cast<Lib3MF::CFunction *>(resource.get());
                            if (functionResource)
                            {
                                levelSet->SetFunction(functionResource);
                                document->markFileAsChanged();
                                document->updateDocumenFrom3mfModel();
                                propertiesChanged = true;
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
            }

            ImGui::EndCombo();
        }
        ImGui::PopID();

        return propertiesChanged;
    }

    bool LevelSetView::renderChannelDropdown(SharedDocument document,
                                             Lib3MF::PModel model3mf,
                                             Lib3MF::PLevelSet levelSet)
    {
        bool propertiesChanged = false;

        auto function = levelSet->GetFunction();
        if (!function)
        {
            return propertiesChanged;
        }

        auto functionModel = document->getAssembly()->findModel(
          io::uniqueResourceIdToResourceId(model3mf, function->GetResourceID()));
        if (!functionModel)
        {
            return propertiesChanged;
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
                    try
                    {
                        document->update3mfModel();
                        levelSet->SetChannelName(paramName);
                        document->markFileAsChanged();
                        document->updateDocumenFrom3mfModel();
                        propertiesChanged = true;
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

        return propertiesChanged;
    }

    bool LevelSetView::renderMeshDropdown(SharedDocument document,
                                          Lib3MF::PModel model3mf,
                                          Lib3MF::PLevelSet levelSet)
    {
        bool propertiesChanged = false;

        ImGui::PushID("MeshDropdown");
        Lib3MF::PMeshObject currentMesh;
        try
        {
            currentMesh = levelSet->GetMesh();
        }
        catch (...)
        {
            // Handle errors silently
        }


        std::string currentMeshName =
          currentMesh ? fmt::format("Mesh #{}", currentMesh->GetModelResourceID()) : "Please select";

        if (ImGui::BeginCombo("", currentMeshName.c_str()))
        {
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
                        document->update3mfModel();

                        auto lib3mfUniqueResourceId = gladius::io::resourceIdToUniqueResourceId(
                          model3mf, key.getResourceId().value());

                        auto mesh = model3mf->GetResourceByID(lib3mfUniqueResourceId);
                        auto meshResource = dynamic_cast<Lib3MF::CMeshObject *>(mesh.get());
                        if (meshResource)
                        {
                            levelSet->SetMesh(meshResource);
                            document->markFileAsChanged();
                            document->updateDocumenFrom3mfModel();
                            propertiesChanged = true;
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

        return propertiesChanged;
    }

    bool LevelSetView::renderVolumeDataDropdown(SharedDocument document,
                                                Lib3MF::PModel model3mf,
                                                Lib3MF::PLevelSet levelSet)
    {
        bool propertiesChanged = false;

        ImGui::PushID("VolumeDataDropdown");
        
        // Get current VolumeData for this levelset if it exists
        Lib3MF::PVolumeData currentVolumeData;
        std::string currentVolumeDataName = "None";

        try
        {
            currentVolumeData = levelSet->GetVolumeData();
            if (currentVolumeData)
            {
                currentVolumeDataName =
                  fmt::format("VolumeData #{}", currentVolumeData->GetResourceID());
            }
        }
        catch (...)
        {
            // Handle errors silently - no VolumeData exists
        }

        if (ImGui::BeginCombo("##VolumeData", currentVolumeDataName.c_str()))
        {
            // Add "None" option to remove VolumeData
            bool isSelected = !currentVolumeData;
            if (ImGui::Selectable("None", isSelected))
            {
                try
                {
                    document->update3mfModel();
                    // Set VolumeData to nullptr to remove the association
                    levelSet->SetVolumeData(nullptr);
                    document->markFileAsChanged();
                    document->updateDocumenFrom3mfModel();
                    propertiesChanged = true;
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

            // List all available VolumeData resources
            auto resourceIterator = model3mf->GetResources();
            while (resourceIterator->MoveNext())
            {
                auto resource = resourceIterator->GetCurrent();
                if (!resource)
                {
                    continue;
                }

                auto volumeData = std::dynamic_pointer_cast<Lib3MF::CVolumeData>(resource);
                if (!volumeData)
                {
                    continue;
                }

                auto name = fmt::format("VolumeData #{}", volumeData->GetResourceID());
                isSelected = currentVolumeData &&
                             (currentVolumeData->GetResourceID() == volumeData->GetResourceID());

                if (ImGui::Selectable(name.c_str(), isSelected))
                {
                    try
                    {
                        document->update3mfModel();
                        levelSet->SetVolumeData(volumeData);
                        document->markFileAsChanged();
                        document->updateDocumenFrom3mfModel();
                        propertiesChanged = true;
                    }
                    catch (const std::exception & e)
                    {
                        if (document->getSharedLogger())
                        {
                            document->getSharedLogger()->addEvent(
                              {fmt::format("Failed to set VolumeData: {}", e.what()),
                               events::Severity::Error});
                        }
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

        return propertiesChanged;
    }
}
