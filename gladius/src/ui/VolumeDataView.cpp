#include "VolumeDataView.h"

#include "Document.h"
#include "MeshResource.h"
#include "ResourceKey.h" // Include ResourceKey header
#include "ResourceManager.h"
#include "Widgets.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <algorithm> // For std::sort
#include <io/3mf/ResourceIdUtil.h>
#include <lib3mf_implicit.hpp>
#include <nodes/ModelUtils.h>
#include <vector> // For std::vector

namespace gladius::ui
{
    namespace
    {
        bool renderVolumeDataProperties(const Lib3MF::PVolumeData & volumeData,
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

                    // Channel name of color data
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Channel Name");
                    ImGui::TableNextColumn();

                    propertiesChanged |= VolumeDataView::renderChannelDropdown(
                      document, model3mf, volumeData, colorData);
                }
                else
                {
                    // First draw the button - we'll check for functions only when needed
                    bool buttonClicked = ImGui::Button("Add Color Function");

                    // Check if button is hovered to determine tooltip
                    if (ImGui::IsItemHovered())
                    {
                        // Only call areColorFunctionsAvailable when the button is hovered
                        bool functionsAvailable =
                          VolumeDataView::areColorFunctionsAvailable(document, model3mf);
                        if (!functionsAvailable)
                        {
                            ImGui::SetTooltip("No qualified color functions available");
                        }
                    }

                    if (buttonClicked)
                    {
                        try
                        {
                            // Use CreateNewColor to add a color function, passing a default
                            // function if available We'll pick the first qualified function
                            auto functions = document->getAssembly()->getFunctions();
                            for (const auto & [id, modelNode] : functions)
                            {
                                if (!modelNode || !nodes::isQualifiedForVolumeColor(*modelNode))
                                {
                                    continue;
                                }

                                try
                                {
                                    // Get the corresponding Lib3MF::PFunction
                                    auto resource =
                                      model3mf->GetResourceByID(io::resourceIdToUniqueResourceId(
                                        model3mf, modelNode->getResourceId()));
                                    auto func3mf =
                                      std::dynamic_pointer_cast<Lib3MF::CFunction>(resource);

                                    if (func3mf)
                                    {
                                        // Use CreateNewColor
                                        auto newColorData =
                                          volumeData->CreateNewColor(func3mf.get());
                                        propertiesChanged = true;
                                        break; // Successfully added a color function, so exit the
                                               // loop
                                    }
                                }
                                catch (...)
                                {
                                    // Skip to the next function if there was an error
                                    continue;
                                }
                            }

                            // If we got here without setting propertiesChanged, no suitable
                            // function was found
                            if (!propertiesChanged && document->getSharedLogger())
                            {
                                document->getSharedLogger()->addEvent(
                                  {"Failed to add color function: No suitable function found",
                                   events::Severity::Warning});
                            }
                        }
                        catch (const std::exception & e)
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

                ImGui::EndTable();
            }

            return propertiesChanged;
        }
    } // End anonymous namespace

    bool VolumeDataView::areColorFunctionsAvailable(SharedDocument const & document,
                                                    Lib3MF::PModel const & model3mf)
    {
        if (!document || !model3mf)
        {
            return false;
        }

        auto assembly = document->getAssembly();
        if (!assembly)
        {
            return false;
        }

        // Get all functions from the document's assembly
        auto functions = assembly->getFunctions();
        if (functions.empty())
        {
            return false;
        }

        // Check each function to see if it's qualified and has a corresponding 3MF resource
        for (const auto & [id, modelNode] : functions)
        {
            if (!modelNode)
            {
                continue;
            }

            // Skip if not qualified for volume color
            if (!nodes::isQualifiedForVolumeColor(*modelNode))
            {
                continue;
            }

            // Check if there's a corresponding resource in the 3MF model
            try
            {
                Lib3MF_uint32 uniqueResourceId =
                  io::resourceIdToUniqueResourceId(model3mf, modelNode->getResourceId());
                auto resource = model3mf->GetResourceByID(uniqueResourceId);
                auto functionResource = std::dynamic_pointer_cast<Lib3MF::CFunction>(resource);

                if (functionResource)
                {
                    // Found at least one qualified function with a corresponding resource
                    return true;
                }
            }
            catch (...)
            {
                // Function might not exist in the 3mf model yet or ID mismatch
                // Continue checking other functions
            }
        }

        // No qualified color functions available
        return false;
    }

    bool VolumeDataView::render(SharedDocument document) const
    {
        // ... (render function remains largely the same, using ResourceKey constructor and
        // RemoveResource) ...
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
                          {fmt::format("Added new VolumeData (ID: {})",
                                       newVolumeData->GetResourceID()),
                           events::Severity::Info});
                    }
                    propertiesChanged = true;
                }
            }
            catch (const std::exception & e)
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

            Lib3MF::PVolumeData volumeData =
              std::dynamic_pointer_cast<Lib3MF::CVolumeData>(resource);
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
                                  {fmt::format("Deleted VolumeData '{}'", name),
                                   events::Severity::Info});
                            }
                            // Need to break or handle iterator invalidation if removing the current
                            // item
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
                    catch (const std::exception & e)
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
            frameOverlay(ImVec4(1.0f, 0.0f, 1.0f, 0.2f),
                        "Volume Data Properties\n\n"
                        "Volume data defines properties like color or density that change\n"
                        "throughout your model. This allows for gradients, varying materials,\n"
                        "and other effects that aren't possible with simple surface models.");
        }

        return propertiesChanged;
    }

    std::string VolumeDataView::getVolumeDataName(const Lib3MF::PVolumeData & volumeData)
    {
        if (!volumeData)
        {
            return {};
        }
        // build a name based on the resource ID
        return fmt::format("VolumeData #{}", volumeData->GetResourceID());
    }

    bool VolumeDataView::renderColorFunctionDropdown(
      SharedDocument document,
      Lib3MF::PModel model3mf,
      Lib3MF::PVolumeData volumeData,
      Lib3MF::PVolumeDataColor colorData) // Pass PVolumeDataColor
    {
        bool propertiesChanged = false;

        ImGui::PushID("ColorFunctionDropdown");

        std::string functionDisplayName = "Please select";
        Lib3MF_uint32 currentFunctionId = 0;
        try
        {
            // Use GetFunctionResourceID on colorData
            currentFunctionId = colorData->GetFunctionResourceID();
            if (currentFunctionId != 0)
            {
                // Use GetResourceByID on model3mf
                auto resource = model3mf->GetResourceByID(currentFunctionId);
                auto currentFunction = std::dynamic_pointer_cast<Lib3MF::CFunction>(resource);
                if (currentFunction)
                {
                    functionDisplayName = currentFunction->GetDisplayName();
                }
                else
                {
                    functionDisplayName = fmt::format("Invalid Function ID: {}", currentFunctionId);
                }
            }
        }
        catch (...)
        {
            // Handle error getting function ID or function
            functionDisplayName = "Error reading function";
        }

        if (ImGui::BeginCombo("##ColorFunctionCombo",
                              functionDisplayName.c_str())) // Added unique label
        {
            auto assembly = document->getAssembly();
            if (assembly)
            {
                for (const auto & [id, modelNode] : assembly->getFunctions())
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
                    Lib3MF_uint32 uniqueResourceId =
                      io::resourceIdToUniqueResourceId(model3mf, modelNode->getResourceId());
                    try
                    {
                        // Use GetResourceByID
                        auto resource = model3mf->GetResourceByID(uniqueResourceId);
                        functionResource = std::dynamic_pointer_cast<Lib3MF::CFunction>(resource);
                    }
                    catch (...)
                    {
                        // Function might not exist in the 3mf model yet or ID mismatch
                        continue;
                    }

                    if (!functionResource)
                    {
                        continue;
                    }

                    // Check against the unique resource ID
                    bool isSelected =
                      (currentFunctionId != 0) &&
                      (functionResource->GetUniqueResourceID() == currentFunctionId);

                    std::string itemName = fmt::format(
                      "{} (Function #{})",
                      functionResource->GetDisplayName(),
                      functionResource
                        ->GetResourceID()); // Use ModelResourceID for display consistency?

                    if (ImGui::Selectable(itemName.c_str(), isSelected))
                    {
                        try
                        {
                            // Set the function ID on the color data using SetFunctionResourceID
                            colorData->SetFunctionResourceID(
                              functionResource->GetUniqueResourceID());
                            propertiesChanged = true;
                        }
                        catch (const std::exception & e)
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
            if (ImGui::Selectable("[Remove Color Function]", false))
            {
                try
                {
                    volumeData->RemoveColor();
                    propertiesChanged = true;
                }
                catch (const std::exception & e)
                {
                    if (document->getSharedLogger())
                    {
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

    bool VolumeDataView::renderPropertyFunctionsSection(
      SharedDocument document,
      Lib3MF::PModel model3mf,
      Lib3MF::PVolumeData volumeData,
      Lib3MF::PVolumeDataProperty propertyData) // Pass PVolumeDataProperty
    {
        bool propertiesChanged = false;

        std::string propName = "[Unnamed Property]";
        Lib3MF_uint32 functionId = 0;
        std::string functionName = "[Unknown Function]";

        try
        {
            // Use GetName on propertyData
            propName = propertyData->GetName();
            // Use GetFunctionResourceID on propertyData
            functionId = propertyData->GetFunctionResourceID();
            if (functionId != 0)
            {
                // Use GetResourceByID on model3mf
                auto resource = model3mf->GetResourceByID(functionId);
                auto func = std::dynamic_pointer_cast<Lib3MF::CFunction>(resource);
                if (func)
                {
                    functionName = func->GetDisplayName();
                }
                else
                {
                    functionName = fmt::format("[Invalid Function ID: {}]", functionId);
                }
            }
        }
        catch (...)
        {
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

    bool VolumeDataView::renderChannelDropdown(SharedDocument document,
                                               Lib3MF::PModel model3mf,
                                               Lib3MF::PVolumeData volumeData,
                                               Lib3MF::PVolumeDataColor colorData)
    {
        bool propertiesChanged = false;

        // Get the current function from the colorData
        auto functionId = colorData->GetFunctionResourceID();
        if (functionId == 0)
        {
            return propertiesChanged;
        }

        // Get the function resource
        Lib3MF::PFunction function;
        try
        {
            auto resource = model3mf->GetResourceByID(functionId);
            function = std::dynamic_pointer_cast<Lib3MF::CFunction>(resource);
        }
        catch (...)
        {
            // Handle errors silently
            return propertiesChanged;
        }

        if (!function)
        {
            return propertiesChanged;
        }

        // Find the corresponding function model
        auto functionModel = document->getAssembly()->findModel(
          io::uniqueResourceIdToResourceId(model3mf, function->GetResourceID()));
        if (!functionModel)
        {
            return propertiesChanged;
        }

        // Get the outputs from the function model's end node
        auto & endNodeParameters = functionModel->getOutputs();

        ImGui::PushID("ColorChannelDropdown");
        if (ImGui::BeginCombo("##ColorChannelCombo", colorData->GetChannelName().c_str()))
        {
            for (const auto & [paramName, param] : endNodeParameters)
            {
                // Filter for float3 type outputs only
                if (param.getTypeIndex() != nodes::ParameterTypeIndex::Float3)
                {
                    continue;
                }

                bool isSelected = (paramName == colorData->GetChannelName());
                if (ImGui::Selectable(paramName.c_str(), isSelected))
                {
                    try
                    {
                        colorData->SetChannelName(paramName);
                        propertiesChanged = true;
                    }
                    catch (const std::exception & e)
                    {
                        if (document->getSharedLogger())
                        {
                            document->getSharedLogger()->addEvent(
                              {fmt::format("Failed to set channel name: {}", e.what()),
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
} // namespace gladius::ui