#include "BeamLatticeExporter.h"
#include "kernel/types.h"

#include <fmt/format.h>
#include <map>
#include <tuple>

namespace gladius::io
{
    BeamLatticeExporter::BeamLatticeExporter(events::SharedLogger logger)
        : m_eventLogger(logger)
    {
    }

    bool BeamLatticeExporter::exportToMeshObject(Lib3MF::PMeshObject meshObject,
                                                 std::vector<BeamData> const & beams,
                                                 std::vector<BallData> const & balls,
                                                 BeamLatticeBallConfig const & ballConfig)
    {
        if (!meshObject)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent(
                  {"Invalid mesh object for beam lattice export", events::Severity::Error});
            }
            return false;
        }

        try
        {
            // Get the beam lattice from the mesh object
            auto beamLattice = meshObject->BeamLattice();
            if (!beamLattice)
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent(
                      {"Failed to get beam lattice from mesh object", events::Severity::Error});
                }
                return false;
            }

            // Add vertices (extract unique vertices from beam endpoints)
            std::vector<Lib3MF::sPosition> vertices;
            std::map<std::tuple<float, float, float>, Lib3MF_uint32> vertexMap;

            auto getOrAddVertex = [&](float x, float y, float z) -> Lib3MF_uint32
            {
                auto key = std::make_tuple(x, y, z);
                auto it = vertexMap.find(key);
                if (it != vertexMap.end())
                {
                    return it->second;
                }

                Lib3MF::sPosition pos;
                pos.m_Coordinates[0] = x;
                pos.m_Coordinates[1] = y;
                pos.m_Coordinates[2] = z;

                Lib3MF_uint32 index = static_cast<Lib3MF_uint32>(vertices.size());
                vertices.push_back(pos);
                vertexMap[key] = index;
                return index;
            };

            // Process beams and extract vertices
            std::vector<std::pair<Lib3MF_uint32, Lib3MF_uint32>> beamIndices;
            for (auto const & beam : beams)
            {
                Lib3MF_uint32 startIdx =
                  getOrAddVertex(beam.startPos.s[0], beam.startPos.s[1], beam.startPos.s[2]);
                Lib3MF_uint32 endIdx =
                  getOrAddVertex(beam.endPos.s[0], beam.endPos.s[1], beam.endPos.s[2]);
                beamIndices.push_back({startIdx, endIdx});
            }

            // Add all vertices to the mesh object
            for (auto const & vertex : vertices)
            {
                meshObject->AddVertex(vertex);
            }

            // Set ball options
            Lib3MF::eBeamLatticeBallMode ballMode;
            switch (ballConfig.mode)
            {
            case BallMode::None:
                ballMode = Lib3MF::eBeamLatticeBallMode::BeamLatticeBallModeNone;
                break;
            case BallMode::Mixed:
                ballMode = Lib3MF::eBeamLatticeBallMode::Mixed;
                break;
            case BallMode::All:
                ballMode = Lib3MF::eBeamLatticeBallMode::All;
                break;
            default:
                ballMode = Lib3MF::eBeamLatticeBallMode::BeamLatticeBallModeNone;
            }

            if (ballMode != Lib3MF::eBeamLatticeBallMode::BeamLatticeBallModeNone)
            {
                beamLattice->SetBallOptions(ballMode, ballConfig.defaultRadius);
            }

            // Add beams
            for (size_t i = 0; i < beams.size(); ++i)
            {
                auto const & beam = beams[i];
                auto const & indices = beamIndices[i];

                Lib3MF::sBeam lib3mfBeam;
                lib3mfBeam.m_Indices[0] = indices.first;
                lib3mfBeam.m_Indices[1] = indices.second;
                lib3mfBeam.m_Radii[0] = beam.startRadius;
                lib3mfBeam.m_Radii[1] = beam.endRadius;

                // Map cap styles
                auto mapCapStyle = [](int capStyle) -> Lib3MF::eBeamLatticeCapMode
                {
                    switch (capStyle)
                    {
                    case 0:
                        return Lib3MF::eBeamLatticeCapMode::HemiSphere;
                    case 1:
                        return Lib3MF::eBeamLatticeCapMode::Sphere;
                    case 2:
                        return Lib3MF::eBeamLatticeCapMode::Butt;
                    default:
                        return Lib3MF::eBeamLatticeCapMode::HemiSphere;
                    }
                };

                lib3mfBeam.m_CapModes[0] = mapCapStyle(beam.startCapStyle);
                lib3mfBeam.m_CapModes[1] = mapCapStyle(beam.endCapStyle);

                beamLattice->AddBeam(lib3mfBeam);
            }

            // Add explicit balls if in Mixed mode
            if (ballMode == Lib3MF::eBeamLatticeBallMode::Mixed && !balls.empty())
            {
                for (auto const & ball : balls)
                {
                    // BallData.positionRadius: xyz = position, w = radius
                    auto key = std::make_tuple(
                      ball.positionRadius.s[0], ball.positionRadius.s[1], ball.positionRadius.s[2]);
                    auto it = vertexMap.find(key);
                    if (it != vertexMap.end())
                    {
                        Lib3MF::sBall lib3mfBall;
                        lib3mfBall.m_Index = it->second;
                        lib3mfBall.m_Radius = ball.positionRadius.s[3]; // w component is radius
                        beamLattice->AddBall(lib3mfBall);
                    }
                }
            }

            if (m_eventLogger)
            {
                m_eventLogger->addEvent(
                  {fmt::format("Successfully exported beam lattice with {} beams and {} vertices",
                               beams.size(),
                               vertices.size()),
                   events::Severity::Info});
            }

            return true;
        }
        catch (std::exception const & e)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent({fmt::format("Error exporting beam lattice: {}", e.what()),
                                         events::Severity::Error});
            }
            return false;
        }
    }
}
