#include "BeamLatticeView.h"
#include "BeamLatticeResource.h"
#include "FileChooser.h"
#include "ResourceManager.h"
#include "Widgets.h"

#include "imgui.h"
#include <cstring>
#include <fmt/format.h>

namespace gladius::ui
{
    bool BeamLatticeView::render(SharedDocument document)
    {
        if (!document)
        {
            return false;
        }
        if (!document->getCore())
        {
            return false;
        }

        auto & resourceManager = document->getGeneratorContext().resourceManager;
        auto const & resources = resourceManager.getResourceMap();

        bool propertiesChanged = false;

        renderImportDialog(document, propertiesChanged);

        ImGuiTreeNodeFlags const baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                             ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                             ImGuiTreeNodeFlags_SpanAvailWidth;

        ImGuiTreeNodeFlags infoNodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_DefaultOpen;

        ImGui::BeginGroup();
        if (ImGui::TreeNodeEx("Beam Lattices", baseFlags | ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Import button
            if (ImGui::Button("Import STL as Beam Lattice..."))
            {
                m_showImportDialog = true;
            }

            bool hasBeamLattices = false;

            for (auto const & [key, res] : resources)
            {
                if (!res)
                {
                    continue;
                }
                auto const * beamLattice = dynamic_cast<BeamLatticeResource const *>(res.get());
                if (!beamLattice)
                {
                    continue;
                }

                hasBeamLattices = true;

                ImGui::BeginGroup();
                if (ImGui::TreeNodeEx(getBeamLatticeName(key).c_str(), baseFlags))
                {
                    ImGui::TextUnformatted(
                      fmt::format("Resource ID: {}", key.getResourceId().value_or(-1)).c_str());

                    // Display beam lattice statistics
                    if (ImGui::TreeNodeEx("Statistics", infoNodeFlags))
                    {
                        if (ImGui::BeginTable("BeamLatticeStats",
                                              2,
                                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                        {
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("Beam Count:");
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(
                              fmt::format("{}", beamLattice->getBeams().size()).c_str());

                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("Ball Count:");
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(
                              fmt::format("{}", beamLattice->getBalls().size()).c_str());

                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("Total Primitives:");
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(
                              fmt::format("{}", beamLattice->getTotalPrimitiveCount()).c_str());

                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("BVH Nodes:");
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(
                              fmt::format("{}", beamLattice->getBVHNodes().size()).c_str());

                            // Display BVH statistics if available
                            auto const & buildStats = beamLattice->getBuildStats();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("BVH Depth:");
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(fmt::format("{}", buildStats.maxDepth).c_str());

                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("Leaf Nodes:");
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(fmt::format("{}", buildStats.leafNodes).c_str());

                            ImGui::EndTable();
                        }
                        ImGui::TreePop();
                    }

                    // Display properties if available
                    if (ImGui::TreeNodeEx("Properties", infoNodeFlags))
                    {
                        if (ImGui::BeginTable("BeamLatticeProperties",
                                              2,
                                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                        {
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("Display Name:");
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(key.getDisplayName().c_str());

                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted("Has Balls:");
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(beamLattice->hasBalls() ? "Yes" : "No");

                            ImGui::EndTable();
                        }
                        ImGui::TreePop();
                    }

                    // Delete button (similar to other resource views)
                    auto safeResult = document->isItSafeToDeleteResource(key);
                    if (ImGui::Button("Delete"))
                    {
                        if (safeResult.canBeRemoved)
                        {
                            document->deleteResource(key);
                            propertiesChanged = true;
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
                frameOverlay(
                  ImVec4(1.0f, 1.0f, 1.0f, 0.2f),
                  "Beam Lattice Details\n\n"
                  "View the structure and properties of this beam lattice.\n"
                  "Beam lattices define complex structural geometries using beams and nodes.");
            }

            if (!hasBeamLattices)
            {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                                   "No beam lattice resources found");
            }

            ImGui::TreePop();
        }
        ImGui::EndGroup();
        frameOverlay(ImVec4(0.8f, 0.4f, 1.0f, 0.1f),
                     "Beam Lattices\n\n"
                     "Complex structural geometries made of interconnected beams.\n"
                     "Beam lattices are ideal for lightweight structures, supports,\n"
                     "and metamaterials with specific mechanical properties.");

        return propertiesChanged;
    }

    void BeamLatticeView::renderImportDialog(SharedDocument document, bool & propertiesChanged)
    {
        if (!m_showImportDialog)
        {
            return;
        }

        ImVec2 const center(ImGui::GetIO().DisplaySize.x * 0.5f,
                            ImGui::GetIO().DisplaySize.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::OpenPopup("Import STL as Beam Lattice");

        if (ImGui::BeginPopupModal(
              "Import STL as Beam Lattice", &m_showImportDialog, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Select an STL file to import as a beam lattice structure.");
            ImGui::Separator();

            // Filename input with browse button
            ImGui::Text("File:");
            ImGui::PushItemWidth(300);
            char filenameBuf[512];
            size_t copyLen = std::min(m_filename.length(), sizeof(filenameBuf) - 1);
            std::memcpy(filenameBuf, m_filename.c_str(), copyLen);
            filenameBuf[copyLen] = '\0';
            if (ImGui::InputText("##filename", filenameBuf, sizeof(filenameBuf)))
            {
                m_filename = filenameBuf;
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                auto result = queryLoadFilename({{"*.stl"}});
                if (result)
                {
                    m_filename = result->generic_string();
                }
            }

            // Beam diameter input
            ImGui::Text("Beam Diameter:");
            ImGui::PushItemWidth(150);
            ImGui::InputFloat("##beamDiameter", &m_beamDiameter, 0.1f, 1.0f, "%.2f");
            ImGui::PopItemWidth();

            ImGui::Separator();

            // Create and Cancel buttons
            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                if (!m_filename.empty() && m_beamDiameter > 0.0f)
                {
                    document->addMeshAsBeamLattice(m_filename, m_beamDiameter);
                    m_showImportDialog = false;
                    m_filename.clear();
                    m_beamDiameter = 2.0f;
                    propertiesChanged = true;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                m_showImportDialog = false;
                m_filename.clear();
                m_beamDiameter = 2.0f;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    std::string BeamLatticeView::getBeamLatticeName(const ResourceKey & key)
    {
        auto displayName = key.getDisplayName();
        if (displayName.empty())
        {
            return fmt::format("Beam Lattice #{}", key.getResourceId().value_or(-1));
        }
        return displayName;
    }
}
