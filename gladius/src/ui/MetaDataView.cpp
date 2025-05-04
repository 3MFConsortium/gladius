#include "MetaDataView.h"

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
                bool preserve = metaData->GetPreserve();

                // Name column
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name.c_str());

                // Value column
                ImGui::TableNextColumn();
                if (ImGui::InputText(("##Value_" + name).c_str(), &value, ImGuiInputTextFlags_None))
                {
                    try
                    {
                        document->update3mfModel();
                        metaData->SetValue(value);
                        document->markFileAsChanged();
                        modified = true;
                    }
                    catch (...)
                    {
                        // Handle errors silently
                    }
                }

                // Namespace column
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(namespace_.c_str());

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
                        metaData->SetPreserve(preserve);
                        document->markFileAsChanged();
                        modified = true;
                    }
                    catch (...)
                    {
                        // Handle errors silently
                    }
                }

                // Delete button column
                ImGui::TableNextColumn();
                if (ImGui::Button(("Delete##" + name).c_str()))
                {
                    try
                    {
                        document->update3mfModel();
                        Lib3MF::PMetaDataGroup metaDataGroup = model3mf->GetMetaDataGroup();
                        metaDataGroup->RemoveMetaData(namespace_, name);
                        document->markFileAsChanged();
                        modified = true;
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

    bool MetaDataView::renderMetaDataTable(
        SharedDocument document,
        Lib3MF::PModel model3mf)
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

            Lib3MF_uint32 count = metaDataGroup->GetMetaDataCount();
            if (count == 0)
            {
                ImGui::TextUnformatted("No metadata entries found");
                return false;
            }

            if (ImGui::BeginTable(
                "MetaDataTable", 6, 
                ImGuiTableFlags_Borders | 
                ImGuiTableFlags_RowBg | 
                ImGuiTableFlags_Resizable))
            {
                // Table headers
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Namespace", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Preserve", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableHeadersRow();

                // Table rows for each metadata entry
                for (Lib3MF_uint32 i = 0; i < count; i++)
                {
                    auto metaData = metaDataGroup->GetMetaData(i);
                    if (renderMetaDataEntry(metaData, document, model3mf))
                    {
                        modified = true;
                    }
                }

                ImGui::EndTable();
            }
        }
        catch (const std::exception& e)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 
                               "Error accessing metadata: %s", e.what());
        }

        return modified;
    }

    bool MetaDataView::renderAddMetaDataEntry(
        SharedDocument document,
        Lib3MF::PModel model3mf)
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
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##NewName", &newName);
            
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
            
            if (ImGui::Button("Add Metadata Entry"))
            {
                if (!newName.empty())
                {
                    try
                    {
                        document->update3mfModel();
                        Lib3MF::PMetaDataGroup metaDataGroup = model3mf->GetMetaDataGroup();
                        if (metaDataGroup)
                        {
                            metaDataGroup->AddMetaData(newNamespace, newName, newValue, newType, newPreserve);
                            document->markFileAsChanged();
                            modified = true;
                            
                            // Clear form after successful addition
                            newName.clear();
                            newValue.clear();
                            // Keep namespace, type and preserve as they are
                        }
                    }
                    catch (const std::exception& e)
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 
                                          "Error adding metadata: %s", e.what());
                    }
                }
            }
            
            ImGui::PopID();
        }
        
        ImGui::Unindent();
        
        return modified;
    }

} // namespace gladius::ui
