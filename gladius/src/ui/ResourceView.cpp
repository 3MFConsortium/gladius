#include "ResourceView.h"

#include "FileChooser.h"
#include "ImageStackResource.h"
#include "MeshResource.h"
#include "ResourceManager.h"
#include "Widgets.h"

#include "imgui.h"

namespace gladius::ui
{

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
        if (ImGui::TreeNodeEx("mesh resources", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            if (ImGui::Button("import STL"))
            {
                addMesh(document);
            }

            if (ImGui::Button("add current bounding box"))
            {
                document->addBoundingBoxAsMesh();
            }

            ImGui::Unindent();

            for (auto const & [key, res] : resources)
            {
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
                        ImGui::TextUnformatted("Faces:");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format("{}", meshData.polygonCount()).c_str());

                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Min:");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format("({}, {}, {})",
                                                           meshData.getMin().x,
                                                           meshData.getMin().y,
                                                           meshData.getMin().z)
                                                 .c_str());

                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Max:");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format("({}, {}, {})",
                                                           meshData.getMax().x,
                                                           meshData.getMax().y,
                                                           meshData.getMax().z)
                                                 .c_str());

                        ImGui::EndTable();
                    }

                    if (ImGui::Button("delete"))
                    {
                        document->deleteResource(key);
                    }

                    ImGui::TreePop();
                }
                ImGui::EndGroup();
                frameOverlay(ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
            }
            ImGui::TreePop();
        }

        ImGui::EndGroup();
        frameOverlay(ImVec4(0.5f, 1.0f, 0.5f, 0.1f));

        // image stack
        ImGui::BeginGroup();
        if (ImGui::TreeNodeEx("image stacks", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            // import image stack
            if (ImGui::Button("import from directory"))
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
                                "size: {}x{}x{}", dimensions.x, dimensions.y, dimensions.z)
                                .c_str(),
                              infoNodeFlags))
                        {
                            ImGui::TreePop();
                        }
                    }

                    // delete image stack
                    if (ImGui::Button("delete"))
                    {
                        document->deleteResource(key);
                    }

                    ImGui::TreePop();
                }
                ImGui::EndGroup();
                frameOverlay(ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
            }
            ImGui::TreePop();
        }
        ImGui::EndGroup();
        frameOverlay(ImVec4(1.0f, 0.65f, 0.0f, 0.1f));
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