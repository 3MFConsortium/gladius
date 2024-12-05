#include "ResourceView.h"

#include "FileChooser.h"
#include "ImageStackResource.h"
#include "MeshResource.h"
#include "ResourceManager.h"

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

        if (ImGui::TreeNodeEx("mesh resources", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            if (ImGui::Button("add"))
            {
                addMesh(document);
            }

            ImGui::SameLine();

            if (ImGui::Button("add bounding box"))
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

                if (ImGui::TreeNodeEx(name.c_str(), baseFlags))
                {
                    auto const & meshData = mesh->getMesh();

                    if (ImGui::TreeNodeEx(fmt::format("faces: {}", meshData.polygonCount()).c_str(),
                                          infoNodeFlags))
                    {
                        ImGui::TreePop();
                    }

                    if (ImGui::TreeNodeEx(fmt::format("min: ({}, {}, {})  max: {}, {}, {})",
                                                      meshData.getMin().x,
                                                      meshData.getMin().y,
                                                      meshData.getMin().z,
                                                      meshData.getMax().x,
                                                      meshData.getMax().y,
                                                      meshData.getMax().z)
                                            .c_str(),
                                          infoNodeFlags))
                    {

                        if (ImGui::Button("delete"))
                        {
                            document->deleteResource(key);
                        }

                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        // image stack
        if (ImGui::TreeNodeEx("image stacks", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            // import image stack
            if (ImGui::Button("add"))
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

                if (ImGui::TreeNodeEx(key.getDisplayName().c_str(), baseFlags))
                {
                    if (ImGui::TreeNodeEx(fmt::format("# {} loaded as {}",
                                                      key.getResourceId().value_or(-1),
                                                      stack ? "image stack" : "vdb grid")
                                            .c_str(),
                                          infoNodeFlags))
                    {
                        ImGui::TreePop();
                    }

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
            }
            ImGui::TreePop();
        }
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