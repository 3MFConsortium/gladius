#include "ResourceView.h"
#include "io/3mf/ResourceDependencyGraph.h"
#include "io/3mf/ResourceIdUtil.h"

#include "FileChooser.h"
#include "ImageStackResource.h"
#include "MeshResource.h"
#include "ResourceManager.h"
#include "Widgets.h"

#include "imgui.h"
#include <imgui_stdlib.h>      // For InputText with std::string
#include <lib3mf_implicit.hpp> // For VolumeData interfaces

namespace gladius::ui
{

    // Implementation of renderVolumeDataDropdown to show available VolumeData resources for a mesh
    bool ResourceView::renderVolumeDataDropdown(SharedDocument document,
                                                Lib3MF::PModel model3mf,
                                                std::shared_ptr<Lib3MF::CMeshObject> mesh) const
    {
        bool propertiesChanged = false;

        ImGui::PushID("VolumeDataDropdown");

        // Get current VolumeData for this mesh if it exists
        Lib3MF::PVolumeData currentVolumeData;
        std::string currentVolumeDataName = "None";

        try
        {
            currentVolumeData = mesh->GetVolumeData();
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
                    mesh->SetVolumeData(nullptr);
                    document->markFileAsChanged();
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
                        mesh->SetVolumeData(volumeData);
                        document->markFileAsChanged();
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

    void ResourceView::render(SharedDocument document) const
    {
        if (!document)
        {
            return;
        }
        if (!document->getCore())
        {
            return;
        }

        auto & resourceManager = document->getGeneratorContext().resourceManager;

        auto const & resources = resourceManager.getResourceMap();

        ImGuiTreeNodeFlags const baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                             ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                             ImGuiTreeNodeFlags_SpanAvailWidth;

        ImGuiTreeNodeFlags nodeFlags = baseFlags | ImGuiTreeNodeFlags_Leaf;
        ImGuiTreeNodeFlags infoNodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_DefaultOpen;

        ImGui::BeginGroup();
        if (ImGui::TreeNodeEx("Mesh Resources", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            if (ImGui::Button("Import STL"))
            {
                addMesh(document);
            }

            if (ImGui::Button("Add current bounding box"))
            {
                document->addBoundingBoxAsMesh();
            }

            ImGui::Unindent();

            for (auto const & [key, res] : resources)
            {
                if (!res)
                {
                    continue;
                }
                auto const * mesh = dynamic_cast<MeshResource const *>(res.get());
                if (!mesh)
                {
                    continue;
                }

                auto name =
                  fmt::format("{} #{}", key.getDisplayName(), key.getResourceId().value_or(-1));
                ImGui::BeginGroup();
                if (ImGui::TreeNodeEx(name.c_str(), baseFlags))
                {
                    auto const & meshData = mesh->getMesh();

                    if (ImGui::BeginTable(
                          "MeshData", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                    {
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Faces");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format("{}", meshData.polygonCount()).c_str());

                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Min");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format("({}, {}, {})",
                                                           meshData.getMin().x,
                                                           meshData.getMin().y,
                                                           meshData.getMin().z)
                                                 .c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Max");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format("({}, {}, {})",
                                                           meshData.getMax().x,
                                                           meshData.getMax().y,
                                                           meshData.getMax().z)
                                                 .c_str());

                        // Add Part Number field
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Part Number:");
                        ImGui::TableNextColumn();
                        try
                        {
                            auto model3mf = document->get3mfModel();
                            if (model3mf && key.getResourceId().has_value())
                            {
                                auto lib3mfUniqueResourceId =
                                  gladius::io::resourceIdToUniqueResourceId(
                                    model3mf, key.getResourceId().value());

                                auto resource = model3mf->GetResourceByID(lib3mfUniqueResourceId);
                                if (resource)
                                {
                                    // Convert resource to Object since only Objects have part
                                    // numbers
                                    auto object =
                                      std::dynamic_pointer_cast<Lib3MF::CObject>(resource);
                                    if (object)
                                    {
                                        std::string partNumber = object->GetPartNumber();
                                        if (ImGui::InputText("##PartNumber",
                                                             &partNumber,
                                                             ImGuiInputTextFlags_None))
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
                            }

                            // Add dropdown for selecting a volume data
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("Volume Data:");
                            ImGui::TableNextColumn();
                            try
                            {
                                auto model3mf = document->get3mfModel();
                                if (model3mf && key.getResourceId().has_value())
                                {
                                    auto lib3mfUniqueResourceId =
                                      gladius::io::resourceIdToUniqueResourceId(
                                        model3mf, key.getResourceId().value());

                                    auto resource =
                                      model3mf->GetResourceByID(lib3mfUniqueResourceId);
                                    if (resource)
                                    {
                                        auto meshObject =
                                          std::dynamic_pointer_cast<Lib3MF::CMeshObject>(resource);
                                        if (meshObject)
                                        {
                                            renderVolumeDataDropdown(
                                              document, model3mf, meshObject);
                                        }
                                    }
                                }
                            }

                            catch (...)
                            {
                                // Handle errors silently
                            }
                        }
                        catch (...)
                        {
                            // Handle errors silently
                        }

                        ImGui::EndTable();
                    }

                    // always show delete button, but indicate dependencies
                    auto safeResult = document->isItSafeToDeleteResource(key);
                    if (ImGui::Button("Delete"))
                    {
                        if (safeResult.canBeRemoved)
                        {
                            document->deleteResource(key);
                        }
                    }

                    if (!safeResult.canBeRemoved)
                    {
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::TextColored(
                              ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                              "Cannot delete, the resource is referenced by another item:");
                            for (auto const & depRes : safeResult.dependentResources)
                            {
                                ImGui::BulletText("Resource ID: %u", depRes->GetModelResourceID());
                            }
                            for (auto const & depItem : safeResult.dependentBuildItems)
                            {
                                ImGui::BulletText("Build item: %u", depItem->GetObjectResourceID());
                            }
                            ImGui::EndTooltip();
                        }
                    }

                    ImGui::TreePop();
                }
                ImGui::EndGroup();
                frameOverlay(ImVec4(1.0f, 1.0f, 1.0f, 0.2f),
                            "Mesh Resource Details\n\n"
                            "View vertices, triangles, and properties of this mesh.\n"
                            "Meshes define the shape of objects using triangular surfaces.");
            }
            ImGui::TreePop();
        }

        ImGui::EndGroup();
        frameOverlay(ImVec4(0.5f, 1.0f, 0.5f, 0.1f),
                    "Mesh Resources\n\n"
                    "Traditional 3D models made of triangles.\n"
                    "Meshes define the surface of your objects using connected triangles\n"
                    "and can include properties like color and texture.");

        // image stack
        ImGui::BeginGroup();
        if (ImGui::TreeNodeEx("Image Stacks", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            // import image stack
            if (ImGui::Button("Import from directory"))
            {
                // query directory
                const auto directory = queryDirectory();
                if (!directory.has_value())
                {
                    return;
                }

                document->addImageStackResource(directory.value());
            }

            ImGui::Unindent();

            for (auto const & [key, res] : resources)
            {
                auto const * stack = dynamic_cast<ImageStackResource const *>(res.get());
                auto const * grid = dynamic_cast<VdbResource const *>(res.get());
                if (!stack && !grid)
                {
                    continue;
                }

                ImGui::BeginGroup();
                if (ImGui::TreeNodeEx(key.getDisplayName().c_str(), baseFlags))
                {
                    (ImGui::TextUnformatted(fmt::format("# {} loaded as {}",
                                                        key.getResourceId().value_or(-1),
                                                        stack ? "image stack" : "vdb grid")
                                              .c_str()));

                    if (grid)
                    {
                        auto dimensions = grid->getGridSize();
                        if (ImGui::TreeNodeEx(
                              fmt::format(
                                "Size: {}x{}x{}", dimensions.x, dimensions.y, dimensions.z)
                                .c_str(),
                              infoNodeFlags))
                        {
                            ImGui::TreePop();
                        }
                    }

                    // Add Part Number field for image resources
                    if (ImGui::TreeNodeEx("Properties", infoNodeFlags))
                    {
                        if (ImGui::BeginTable("ResourceProperties",
                                              2,
                                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                        {
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("Part Number:");
                            ImGui::TableNextColumn();
                            try
                            {
                                auto model3mf = document->get3mfModel();
                                if (model3mf && key.getResourceId().has_value())
                                {
                                    auto lib3mfUniqueResourceId =
                                      gladius::io::resourceIdToUniqueResourceId(
                                        model3mf, key.getResourceId().value());

                                    auto resource =
                                      model3mf->GetResourceByID(lib3mfUniqueResourceId);
                                    if (resource)
                                    {
                                        // Convert resource to Object since only Objects have part
                                        // numbers
                                        auto object =
                                          std::dynamic_pointer_cast<Lib3MF::CObject>(resource);
                                        if (object)
                                        {
                                            std::string partNumber = object->GetPartNumber();
                                            if (ImGui::InputText("##ImgPartNumber",
                                                                 &partNumber,
                                                                 ImGuiInputTextFlags_None))
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
                                }
                            }
                            catch (...)
                            {
                                // Handle errors silently
                            }
                            ImGui::EndTable();
                        }
                        ImGui::TreePop();
                    }

                    // delete image stack
                    auto safeResult = document->isItSafeToDeleteResource(key);
                    if (ImGui::Button("Delete"))
                    {
                        if (safeResult.canBeRemoved)
                        {
                            document->deleteResource(key);
                        }
                    }

                    if (!safeResult.canBeRemoved)
                    {
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::TextColored(
                              ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                              "Cannot delete, the resource is referenced by another item:");
                            for (auto const & depRes : safeResult.dependentResources)
                            {
                                ImGui::BulletText("Resource ID: %u", depRes->GetModelResourceID());
                            }
                            for (auto const & depItem : safeResult.dependentBuildItems)
                            {
                                ImGui::BulletText("Build item: %u", depItem->GetObjectResourceID());
                            }
                            ImGui::EndTooltip();
                        }
                    }

                    ImGui::TreePop();
                }
                ImGui::EndGroup();
                frameOverlay(ImVec4(1.0f, 1.0f, 1.0f, 0.2f),
                            "Image Stack Details\n\n"
                            "View and edit the 3D image data used in volumetric models.\n"
                            "These stacked images create a full 3D representation.");
            }
            ImGui::TreePop();
        }
        ImGui::EndGroup();
        frameOverlay(ImVec4(1.0f, 0.65f, 0.0f, 0.1f),
                    "Image Stacks\n\n"
                    "3D image data for volumetric models.\n"
                    "Image stacks store information as voxels (3D pixels) and allow you to\n"
                    "represent object properties that vary throughout the volume.");
    }

    void ResourceView::addMesh(SharedDocument document) const
    {
        if (!document)
        {
            return;
        }

        const auto filename = queryLoadFilename({{"*.stl"}});
        if (!filename.has_value())
        {
            return;
        }
        document->addMeshResource(filename.value());
    }
}