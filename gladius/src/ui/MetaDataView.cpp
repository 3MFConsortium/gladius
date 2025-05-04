#include "MetaDataView.h"

#include "Document.h"
#include "Widgets.h"
#include "imgui.h"
#include <imgui_stdlib.h>
#include <io/3mf/ResourceIdUtil.h>
#include <nodes/ModelUtils.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace gladius::ui
{
    namespace
    {
        // List of well-known metadata names as per 3MF specification
        const std::array<std::string, 9> g_wellKnownMetadataNames = {"Title",
                                                                     "Designer",
                                                                     "Description",
                                                                     "Copyright",
                                                                     "LicenseTerms",
                                                                     "Rating",
                                                                     "CreationDate",
                                                                     "ModificationDate",
                                                                     "Application"};

        // Checks if a metadata name is in the list of well-known names
        bool isWellKnownMetadataName(const std::string & name)
        {
            return std::find(g_wellKnownMetadataNames.begin(),
                             g_wellKnownMetadataNames.end(),
                             name) != g_wellKnownMetadataNames.end();
        }

        // Checks if a metadata with the given name and namespace already exists
        bool metadataExists(Lib3MF::PMetaDataGroup metaDataGroup,
                            const std::string & namespace_,
                            const std::string & name)
        {
            try
            {
                // If this doesn't throw an exception, the metadata exists
                metaDataGroup->GetMetaDataByKey(namespace_, name);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        // Format a date string according to ISO 8601 (YYYY-MM-DDThh:mm:ss)
        std::string getCurrentDateTimeIso8601()
        {
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);

            std::stringstream ss;

#ifdef _WIN32
            std::tm local_tm;
            localtime_s(&local_tm, &time_t_now);
            ss << std::put_time(&local_tm, "%Y-%m-%dT%H:%M:%S");
#else
            std::tm * local_tm = localtime(&time_t_now);
            ss << std::put_time(local_tm, "%Y-%m-%dT%H:%M:%S");
#endif

            return ss.str();
        }

        // Renders a metadata entry row in the table
        bool renderMetaDataEntry(Lib3MF::PMetaData metaData,
                                 SharedDocument document,
                                 Lib3MF::PModel model3mf)
        {
            bool modified = false;
            try
            {
                // Get metadata properties
                std::string namespace_ = metaData->GetNameSpace();
                std::string name = metaData->GetName();
                std::string value = metaData->GetValue();
                std::string type = metaData->GetType();
                bool preserve = metaData->GetMustPreserve();

                bool isWellKnown = isWellKnownMetadataName(name) && namespace_.empty();
                ImVec4 nameColor = isWellKnown ? ImVec4(0.0f, 0.7f, 0.7f, 1.0f)
                                               :                     // Teal for well-known names
                                     ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White for custom names

                // Name column with appropriate color for well-known names
                ImGui::TableNextColumn();
                ImGui::TextColored(nameColor, "%s", name.c_str());

                // If this is a well-known metadata name, show tooltip with info
                if (ImGui::IsItemHovered() && isWellKnown)
                {
                    std::string tooltip;
                    if (name == "Title")
                        tooltip = "A title for the 3MF document";
                    else if (name == "Designer")
                        tooltip = "A name for a designer of this document";
                    else if (name == "Description")
                        tooltip = "A description of the document";
                    else if (name == "Copyright")
                        tooltip = "A copyright associated with this document";
                    else if (name == "LicenseTerms")
                        tooltip = "License information associated with this document";
                    else if (name == "Rating")
                        tooltip = "An industry rating associated with this document";
                    else if (name == "CreationDate")
                        tooltip = "The date this documented was created by a source app";
                    else if (name == "ModificationDate")
                        tooltip = "The date this document was last modified";
                    else if (name == "Application")
                        tooltip = "The name of the source application that created this document";

                    if (!tooltip.empty())
                    {
                        ImGui::SetTooltip("%s", tooltip.c_str());
                    }
                }

                // Value column with appropriate input based on type
                ImGui::TableNextColumn();
                bool valueChanged = false;
                ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_None;

                // Special handling for different types
                if (type == "boolean")
                {
                    bool boolValue = (value == "true" || value == "1");
                    if (ImGui::Checkbox(("##Value_" + name).c_str(), &boolValue))
                    {
                        value = boolValue ? "true" : "false";
                        valueChanged = true;
                    }
                }
                else if (type == "integer")
                {
                    int intValue = 0;
                    try
                    {
                        intValue = std::stoi(value);
                    }
                    catch (...)
                    {
                    }

                    if (ImGui::InputInt(("##Value_" + name).c_str(), &intValue, 0, 0))
                    {
                        value = std::to_string(intValue);
                        valueChanged = true;
                    }
                }
                else if (type == "float")
                {
                    float floatValue = 0.0f;
                    try
                    {
                        floatValue = std::stof(value);
                    }
                    catch (...)
                    {
                    }

                    if (ImGui::InputFloat(
                          ("##Value_" + name).c_str(), &floatValue, 0.0f, 0.0f, "%.6f"))
                    {
                        std::stringstream ss;
                        ss << floatValue;
                        value = ss.str();
                        valueChanged = true;
                    }
                }
                else if (type == "dateTime" || name == "CreationDate" || name == "ModificationDate")
                {
                    // Special case for date fields
                    if (ImGui::InputText(("##Value_" + name).c_str(), &value, inputFlags))
                    {
                        valueChanged = true;
                    }

                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Date format: YYYY-MM-DDThh:mm:ss");
                    }

                    // Button to set current date/time
                    ImGui::SameLine();
                    if (ImGui::Button(("Now##" + name).c_str()))
                    {
                        value = getCurrentDateTimeIso8601();
                        valueChanged = true;
                    }
                }
                else
                {
                    // Default text field
                    if (ImGui::InputText(("##Value_" + name).c_str(), &value, inputFlags))
                    {
                        valueChanged = true;
                    }
                }

                // Update value if changed
                if (valueChanged)
                {
                    try
                    {
                        document->update3mfModel();
                        metaData->SetValue(value);
                        document->markFileAsChanged();
                        modified = true;
                    }
                    catch (const std::exception & e)
                    {
                        ImGui::SetTooltip("Error: %s", e.what());
                    }
                }

                // Namespace column
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(namespace_.c_str());
                if (namespace_.empty() && isWellKnownMetadataName(name))
                {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(well-known)");
                }

                // Type column
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(type.c_str());

                // Preserve column
                ImGui::TableNextColumn();
                if (ImGui::Checkbox(("##Preserve_" + name).c_str(), &preserve))
                {
                    try
                    {
                        document->update3mfModel();
                        metaData->SetMustPreserve(preserve);
                        document->markFileAsChanged();
                        modified = true;
                    }
                    catch (const std::exception & e)
                    {
                        ImGui::SetTooltip("Error: %s", e.what());
                    }
                }

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(
                      "When true, consumers that modify the 3MF file should retain\n"
                      "the original metadata value even if the data it references is modified.");
                }

                // Delete button column
                ImGui::TableNextColumn();
                if (ImGui::Button(("Delete##" + name).c_str()))
                {
                    try
                    {
                        document->update3mfModel();
                        Lib3MF::PMetaDataGroup metaDataGroup = model3mf->GetMetaDataGroup();
                        // First get the metadata by key, then remove it
                        auto metaDataToRemove = metaDataGroup->GetMetaDataByKey(namespace_, name);
                        if (metaDataToRemove)
                        {
                            metaDataGroup->RemoveMetaData(metaDataToRemove);
                            document->markFileAsChanged();
                            modified = true;
                        }
                    }
                    catch (const std::exception & e)
                    {
                        ImGui::SetTooltip("Error: %s", e.what());
                    }
                }
            }
            catch (const std::exception & e)
            {
                ImGui::TextColored(
                  ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error displaying metadata: %s", e.what());
            }

            return modified;
        }
    }

    bool MetaDataView::render(SharedDocument document) const
    {
        if (!document)
        {
            return false;
        }

        bool modified = false;

        // Get the 3MF model from the document
        Lib3MF::PModel model3mf = document->get3mfModel();
        if (!model3mf)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "No 3MF model loaded");
            return false;
        }

        // Render the metadata table
        modified |= renderMetaDataTable(document, model3mf);

        // Render interface to add new metadata
        modified |= renderAddMetaDataEntry(document, model3mf);

        return modified;
    }

    bool MetaDataView::renderMetaDataTable(SharedDocument document, Lib3MF::PModel model3mf)
    {
        bool modified = false;

        try
        {
            Lib3MF::PMetaDataGroup metaDataGroup = model3mf->GetMetaDataGroup();
            if (!metaDataGroup)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "No metadata group found");
                return false;
            }

            // Get the count of metadata entries
            Lib3MF_uint32 count = metaDataGroup->GetMetaDataCount();

            // Collect metadata entries to sort them
            struct MetadataEntry
            {
                Lib3MF::PMetaData data;
                bool isWellKnown;
                std::string name;
            };

            std::vector<MetadataEntry> entries;
            entries.reserve(count);

            for (Lib3MF_uint32 i = 0; i < count; i++)
            {
                auto metaData = metaDataGroup->GetMetaData(i);
                bool isWellKnown =
                  isWellKnownMetadataName(metaData->GetName()) && metaData->GetNameSpace().empty();
                entries.push_back({metaData, isWellKnown, metaData->GetName()});
            }

            // Sort: first well-known entries (alphabetically), then custom entries (alphabetically)
            std::sort(entries.begin(),
                      entries.end(),
                      [](const MetadataEntry & a, const MetadataEntry & b)
                      {
                          if (a.isWellKnown != b.isWellKnown)
                              return a.isWellKnown > b.isWellKnown;
                          return a.name < b.name;
                      });

            if (ImGui::BeginTable(
                  "MetaDataTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                // Table headers
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Namespace", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Preserve", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();

                bool lastWasWellKnown = true;
                bool firstCustomShown = false;

                // Table rows for each metadata entry
                for (const auto & entry : entries)
                {
                    // Add a separator between well-known and custom entries
                    if (lastWasWellKnown && !entry.isWellKnown && !firstCustomShown)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Separator();
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Custom Metadata");
                        ImGui::TableNextColumn();
                        ImGui::Separator();
                        ImGui::TableNextColumn();
                        ImGui::Separator();
                        ImGui::TableNextColumn();
                        ImGui::Separator();
                        ImGui::TableNextColumn();
                        ImGui::Separator();
                        ImGui::TableNextColumn();
                        ImGui::Separator();

                        firstCustomShown = true;
                    }

                    lastWasWellKnown = entry.isWellKnown;

                    ImGui::TableNextRow();
                    if (renderMetaDataEntry(entry.data, document, model3mf))
                    {
                        modified = true;
                    }
                }

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }
        catch (const std::exception & e)
        {
            ImGui::TextColored(
              ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error accessing metadata: %s", e.what());
        }

        return modified;
    }

    bool MetaDataView::renderAddMetaDataEntry(SharedDocument document, Lib3MF::PModel model3mf)
    {
        static std::string newName;
        static std::string newValue;
        static std::string newNamespace;
        static std::string newType = "string";
        static bool newPreserve = true;

        bool modified = false;

        ImGui::Indent();

        if (ImGui::CollapsingHeader("Add New Metadata Entry", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID("AddMetaDataForm");

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Name:");
            ImGui::SameLine();

            // ComboBox for well-known metadata names
            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::BeginCombo("##WellKnownNames", "Select Well-Known..."))
            {
                for (const auto & wellKnownName : g_wellKnownMetadataNames)
                {
                    bool isSelected = (newName == wellKnownName);
                    if (ImGui::Selectable(wellKnownName.c_str(), isSelected))
                    {
                        newName = wellKnownName;
                        // Clear namespace for well-known names as they use the default namespace
                        newNamespace.clear();
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##NewName", &newName);

            // Show a hint about namespace requirement
            bool isCustomName = !newName.empty() && !isWellKnownMetadataName(newName);
            if (isCustomName && newNamespace.empty())
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f),
                                   "(Namespace required for custom names)");
            }

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Value:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##NewValue", &newValue);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Namespace:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##NewNamespace", &newNamespace);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Type:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##NewType", newType.c_str()))
            {
                if (ImGui::Selectable("string", newType == "string"))
                    newType = "string";
                if (ImGui::Selectable("boolean", newType == "boolean"))
                    newType = "boolean";
                if (ImGui::Selectable("integer", newType == "integer"))
                    newType = "integer";
                if (ImGui::Selectable("float", newType == "float"))
                    newType = "float";
                ImGui::EndCombo();
            }

            ImGui::Checkbox("Preserve", &newPreserve);

            ImGui::Separator();

            // Use the previously defined isCustomName variable
            bool namespaceRequired = isCustomName && newNamespace.empty();

            if (ImGui::Button("Add Metadata Entry"))
            {
                if (!newName.empty())
                {
                    if (namespaceRequired)
                    {
                        // Display an error that namespace is required for custom names
                        ImGui::TextColored(
                          ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                          "Error: Namespace is required for custom metadata names.");
                    }
                    else
                    {
                        try
                        {
                            document->update3mfModel();
                            Lib3MF::PMetaDataGroup metaDataGroup = model3mf->GetMetaDataGroup();
                            if (metaDataGroup)
                            {
                                // Check if metadata with same name and namespace already exists
                                if (metadataExists(metaDataGroup, newNamespace, newName))
                                {
                                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                                                       "Error: Metadata with this name and "
                                                       "namespace already exists.");
                                }
                                else
                                {
                                    metaDataGroup->AddMetaData(
                                      newNamespace, newName, newValue, newType, newPreserve);
                                    document->markFileAsChanged();
                                    modified = true;

                                    // Clear form after successful addition
                                    newName.clear();
                                    newValue.clear();
                                    // Keep namespace, type and preserve as they are
                                }
                            }
                        }
                        catch (const std::exception & e)
                        {
                            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                                               "Error adding metadata: %s",
                                               e.what());
                        }
                    }
                }
            }

            ImGui::PopID();
        }

        ImGui::Unindent();

        return modified;
    }

} // namespace gladius::ui
