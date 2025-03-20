#include "LevelSetView.h"

namespace gladius::ui
{
    void LevelSetView::render(SharedDocument document) const
    {
        if (!document)
        {
            return;
        }

        auto model = document->get3mfModel();
        if (!model)
        {
            return;
        }

        auto resourceIterator = model->GetResources();

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

            auto levelSet = dynamic_cast<Lib3MF::CLevelSet *>(resource.get());
            if (!levelSet)
            {
                continue;
            }

            auto name = fmt::format("LevelSet #{}", levelSet->GetResourceID());
            ImGui::BeginGroup();
            if (ImGui::TreeNodeEx(name.c_str(), baseFlags))
            {
                ImGui::Text("Function ID: %u", levelSet->GetFunction()->GetResourceID());
                ImGui::Text("Channel: %s", levelSet->GetChannelName().c_str());
                ImGui::Text("Min Feature Size: %f", levelSet->GetMinFeatureSize());
                ImGui::Text("Mesh BBox Only: %s", levelSet->GetMeshBBoxOnly() ? "true" : "false");
                ImGui::Text("Fallback Value: %f", levelSet->GetFallBackValue());

                ImGui::TreePop();
            }
            ImGui::EndGroup();
            // frameOverlay(ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
        }
    }
}
