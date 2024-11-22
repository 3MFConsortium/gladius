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

        if (ImGui::TreeNodeEx("mesh", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Button("add mesh"))
            {
                addMesh(document);
            }

            ImGui::SameLine();

            if (ImGui::Button("add bounding box"))
            {
                document->addBoundingBoxAsMesh();
            }

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
                            document->deleteResource(key.getResourceId().value());
                            resourceManager.deleteResource(key);
                        }

                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        // image stack
        if (ImGui::TreeNodeEx("image stack", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
        {

            for (auto const & [key, res] : resources)
            {
                auto const * stack = dynamic_cast<ImageStackResource const *>(res.get());
                if (!stack)
                {
                    continue;
                }

                if (ImGui::TreeNodeEx(key.getDisplayName().c_str(), nodeFlags))
                {

                    ImGui::SameLine();
                    ImGui::TextUnformatted(
                      fmt::format(" Resource id: # {}", key.getResourceId().value_or(-1)).c_str());
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