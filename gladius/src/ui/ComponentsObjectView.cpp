#include "ComponentsObjectView.h"

#include "Document.h"
#include "Widgets.h"
#include "imgui.h"
#include <imgui_stdlib.h>
#include <io/3mf/ResourceIdUtil.h>
#include <nodes/ModelUtils.h>

namespace gladius::ui
{
    namespace
    {
        std::string getComponentsObjectName(const Lib3MF::PComponentsObject& componentsObject)
        {
            try
            {
                std::string name = componentsObject->GetName();
                if (!name.empty())
                {
                    return fmt::format("{} (ComponentsObject #{})", 
                        name, componentsObject->GetResourceID());
                }
                return fmt::format("ComponentsObject #{}", componentsObject->GetResourceID());
            }
            catch (...)
            {
                // Handle errors gracefully
                return fmt::format("ComponentsObject (unknown)");
            }
        }

        std::string getComponentName(const Lib3MF::PComponent& component, int index)
        {
            try
            {
                auto object = component->GetObjectResource();
                if (object)
                {
                    std::string objectName = object->GetName();
                    if (!objectName.empty())
                    {
                        return fmt::format("Component {} - {} (#{}) ", 
                            index, objectName, object->GetResourceID());
                    }
                    return fmt::format("Component {} - Object #{}", 
                        index, object->GetResourceID());
                }
                return fmt::format("Component {} (unknown)", index);
            }
            catch (...)
            {
                return fmt::format("Component {} (error)", index);
            }
        }

        bool renderComponentProperties(const Lib3MF::PComponent& component,
                                      SharedDocument document,
                                      Lib3MF::PModel model3mf,
                                      int index)
        {
            bool propertiesChanged = false;

            if (ImGui::BeginTable(
                  "ComponentProperties", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Object:");
                ImGui::TableNextColumn();
                propertiesChanged |= ComponentsObjectView::renderObjectDropdown(
                    document, model3mf, component);


                // Add Part Number field
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Part Number:");
                ImGui::TableNextColumn();
                ImGui::PushID(index);
                try
                {
                    auto objectResource = component->GetObjectResource();
                    if (objectResource)
                    {
                        std::string partNumber = objectResource->GetPartNumber();
                        if (ImGui::InputText("##ComponentPartNumber", &partNumber, ImGuiInputTextFlags_None))
                        {
                            try
                            {
                                document->update3mfModel();
                                objectResource->SetPartNumber(partNumber);
                                document->markFileAsChanged();
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
                    ImGui::TextUnformatted("(Error retrieving part number)");
                }
                ImGui::PopID();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Transform:");
                ImGui::TableNextColumn();
                propertiesChanged |= ComponentsObjectView::renderTransformControls(
                    document, model3mf, component);

                ImGui::EndTable();
            }

            return propertiesChanged;
        }
    }

    bool ComponentsObjectView::render(SharedDocument document) const
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
        // Add "Add Components Object" button
        if (ImGui::Button("Add Components Object"))
        {
            try
            {
                document->update3mfModel();
                auto componentsObject = model3mf->AddComponentsObject();
                
                // Set a default name if needed
                componentsObject->SetName("New Components Object");
                
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

        // Iterate through all resources to find ComponentsObjects
        try
        {
            auto resourceIterator = model3mf->GetResources();
            while (resourceIterator->MoveNext())
            {
                auto resource = resourceIterator->GetCurrent();
                auto componentsObject = std::dynamic_pointer_cast<Lib3MF::CComponentsObject>(resource);
                if (!componentsObject)
                {
                    continue;
                }

                std::string componentsObjectName = getComponentsObjectName(componentsObject);

                ImGui::BeginGroup();
                if (ImGui::TreeNodeEx(componentsObjectName.c_str(), baseFlags))
                {
                    // Delete button
                    if (ImGui::Button("Delete"))
                    {
                        try
                        {
                            document->update3mfModel();
                            
                            // Check if this is referenced by other objects first
                            bool canDelete = true;
                            auto buildItemIterator = model3mf->GetBuildItems();
                            while (buildItemIterator->MoveNext())
                            {
                                auto buildItem = buildItemIterator->GetCurrent();
                                auto objectResource = buildItem->GetObjectResource();
                                if (objectResource && objectResource->GetResourceID() == componentsObject->GetResourceID())
                                {
                                    canDelete = false;
                                    break;
                                }
                            }
                              if (canDelete)
                            {
                                model3mf->RemoveResource(componentsObject.get());
                                document->markFileAsChanged();
                                document->updateDocumenFrom3mfModel();
                                propertiesChanged = true;
                            }
                            else
                            {
                                // Show error or warning about being referenced
                            }
                        }
                        catch (...)
                        {
                            // Handle errors silently
                        }
                    }
                    
                    // Part number
                    try
                    {
                        std::string partNumber = componentsObject->GetPartNumber();
                        ImGui::Text("Part Number:");
                        ImGui::SameLine();
                        if (ImGui::InputText("##PartNumber", &partNumber, ImGuiInputTextFlags_None))
                        {
                            try
                            {
                                document->update3mfModel();
                                componentsObject->SetPartNumber(partNumber);
                                document->markFileAsChanged();
                            }
                            catch (...)
                            {
                                // Handle errors silently
                            }
                        }
                    }
                    catch (...)
                    {
                        // Handle errors silently
                    }
                    
                    // Add a new component button
                    if (ImGui::Button("Add Component"))
                    {
                        try
                        {
                            document->update3mfModel();
                            
                            // Find a default object to reference, preferably a mesh
                            Lib3MF::PObject defaultObject;
                            auto innerResourceIterator = model3mf->GetResources();
                            while (innerResourceIterator->MoveNext())
                            {
                                auto resource = innerResourceIterator->GetCurrent();
                                auto meshObject = std::dynamic_pointer_cast<Lib3MF::CMeshObject>(resource);
                                if (meshObject && meshObject->GetType() == Lib3MF::eObjectType::Model)
                                {
                                    defaultObject = std::static_pointer_cast<Lib3MF::CObject>(meshObject);
                                    break;
                                }
                            }
                            
                            if (defaultObject)
                            {
                                // Create a new component with identity transform
                                Lib3MF::sTransform transform;
                                // Initialize with identity matrix
                                transform.m_Fields[0][0] = 1.0;
                                transform.m_Fields[1][1] = 1.0;
                                transform.m_Fields[2][2] = 1.0;
                                transform.m_Fields[3][3] = 1.0;
                                
                                componentsObject->AddComponent(defaultObject.get(), transform);
                                
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
                      // List all components
                    try
                    {
                        // Using GetComponentCount/GetComponent instead of an iterator
                        Lib3MF_uint32 componentCount = componentsObject->GetComponentCount();
                        for (Lib3MF_uint32 i = 0; i < componentCount; i++)
                        {
                            auto component = componentsObject->GetComponent(i);
                            
                            std::string componentName = getComponentName(component, static_cast<int>(i));
                            
                            ImGui::PushID(fmt::format("Component_{}", i).c_str());
                            
                            if (ImGui::TreeNodeEx(componentName.c_str(), baseFlags))
                            {
                                // Delete component button
                                if (ImGui::Button("Delete Component"))
                                {
                                    try
                                    {
                                        document->update3mfModel();
                                        // TODO: Find a way to remove individual components
                                        // Currently no direct way to remove a component
                                        // We'd need to recreate the components object without this component
                                        document->markFileAsChanged();
                                        document->updateDocumenFrom3mfModel();
                                        propertiesChanged = true;
                                    }
                                    catch (...)
                                    {
                                        // Handle errors silently
                                    }
                                }
                                  bool currentPropertiesChanged = renderComponentProperties(
                                    component, document, model3mf, static_cast<int>(i));
                                propertiesChanged = propertiesChanged || currentPropertiesChanged;
                                
                                ImGui::TreePop();
                            }
                            
                            ImGui::PopID();
                        }
                    }
                    catch (...)
                    {
                        // Handle errors silently
                    }
                    
                    ImGui::TreePop();
                }
                ImGui::EndGroup();
                frameOverlay(ImVec4(1.0f, 0.8f, 0.8f, 0.2f));
            }
        }
        catch (...)
        {
            // Handle any exceptions from iteration
        }

        return propertiesChanged;
    }

    bool ComponentsObjectView::renderObjectDropdown(
        SharedDocument document,
        Lib3MF::PModel model3mf,
        Lib3MF::PComponent component)
    {
        bool propertiesChanged = false;

        ImGui::PushID("ObjectDropdown");
        
        Lib3MF::PObject currentObject;
        try
        {
            currentObject = component->GetObjectResource();
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
                if (!object)
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
                        Lib3MF::sTransform transform = component->GetTransform();
                        
                        // Create a new component with the selected object and same transform
                        // (There's no direct way to change a component's object reference)
                        // This would need to remove the old component and add a new one
                        // which is complex without direct library support
                        
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

    bool ComponentsObjectView::renderTransformControls(
        SharedDocument document,
        Lib3MF::PModel model3mf,
        Lib3MF::PComponent component)
    {
        bool propertiesChanged = false;

        ImGui::PushID("TransformControls");
        
        try
        {
            Lib3MF::sTransform transform = component->GetTransform();
            
            // Create a 4x3 matrix UI for editing
            if (ImGui::BeginTable("TransformMatrix", 3, ImGuiTableFlags_Borders))
            {
                for (int i = 0; i < 4; i++)
                {
                    ImGui::TableNextRow();
                    for (int j = 0; j < 3; j++)
                    {
                        ImGui::TableNextColumn();
                        float value = transform.m_Fields[i][j];
                        ImGui::PushID(i * 4 + j);
                        if (ImGui::InputFloat("##", &value, 0.0f, 0.0f, "%.3f"))
                        {
                            document->update3mfModel();
                            transform.m_Fields[i][j] = value;
                            component->SetTransform(transform);
                            document->markFileAsChanged();
                            document->updateDocumenFrom3mfModel();
                            propertiesChanged = true;
                        }
                        ImGui::PopID();
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
                    
                    component->SetTransform(transform);
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
