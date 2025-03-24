#include "BuildItemView.h"

#include "Document.h"
#include "MeshResource.h"
#include "ResourceManager.h"
#include "Widgets.h"
#include "imgui.h"
#include <imgui_stdlib.h>
#include <io/3mf/ResourceIdUtil.h>
#include <nodes/ModelUtils.h>

namespace gladius::ui
{
    namespace
    {
        std::string getBuildItemName(const Lib3MF::PBuildItem& buildItem)
        {
            try
            {
                // Get object resource
                auto object = buildItem->GetObjectResource();
                if (object)
                {
                    // Try to get part name
                    std::string partName = object->GetName();
                    std::string partNumber = object->GetPartNumber();
                    if (!partName.empty())
                    {
                        return fmt::format("{} (BuildItem #{})", 
                            partName, buildItem->GetObjectResourceID());
                    }
                    else if (!partNumber.empty())
                    {
                        return fmt::format("PN:{} (BuildItem #{})", 
                            partNumber, buildItem->GetObjectResourceID());
                    }
                }
                return fmt::format("BuildItem #{}", buildItem->GetObjectResourceID());
            }
            catch (...)
            {
                // Handle errors gracefully
                return fmt::format("BuildItem (unknown)");
            }
        }

        bool renderBuildItemProperties(const Lib3MF::PBuildItem& buildItem,
                                      SharedDocument document,
                                      Lib3MF::PModel model3mf)
        {
            bool propertiesChanged = false;

            if (ImGui::BeginTable(
                  "BuildItemProperties", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Object:");
                ImGui::TableNextColumn();
                propertiesChanged |= BuildItemView::renderObjectDropdown(
                    document, model3mf, buildItem);

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Transform:");
                ImGui::TableNextColumn();
                propertiesChanged |= BuildItemView::renderTransformControls(
                    document, model3mf, buildItem);

                // Part number
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Part Number:");
                ImGui::TableNextColumn();
                try
                {
                    auto object = buildItem->GetObjectResource();
                    if (object)
                    {
                        std::string partNumber = object->GetPartNumber();
                        if (ImGui::InputText("##PartNumber", &partNumber, ImGuiInputTextFlags_None))
                        {
                            try
                            {
                                document->update3mfModel();
                                object->SetPartNumber(partNumber);
                                document->markFileAsChanged();
                                document->updateDocumenFrom3mfModel();
                                propertiesChanged = true;
                            }
                            catch (...)
                            {
                                // Handle errors silently
                            }
                        }
                    }
                }
                catch (...)
                {
                    // Handle errors silently
                }

                ImGui::EndTable();
            }

            return propertiesChanged;
        }
    }

    bool BuildItemView::render(SharedDocument document) const
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
        // Add "Add Build Item" button
        if (ImGui::Button("Add Build Item"))
        {
            try
            {
                document->update3mfModel();
                // Get default mesh object if available
                Lib3MF::PMeshObject defaultObject;
                auto resourceIterator = model3mf->GetResources();
                while (resourceIterator->MoveNext())
                {
                    auto resource = resourceIterator->GetCurrent();
                    Lib3MF::PMeshObject meshObject = std::dynamic_pointer_cast<Lib3MF::CMeshObject>(resource);
                    if (meshObject && meshObject->GetType() == Lib3MF::eObjectType::Model)
                    {
                        defaultObject = meshObject;
                        break;
                    }
                }

                if (defaultObject)
                {
                    // Create a new build item with identity transform
                    Lib3MF::sTransform transform;
                    // Initialize with identity matrix
                    for (int i = 0; i < 4; i++)
                    {
                        for (int j = 0; j < 3; j++)
                        {
                            transform.m_Fields[i][j] = (i == j) ? 1.0f : 0.0f;
                        }
                    }
                    // Convert PMeshObject to PObject before adding to build item
                    Lib3MF::PObject objectResource = std::static_pointer_cast<Lib3MF::CObject>(defaultObject);
                    model3mf->AddBuildItem(objectResource, transform);
                }
                else
                {
                    // No mesh available - just create an empty build item that user can configure
                    Lib3MF::sTransform transform;
                    // Initialize with identity matrix
                    for (int i = 0; i < 4; i++)
                    {
                        for (int j = 0; j < 3; j++)
                        {
                            transform.m_Fields[i][j] = (i == j) ? 1.0f : 0.0f;
                        }
                    }
                    // We still need an object, so find or create a minimal one
                    auto minimalObject = model3mf->AddMeshObject();
                    // Convert PMeshObject to PObject before adding to build item
                    Lib3MF::PObject objectResource = std::static_pointer_cast<Lib3MF::CObject>(minimalObject);
                    model3mf->AddBuildItem(objectResource, transform);
                }
                
                document->markFileAsChanged();
                document->updateDocumenFrom3mfModel();
                propertiesChanged = true;
            }
            catch (...)
            {
                // Handle errors silently
            }
        }

        ImGui::Unindent();

        ImGuiTreeNodeFlags const baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                             ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                             ImGuiTreeNodeFlags_SpanAvailWidth;

        // Iterate through build items
        try
        {
            auto buildItemIterator = model3mf->GetBuildItems();
            while (buildItemIterator->MoveNext())
            {
                auto buildItem = buildItemIterator->GetCurrent();
                if (!buildItem)
                {
                    continue;
                }

                std::string buildItemName = getBuildItemName(buildItem);

                ImGui::BeginGroup();
                if (ImGui::TreeNodeEx(buildItemName.c_str(), baseFlags))
                {
                    // Delete button
                    if (ImGui::Button("Delete"))
                    {
                        try
                        {
                            document->update3mfModel();
                            model3mf->RemoveBuildItem(buildItem);
                            document->markFileAsChanged();
                            document->updateDocumenFrom3mfModel();
                            propertiesChanged = true;
                            ImGui::TreePop();
                            ImGui::EndGroup();
                            frameOverlay(ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
                            continue; // Skip the rest for this deleted item
                        }
                        catch (...)
                        {
                            // Handle errors silently
                        }
                    }

                    bool currentPropertiesChanged = renderBuildItemProperties(buildItem, document, model3mf);
                    propertiesChanged = propertiesChanged || currentPropertiesChanged;
                    ImGui::TreePop();
                }
                ImGui::EndGroup();
                frameOverlay(ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
            }
        }
        catch (...)
        {
            // Handle any exceptions from iteration
        }

        return propertiesChanged;
    }

    bool BuildItemView::renderObjectDropdown(
        SharedDocument document,
        Lib3MF::PModel model3mf,
        Lib3MF::PBuildItem buildItem)
    {
        bool propertiesChanged = false;

        ImGui::PushID("ObjectDropdown");
        
        Lib3MF::PObject currentObject;
        try
        {
            currentObject = buildItem->GetObjectResource();
        }
        catch (...)
        {
            // Handle errors silently
        }

        std::string currentObjectName =
            currentObject ? fmt::format("Object #{}", currentObject->GetResourceID()) : "Please select";

        if (ImGui::BeginCombo("", currentObjectName.c_str()))
        {
            auto resourceIterator = model3mf->GetResources();
            while (resourceIterator->MoveNext())
            {
                auto resource = resourceIterator->GetCurrent();
                Lib3MF::PObject object = std::dynamic_pointer_cast<Lib3MF::CObject>(resource);
                if (!object || object->GetType() != Lib3MF::eObjectType::Model)
                {
                    continue;
                }

                std::string objectName = object->GetName();
                std::string displayName = objectName.empty() 
                    ? fmt::format("Object #{}", object->GetResourceID())
                    : fmt::format("{} (#{})", objectName, object->GetResourceID());

                bool isSelected = (currentObject && currentObject->GetResourceID() == object->GetResourceID());
                if (ImGui::Selectable(displayName.c_str(), isSelected))
                {
                    try
                    {
                        document->update3mfModel();
                        
                        // Get current transform
                        Lib3MF::sTransform transform;
                        try
                        {
                            transform = buildItem->GetObjectTransform();
                        }
                        catch (...)
                        {
                            // Initialize with identity matrix if we can't get current transform
                            for (int i = 0; i < 4; i++)
                            {
                                for (int j = 0; j < 3; j++)
                                {
                                    transform.m_Fields[i][j] = (i == j) ? 1.0f : 0.0f;
                                }
                            }
                        }
                        
                        // Remove current build item and create a new one with the selected object
                        model3mf->RemoveBuildItem(buildItem);
                        model3mf->AddBuildItem(object, transform);
                        
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

    bool BuildItemView::renderTransformControls(
        SharedDocument document,
        Lib3MF::PModel model3mf,
        Lib3MF::PBuildItem buildItem)
    {
        bool propertiesChanged = false;

        ImGui::PushID("TransformControls");
        
        try
        {
            Lib3MF::sTransform transform = buildItem->GetObjectTransform();
            
            // Create a 4x3 matrix UI for editing
            if (ImGui::BeginTable("TransformMatrix", 3, ImGuiTableFlags_Borders))
            {
                for (int i = 0; i < 4; i++)
                {
                    ImGui::TableNextRow();
                    for (int j = 0; j < 3; j++)
                    {
                        ImGui::TableNextColumn();
                        
                        char label[32];
                        snprintf(label, sizeof(label), "##M%d%d", i, j);
                        
                        float value = transform.m_Fields[i][j];
                        if (ImGui::InputFloat(label, &value, 0.0f, 0.0f, "%.3f"))
                        {
                            try
                            {
                                document->update3mfModel();
                                transform.m_Fields[i][j] = value;
                                buildItem->SetObjectTransform(transform);
                                document->markFileAsChanged();
                                document->updateDocumenFrom3mfModel();
                                propertiesChanged = true;
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
            
            // Add buttons for common transformations
            if (ImGui::Button("Reset to Identity"))
            {
                try
                {
                    document->update3mfModel();
                    
                    // Set identity matrix
                    for (int i = 0; i < 4; i++)
                    {
                        for (int j = 0; j < 3; j++)
                        {
                            transform.m_Fields[i][j] = (i == j) ? 1.0f : 0.0f;
                        }
                    }
                    
                    buildItem->SetObjectTransform(transform);
                    document->markFileAsChanged();
                    document->updateDocumenFrom3mfModel();
                    propertiesChanged = true;
                }
                catch (...)
                {
                    // Handle errors silently
                }
            }
        }
        catch (...)
        {
            ImGui::TextUnformatted("Error: Unable to access transform");
        }
        
        ImGui::PopID();

        return propertiesChanged;
    }
}