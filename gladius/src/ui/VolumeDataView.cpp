#include "VolumeDataView.h"

#include "Document.h"
#include "MeshResource.h"
#include "ResourceManager.h"
#include "Widgets.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <io/3mf/ResourceIdUtil.h>
#include <lib3mf_implicit.hpp>
#include <nodes/ModelUtils.h>
#include "ResourceKey.h" // Include ResourceKey header
#include <algorithm> // For std::sort
#include <vector> // For std::vector

namespace gladius::ui
{
    namespace
    {
        bool renderVolumeDataProperties(const Lib3MF::PVolumeData& volumeData,
                                      SharedDocument document,
                                      Lib3MF::PModel model3mf)
        {
            bool propertiesChanged = false;

            if (ImGui::BeginTable(
                  "VolumeDataProperties", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                // Color Function section
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Color Function");
                ImGui::TableNextColumn();
                
                Lib3MF::PVolumeDataColor colorData;
                try
                {
                    colorData = volumeData->GetColor();
                }
                catch (...)
                {
                    // Handle errors silently - GetColor throws if no color exists
                }
                
                if (colorData)
                {
                    propertiesChanged |= VolumeDataView::renderColorFunctionDropdown(
                        document, model3mf, volumeData, colorData);
                }
                else
                {
                    if (ImGui::Button("Add Color Function"))
                    {
                        try
                        {
                            // Use CreateNewColor to add a color function, passing a default function if available
                            // We'll pick the first function if any
                            auto functions = document->getAssembly()->getFunctions();
                            if (!functions.empty()) {
                                auto funcNode = std::begin(functions)->second;
                                // Get the corresponding Lib3MF::PFunction
                                auto resource = model3mf->GetResourceByID(
                                    io::resourceIdToUniqueResourceId(model3mf, funcNode->getResourceId()));
                                auto func3mf = std::dynamic_pointer_cast<Lib3MF::CFunction>(resource);
                                if (func3mf) {
                                    // Use CreateNewColor
                                    auto newColorData = volumeData->CreateNewColor(func3mf.get()); 
                                    propertiesChanged = true;
                                } else {
                                     if (document->getSharedLogger()) {
                                        document->getSharedLogger()->addEvent(
                                            {fmt::format("Failed to find corresponding lib3mf function for Gladius function ID {}", funcNode->getResourceId()), 
                                            events::Severity::Warning});
                                    }
                                }
                            } else {
                                if (document->getSharedLogger()) {
                                    document->getSharedLogger()->addEvent(
                                        {"No functions available to assign to new color function", 
                                        events::Severity::Warning});
                                }
                            }
                        }
                        catch (const std::exception& e)
                        {
                            if (document->getSharedLogger())
                            {
                                document->getSharedLogger()->addEvent(
                                    {fmt::format("Failed to add color function: {}", e.what()), 
                                    events::Severity::Error});
                            }
                        }
                    }
                }

            
                
                // Property Functions section
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Property Functions");
                ImGui::TableNextColumn();
                
                // Use GetPropertyCount on volumeData
                auto propertyCount = volumeData->GetPropertyCount(); 
                if (propertyCount > 0)
                {
                    // Keep track of indices to remove later if needed
                    std::vector<Lib3MF_uint32> indicesToRemove;
                    for (Lib3MF_uint32 i = 0; i < propertyCount; ++i) {
                        ImGui::PushID(static_cast<int>(i)); // Ensure unique IDs for widgets within the loop
                        try {
                            // Use GetProperty(i) on volumeData
                            auto prop = volumeData->GetProperty(i); 
                            propertiesChanged |= VolumeDataView::renderPropertyFunctionsSection(
                                document, model3mf, volumeData, prop);
                            
                            // Add a remove button for each property
                            ImGui::SameLine();
                            if (ImGui::Button("Remove")) {
                                indicesToRemove.push_back(i);
                            }
                        } catch (const std::exception& e) {
                             if (document->getSharedLogger()) {
                                document->getSharedLogger()->addEvent(
                                    {fmt::format("Error rendering property at index {}: {}", i, e.what()), events::Severity::Error});
                            }
                            // Optionally display an error message in the UI
                            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error rendering property");
                        }
                        ImGui::PopID();
                    }

                    // Remove properties marked for deletion (iterate backwards to handle index shifts)
                    if (!indicesToRemove.empty()) {
                        std::sort(indicesToRemove.rbegin(), indicesToRemove.rend());
                        for (auto index : indicesToRemove) {
                            try {
                                volumeData->RemoveProperty(index);
                                propertiesChanged = true;
                            } catch (const std::exception& e) {
                                if (document->getSharedLogger()) {
                                    document->getSharedLogger()->addEvent(
                                        {fmt::format("Failed to remove property function at index {}: {}", index, e.what()), events::Severity::Error});
                                }
                            }
                        }
                    }
                }
                else
                {
                    if (ImGui::Button("Add Property Function"))
                    {
                        try
                        {
                            // Pick first available function if exists
                            auto functions = document->getAssembly()->getFunctions();
                            if (!functions.empty()) {
                                auto funcNode = std::begin(functions)->second;
                                // Get the corresponding Lib3MF::PFunction
                                auto resource = model3mf->GetResourceByID(
                                    io::resourceIdToUniqueResourceId(model3mf, funcNode->getResourceId()));
                                auto func3mf = std::dynamic_pointer_cast<Lib3MF::CFunction>(resource);
                                if (func3mf) {
                                    // TODO: Allow user to specify name
                                    auto newProperty = volumeData->AddPropertyFromFunction("NewProperty", func3mf.get()); 
                                    propertiesChanged = true;
                                } else {
                                     if (document->getSharedLogger()) {
                                        document->getSharedLogger()->addEvent(
                                            {fmt::format("Failed to find corresponding lib3mf function for Gladius function ID {}", funcNode->getResourceId()), 
                                            events::Severity::Warning});
                                    }
                                }
                            } else {
                                if (document->getSharedLogger()) {
                                    document->getSharedLogger()->addEvent(
                                        {"No functions available to assign to new property function", 
                                        events::Severity::Warning});
                                }
                            }
                        }
                        catch (const std::exception& e)
                        {
                            if (document->getSharedLogger())
                            {
                                document->getSharedLogger()->addEvent(
                                    {fmt::format("Failed to add property function: {}", e.what()), 
                                    events::Severity::Error});
                            }
                        }
                    }
                }
                
                ImGui::EndTable();
            }

            return propertiesChanged;
        }
    } // End anonymous namespace

    // ... existing render function ...
    bool VolumeDataView::render(SharedDocument document) const
    {
        // ... (render function remains largely the same, using ResourceKey constructor and RemoveResource) ...
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
        // Add "Add VolumeData" button
        if (ImGui::Button("Add VolumeData"))
        {
            try
            {
                auto newVolumeData = model3mf->AddVolumeData();
                if (newVolumeData)
                {
                    // Initialize default properties if needed
                    // Metadata needs to be added via the model, not directly on the resource
                    // Example: model3mf->GetMetaDataGroup()->AddMetaData(...) 
                    //          referencing newVolumeData->GetResourceID() if needed.
                    
                    if (document->getSharedLogger())
                    {
                        document->getSharedLogger()->addEvent(
                            {fmt::format("Added new VolumeData (ID: {})", newVolumeData->GetResourceID()), 
                            events::Severity::Info});
                    }
                    propertiesChanged = true;
                }
            }
            catch (const std::exception& e)
            {
                if (document->getSharedLogger())
                {
                    document->getSharedLogger()->addEvent(
                        {fmt::format("Failed to add VolumeData: {}", e.what()), 
                        events::Severity::Error});
                }
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

            Lib3MF::PVolumeData volumeData = std::dynamic_pointer_cast<Lib3MF::CVolumeData>(resource);
            if (!volumeData)
            {
                continue;
            }

            auto name = getVolumeDataName(volumeData);

            ImGui::BeginGroup();
            if (ImGui::TreeNodeEx(name.c_str(), baseFlags))
            {
                propertiesChanged |= renderVolumeDataProperties(volumeData, document, model3mf);
                
                // Delete button
                if (ImGui::Button("Delete"))
                {
                    try
                    {
                        // Check if this resource can be safely deleted
                        // Use ResourceKey constructor directly
                        auto resourceKey = ResourceKey(volumeData->GetResourceID()); 
                        auto safeResult = document->isItSafeToDeleteResource(resourceKey);
                        
                        if (safeResult.canBeRemoved)
                        {
                            model3mf->RemoveResource(volumeData.get());
                            propertiesChanged = true;
                            
                            if (document->getSharedLogger())
                            {
                                document->getSharedLogger()->addEvent(
                                    {fmt::format("Deleted VolumeData '{}'", name), events::Severity::Info});
                            }
                            // Need to break or handle iterator invalidation if removing the current item
                            ImGui::TreePop(); 
                            ImGui::EndGroup();
                            break; // Exit loop after removal to avoid using invalidated iterator
                        }
                        else
                        {
                            if (document->getSharedLogger())
                            {
                                document->getSharedLogger()->addEvent(
                                    {fmt::format("Cannot delete VolumeData: it is still in use."), 
                                    events::Severity::Warning});
                            }
                        }
                    }
                    catch (const std::exception& e)
                    {
                        if (document->getSharedLogger())
                        {
                            document->getSharedLogger()->addEvent(
                                {fmt::format("Failed to delete VolumeData: {}", e.what()), 
                                events::Severity::Error});
                        }
                    }
                }
                
                ImGui::TreePop();
            }
            ImGui::EndGroup();
            frameOverlay(ImVec4(1.0f, 0.0f, 1.0f, 0.2f));
        }

        return propertiesChanged;
    }


    std::string VolumeDataView::getVolumeDataName(const Lib3MF::PVolumeData& volumeData)
    {
        if (!volumeData)
        {
            return {};
        }
        // build a name based on the resource ID
        return fmt::format("VolumeData #{}", volumeData->GetResourceID());
    }

    bool VolumeDataView::renderColorFunctionDropdown(SharedDocument document,
                                              Lib3MF::PModel model3mf,
                                              Lib3MF::PVolumeData volumeData,
                                              Lib3MF::PVolumeDataColor colorData) // Pass PVolumeDataColor
    {
        bool propertiesChanged = false;

        ImGui::PushID("ColorFunctionDropdown");
        
        std::string functionDisplayName = "Please select";
        Lib3MF_uint32 currentFunctionId = 0;
        try {
            // Use GetFunctionResourceID on colorData
            currentFunctionId = colorData->GetFunctionResourceID(); 
            if (currentFunctionId != 0) {
                // Use GetResourceByID on model3mf
                auto resource = model3mf->GetResourceByID(currentFunctionId); 
                auto currentFunction = std::dynamic_pointer_cast<Lib3MF::CFunction>(resource);
                if (currentFunction) {
                    functionDisplayName = currentFunction->GetDisplayName();
                } else {
                    functionDisplayName = fmt::format("Invalid Function ID: {}", currentFunctionId);
                }
            }
        } catch (...) {
            // Handle error getting function ID or function
             functionDisplayName = "Error reading function";
        }

        
        if (ImGui::BeginCombo("##ColorFunctionCombo", functionDisplayName.c_str())) // Added unique label
        {
            auto assembly = document->getAssembly();
            if (assembly)
            {
                for (const auto& [id, modelNode] : assembly->getFunctions())
                {
                    if (!modelNode)
                    {
                        continue;
                    }

                    // Filter functions using isQualifiedForVolumeColor
                    if (!nodes::isQualifiedForVolumeColor(*modelNode))
                    {
                        continue;
                    }

                    // Get the corresponding Lib3MF function resource
                    Lib3MF::PFunction functionResource;
                    Lib3MF_uint32 uniqueResourceId = io::resourceIdToUniqueResourceId(model3mf, modelNode->getResourceId());
                    try {
                         // Use GetResourceByID
                         auto resource = model3mf->GetResourceByID(uniqueResourceId); 
                         functionResource = std::dynamic_pointer_cast<Lib3MF::CFunction>(resource);
                    } catch (...) {
                        // Function might not exist in the 3mf model yet or ID mismatch
                        continue; 
                    }

                    if (!functionResource)
                    {
                        continue;
                    }

                    // Check against the unique resource ID
                    bool isSelected = (currentFunctionId != 0) && (functionResource->GetUniqueResourceID() == currentFunctionId); 

                    std::string itemName = fmt::format("{} (Function #{})",
                                                      functionResource->GetDisplayName(),
                                                      functionResource->GetResourceID()); // Use ModelResourceID for display consistency?

                    if (ImGui::Selectable(itemName.c_str(), isSelected))
                    {
                        try
                        {
                            // Set the function ID on the color data using SetFunctionResourceID
                            colorData->SetFunctionResourceID(functionResource->GetUniqueResourceID()); 
                            propertiesChanged = true;
                        }
                        catch (const std::exception& e)
                        {
                            if (document->getSharedLogger())
                            {
                                document->getSharedLogger()->addEvent(
                                    {fmt::format("Failed to set color function: {}", e.what()), 
                                    events::Severity::Error});
                            }
                        }
                    }
                }
            }

            // Option to remove/unset the function
            if (ImGui::Selectable("[Remove Color Function]", false)) {
                 try {
                    volumeData->RemoveColor();
                    propertiesChanged = true;
                 } catch (const std::exception& e) {
                     if (document->getSharedLogger()) {
                         document->getSharedLogger()->addEvent(
                             {fmt::format("Failed to remove color function: {}", e.what()), 
                             events::Severity::Error});
                     }
                 }
            }

            ImGui::EndCombo();
        }
        ImGui::PopID();

        return propertiesChanged;
    }


    bool VolumeDataView::renderPropertyFunctionsSection(SharedDocument document,
                                          Lib3MF::PModel model3mf,
                                          Lib3MF::PVolumeData volumeData,
                                          Lib3MF::PVolumeDataProperty propertyData) // Pass PVolumeDataProperty
    {
        bool propertiesChanged = false;

        std::string propName = "[Unnamed Property]";
        Lib3MF_uint32 functionId = 0;
        std::string functionName = "[Unknown Function]";

        try {
            // Use GetName on propertyData
            propName = propertyData->GetName(); 
            // Use GetFunctionResourceID on propertyData
            functionId = propertyData->GetFunctionResourceID(); 
             if (functionId != 0) {
                // Use GetResourceByID on model3mf
                auto resource = model3mf->GetResourceByID(functionId); 
                auto func = std::dynamic_pointer_cast<Lib3MF::CFunction>(resource);
                if (func) {
                    functionName = func->GetDisplayName();
                } else {
                     functionName = fmt::format("[Invalid Function ID: {}]", functionId);
                }
            }
        } catch (...) {
             // Error reading property details
             propName = "[Error Reading Property]";
        }

        ImGui::Text("%s: %s", propName.c_str(), functionName.c_str());

        // TODO: Add controls to edit property name and select function similar to color dropdown

        // Example: Edit Name
        // std::string currentName = propertyData->GetName();
        // if (ImGui::InputText("##PropName", &currentName)) {
        //     try {
        //         // Need a SetName method on CVolumeDataProperty - check if it exists
        //         // propertyData->SetName(currentName); 
        //         propertiesChanged = true;
        //     } catch (...) { ... }
        // }

        // Example: Edit Function (similar logic to renderColorFunctionDropdown)
        // if (ImGui::BeginCombo("##PropFunc", functionName.c_str())) { ... }


        return propertiesChanged;
    }
} // namespace gladius::ui