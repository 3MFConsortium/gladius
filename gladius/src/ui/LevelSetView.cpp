#include "LevelSetView.h"

#include "Document.h"
#include "Widgets.h"
#include <io/3mf/ResourceIdUtil.h>
#include <nodes/ModelUtils.h>

namespace gladius::ui
{
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

            // Try to get a name for the levelset from metadata
            std::string levelSetName = "";
            // Check if the level set has a metadata group
            // and try to find the name in it
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
                            levelSetName = metaData->GetValue();
                            break;
                        }
                    }
                }
            }
            catch (...)
            {
                // Ignore metadata errors
            }

            // Create display name with both name and ID
            auto name = levelSetName.empty() 
                ? fmt::format("LevelSet #{}", levelSet->GetResourceID())
                : fmt::format("{} (LevelSet #{})", levelSetName, levelSet->GetResourceID());
                
            ImGui::BeginGroup();
            if (ImGui::TreeNodeEx(name.c_str(), baseFlags))
            {
                // Get the function resource
                auto function = levelSet->GetFunction();
               
                // Display function selection dropdown
                renderFunctionDropdown(document, model3mf, levelSet, function);
                
                // Display channel selection dropdown
                renderChannelDropdown(document, model3mf, levelSet);

                ImGui::Text("Channel: %s", levelSet->GetChannelName().c_str());
                ImGui::Text("Min Feature Size: %f", levelSet->GetMinFeatureSize());
                ImGui::Text("Mesh BBox Only: %s", levelSet->GetMeshBBoxOnly() ? "true" : "false");
                ImGui::Text("Fallback Value: %f", levelSet->GetFallBackValue());

                // Display mesh information
                try {
                    auto mesh = levelSet->GetMesh();
                    if (mesh) {
                        ImGui::Separator();
                        ImGui::Text("Mesh ID: #%u", mesh->GetResourceID());
                    }
                }
                catch (...) {
                    // Mesh information not available
                    ImGui::Text("Mesh: Not available");
                }

                ImGui::TreePop();
            }
            ImGui::EndGroup();
            frameOverlay(ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
        }
    }

    void LevelSetView::renderFunctionDropdown(
        SharedDocument document, 
        Lib3MF::PModel model3mf, 
        Lib3MF::PLevelSet levelSet, 
        Lib3MF::PFunction function) const
    {
        if (ImGui::BeginCombo("Function", function->GetDisplayName().c_str()))
        {
            // Get available functions from the document's assembly
            auto assembly = document->getAssembly();
            if (assembly)
            {
                auto& functions = assembly->getFunctions();
                
                // Iterate through available functions/models
                for (auto& [functionId, functionModel] : functions)
                {
                    if (!functionModel)
                    {
                        continue;
                    }

                    // Skip the assembly model
                    if (functionModel->getResourceId() == assembly->assemblyModel()->getResourceId())
                    {
                        continue;
                    }

                    // Skip functions that are not qualified as level sets
                    if (!nodes::isQualifiedForLevelset(*functionModel))
                    {
                        continue;
                    }
                    
                    // Get display name or model name as fallback
                    std::string displayName = functionModel->getDisplayName().value_or(functionModel->getModelName());

                    // find the unique resource id of the function in 3MF model
                    auto uniqueFunctionResourceId = gladius::io::resourceIdToUniqueResourceId(model3mf, functionId);
                    if (uniqueFunctionResourceId == 0)
                    {
                        continue;
                    }
                    
                    bool isSelected = (uniqueFunctionResourceId == function->GetResourceID());
                    if (ImGui::Selectable(fmt::format("#{} - {}", functionId, displayName).c_str(), isSelected))
                    {
                        // Update the function in the levelset
                        try
                        {
                            // Find the model in the 3MF
                            auto resource = model3mf->GetResourceByID(uniqueFunctionResourceId);

                            // try to cast it to a function
                            auto functionResource = dynamic_cast<Lib3MF::CFunction*>(resource.get());
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
    }

    void LevelSetView::renderChannelDropdown(
        SharedDocument document,
        Lib3MF::PModel model3mf,
        Lib3MF::PLevelSet levelSet) const
    {
        auto function = levelSet->GetFunction();
        if (!function)
        {
            return;
        }

        auto functionModel = document->getAssembly()->findModel(io::uniqueResourceIdToResourceId(model3mf, function->GetResourceID()));
        if (!functionModel)
        {
            return;
        }

        auto& endNodeParameters = functionModel->getOutputs();

        if (ImGui::BeginCombo("Channel", levelSet->GetChannelName().c_str()))
        {
            for (const auto& [paramName, param] : endNodeParameters)
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
    }
}
