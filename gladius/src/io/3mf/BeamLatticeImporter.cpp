#include "BeamLatticeImporter.h"
#include "Profiling.h"

#include <fmt/format.h>
#include <lib3mf_types.hpp>

namespace gladius::io
{
    BeamLatticeImporter::BeamLatticeImporter(events::SharedLogger logger)
        : m_eventLogger(logger)
    {
    }

    bool BeamLatticeImporter::process(Lib3MF::PMeshObject meshObject)
    {
        ProfileFunction;

        clear();

        if (!meshObject)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent(
                  {"BeamLatticeImporter: Null mesh object provided", events::Severity::Error});
            }
            return false;
        }

        try
        {
            // Check if mesh object has a beam lattice
            Lib3MF::PBeamLattice beamLattice = meshObject->BeamLattice();
            if (!beamLattice)
            {
                return false; // No beam lattice, not an error
            }

            // Process ball configuration first (needed for beam processing)
            if (!processBallConfig(beamLattice))
            {
                return false;
            }

            // Process beams (also collects vertex indices for ball generation)
            if (!processBeams(beamLattice, meshObject))
            {
                return false;
            }

            // Generate balls based on configuration and collected vertex data
            if (!generateBalls(beamLattice, meshObject))
            {
                return false;
            }

            m_hasBeamLattice = true;

            if (m_eventLogger)
            {
                m_eventLogger->addEvent(
                  {fmt::format("BeamLatticeImporter: Successfully processed {} beams, {} balls",
                               m_beams.size(),
                               m_balls.size()),
                   events::Severity::Info});
            }

            return true;
        }
        catch (const std::exception & e)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent(
                  {fmt::format("BeamLatticeImporter: Error processing beam lattice: {}", e.what()),
                   events::Severity::Error});
            }
            clear();
            return false;
        }
    }

    void BeamLatticeImporter::clear()
    {
        m_beams.clear();
        m_balls.clear();
        m_beamVertexIndices.clear();
        m_ballConfig = BeamLatticeBallConfig{};
        m_hasBeamLattice = false;
    }

    bool BeamLatticeImporter::processBeams(Lib3MF::PBeamLattice beamLattice,
                                           Lib3MF::PMeshObject meshObject)
    {
        ProfileFunction;

        Lib3MF_uint32 beamCount = beamLattice->GetBeamCount();
        if (beamCount == 0)
        {
            return true; // No beams, but not an error
        }

        try
        {
            // Use bulk API to get all beams efficiently
            std::vector<Lib3MF::sBeam> lib3mfBeams;
            beamLattice->GetBeams(lib3mfBeams);

            m_beams.reserve(beamCount);

            // Track vertices for implicit ball generation based on ball mode
            std::map<uint32_t, float> implicitBallVertices; // vertex index -> radius

            for (const Lib3MF::sBeam & beamInfo : lib3mfBeams)
            {
                BeamData beam;

                // Extract vertex indices and collect them for ball generation
                uint32_t startVertexIndex = beamInfo.m_Indices[0];
                uint32_t endVertexIndex = beamInfo.m_Indices[1];

                // Collect vertex indices for efficient ball generation
                m_beamVertexIndices.insert(startVertexIndex);
                m_beamVertexIndices.insert(endVertexIndex);

                // Bounds checking for vertex indices
                if (startVertexIndex >= meshObject->GetVertexCount() ||
                    endVertexIndex >= meshObject->GetVertexCount())
                {
                    if (m_eventLogger)
                    {
                        m_eventLogger->addEvent(
                          {fmt::format(
                             "BeamLatticeImporter: Invalid vertex indices in beam: {} or {} >= {}",
                             startVertexIndex,
                             endVertexIndex,
                             meshObject->GetVertexCount()),
                           events::Severity::Warning});
                    }
                    continue; // Skip this beam
                }

                // Resolve vertex positions
                auto startVertex = meshObject->GetVertex(startVertexIndex);
                auto endVertex = meshObject->GetVertex(endVertexIndex);

                beam.startPos = {static_cast<float>(startVertex.m_Coordinates[0]),
                                 static_cast<float>(startVertex.m_Coordinates[1]),
                                 static_cast<float>(startVertex.m_Coordinates[2]),
                                 1.0f};
                beam.endPos = {static_cast<float>(endVertex.m_Coordinates[0]),
                               static_cast<float>(endVertex.m_Coordinates[1]),
                               static_cast<float>(endVertex.m_Coordinates[2]),
                               1.0f};

                beam.startRadius = static_cast<float>(beamInfo.m_Radii[0]);
                beam.endRadius = static_cast<float>(beamInfo.m_Radii[1]);

                // Cap styles: convert from 3MF to internal representation
                beam.startCapStyle = static_cast<int>(beamInfo.m_CapModes[0]);
                beam.endCapStyle = static_cast<int>(beamInfo.m_CapModes[1]);
                beam.materialId = 0; // Default material
                beam.padding = 0;

                m_beams.push_back(beam);

                // Generate implicit balls based on ball mode and default radius
                if (m_ballConfig.mode == BallMode::All && m_ballConfig.defaultRadius > 0.0f)
                {
                    // For "All" mode, create balls at all beam vertices with default radius
                    implicitBallVertices[startVertexIndex] = m_ballConfig.defaultRadius;
                    implicitBallVertices[endVertexIndex] = m_ballConfig.defaultRadius;
                }
                // Note: BallMode::Mixed and BallMode::None are handled by generateBalls()
                // Mixed mode only creates balls where explicit <ball> elements exist
                // None mode creates no implicit balls
            }

            // Add implicit balls to the ball collection
            for (const auto & implicitBall : implicitBallVertices)
            {
                uint32_t vertexIndex = implicitBall.first;
                float radius = implicitBall.second;

                auto vertex = meshObject->GetVertex(vertexIndex);
                BallData ball;
                ball.positionRadius = {static_cast<float>(vertex.m_Coordinates[0]),
                                       static_cast<float>(vertex.m_Coordinates[1]),
                                       static_cast<float>(vertex.m_Coordinates[2]),
                                       radius};
                m_balls.push_back(ball);
            }

            // Log implicit ball generation
            if (m_eventLogger && implicitBallVertices.size() > 0)
            {
                m_eventLogger->addEvent(
                  {fmt::format(
                     "BeamLatticeImporter: Generated {} implicit balls (mode=All, radius={})",
                     implicitBallVertices.size(),
                     m_ballConfig.defaultRadius),
                   events::Severity::Info});
            }

            return true;
        }
        catch (const std::exception & e)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent(
                  {fmt::format("BeamLatticeImporter: Error processing beams: {}", e.what()),
                   events::Severity::Error});
            }
            return false;
        }
    }

    bool BeamLatticeImporter::processBallConfig(Lib3MF::PBeamLattice beamLattice)
    {
        ProfileFunction;

        try
        {
            Lib3MF::eBeamLatticeBallMode lib3mfBallMode;
            double lib3mfBallRadius;
            beamLattice->GetBallOptions(lib3mfBallMode, lib3mfBallRadius);

            m_ballConfig = convertBallMode(lib3mfBallMode, lib3mfBallRadius);

            // Validate configuration per 3MF spec
            if (m_ballConfig.mode != BallMode::None && m_ballConfig.defaultRadius <= 0.0f)
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent(
                      {fmt::format("BeamLatticeImporter: Ball mode={} but no valid ball radius "
                                   "specified. Defaulting to mode=none.",
                                   static_cast<int>(m_ballConfig.mode)),
                       events::Severity::Warning});
                }
                m_ballConfig.mode = BallMode::None;
            }

            return true;
        }
        catch (const std::exception & e)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent(
                  {fmt::format("BeamLatticeImporter: Error reading ball configuration: {}",
                               e.what()),
                   events::Severity::Warning});
            }
            // Default to no balls
            m_ballConfig = BeamLatticeBallConfig{};
            return true; // Not a fatal error
        }
    }

    bool BeamLatticeImporter::generateBalls(Lib3MF::PBeamLattice beamLattice,
                                            Lib3MF::PMeshObject meshObject)
    {
        ProfileFunction;

        try
        {
            // Step 1: Collect explicit balls from 3MF <balls> elements
            std::map<uint32_t, float> explicitBallRadii; // vertex index -> radius
            std::set<uint32_t> explicitBallVertices;     // track which vertices have explicit balls

            Lib3MF_uint32 ballCount = beamLattice->GetBallCount();
            if (ballCount > 0)
            {
                std::vector<Lib3MF::sBall> lib3mfBalls;
                beamLattice->GetBalls(lib3mfBalls);

                for (const auto & ballInfo : lib3mfBalls)
                {
                    uint32_t vertexIndex = ballInfo.m_Index;
                    float radius = static_cast<float>(ballInfo.m_Radius);

                    // Use default radius if explicit radius is not specified
                    if (radius <= 0.0f)
                    {
                        radius = m_ballConfig.defaultRadius;
                    }

                    explicitBallRadii[vertexIndex] = radius;
                    explicitBallVertices.insert(vertexIndex);
                }
            }

            // Step 2: Handle Mixed mode - only add balls for vertices with explicit <ball> elements
            if (m_ballConfig.mode == BallMode::Mixed)
            {
                for (uint32_t vertexIndex : m_beamVertexIndices)
                {
                    // Only create balls at vertices that have explicit <ball> elements
                    if (explicitBallVertices.count(vertexIndex) > 0)
                    {
                        float ballRadius = explicitBallRadii[vertexIndex];

                        // Bounds checking for vertex index
                        if (vertexIndex >= meshObject->GetVertexCount())
                        {
                            if (m_eventLogger)
                            {
                                m_eventLogger->addEvent(
                                  {fmt::format("BeamLatticeImporter: Invalid vertex index for "
                                               "mixed mode ball: {} >= {}",
                                               vertexIndex,
                                               meshObject->GetVertexCount()),
                                   events::Severity::Warning});
                            }
                            continue;
                        }

                        auto vertex = meshObject->GetVertex(vertexIndex);
                        BallData ball;
                        ball.positionRadius = {static_cast<float>(vertex.m_Coordinates[0]),
                                               static_cast<float>(vertex.m_Coordinates[1]),
                                               static_cast<float>(vertex.m_Coordinates[2]),
                                               ballRadius};
                        m_balls.push_back(ball);
                    }
                }

                if (m_eventLogger && explicitBallVertices.size() > 0)
                {
                    m_eventLogger->addEvent(
                      {fmt::format("BeamLatticeImporter: Generated {} mixed mode balls",
                                   explicitBallVertices.size()),
                       events::Severity::Info});
                }
            }

            // Step 3: Add explicit balls that are not at beam vertices (standalone balls)
            // These should be preserved regardless of mode, per 3MF specification
            for (const auto & explicitBall : explicitBallRadii)
            {
                uint32_t vertexIndex = explicitBall.first;
                float radius = explicitBall.second;

                // Only add if this vertex is not already handled by beam processing
                // (to avoid duplicates for All mode and Mixed mode)
                bool isBeamVertex = (m_beamVertexIndices.count(vertexIndex) > 0);
                bool shouldAddStandalone = !isBeamVertex;

                // For None mode, add all explicit balls regardless of beam vertex status
                if (m_ballConfig.mode == BallMode::None)
                {
                    shouldAddStandalone = true;
                }

                if (shouldAddStandalone)
                {
                    // Bounds checking for vertex index
                    if (vertexIndex >= meshObject->GetVertexCount())
                    {
                        if (m_eventLogger)
                        {
                            m_eventLogger->addEvent(
                              {fmt::format("BeamLatticeImporter: Invalid vertex index for explicit "
                                           "ball: {} >= {}",
                                           vertexIndex,
                                           meshObject->GetVertexCount()),
                               events::Severity::Warning});
                        }
                        continue;
                    }

                    auto vertex = meshObject->GetVertex(vertexIndex);
                    BallData ball;
                    ball.positionRadius = {static_cast<float>(vertex.m_Coordinates[0]),
                                           static_cast<float>(vertex.m_Coordinates[1]),
                                           static_cast<float>(vertex.m_Coordinates[2]),
                                           radius};
                    m_balls.push_back(ball);
                }
            }

            // Log explicit ball information
            if (m_eventLogger && explicitBallRadii.size() > 0)
            {
                size_t standaloneCount = 0;
                for (const auto & explicitBall : explicitBallRadii)
                {
                    uint32_t vertexIndex = explicitBall.first;
                    bool isBeamVertex = (m_beamVertexIndices.count(vertexIndex) > 0);

                    if (!isBeamVertex || m_ballConfig.mode == BallMode::None)
                    {
                        standaloneCount++;
                    }
                }

                if (standaloneCount > 0)
                {
                    m_eventLogger->addEvent(
                      {fmt::format("BeamLatticeImporter: Added {} standalone explicit balls",
                                   standaloneCount),
                       events::Severity::Info});
                }
            }

            return true;
        }
        catch (const std::exception & e)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent(
                  {fmt::format("BeamLatticeImporter: Error generating balls: {}", e.what()),
                   events::Severity::Error});
            }
            return false;
        }
    }

    BeamLatticeBallConfig
    BeamLatticeImporter::convertBallMode(Lib3MF::eBeamLatticeBallMode lib3mfBallMode,
                                         double lib3mfBallRadius)
    {
        ProfileFunction;

        BeamLatticeBallConfig ballConfig;

        // Convert lib3mf enum to internal enum
        switch (lib3mfBallMode)
        {
        case Lib3MF::eBeamLatticeBallMode::BeamLatticeBallModeNone:
            ballConfig.mode = BallMode::None;
            break;
        case Lib3MF::eBeamLatticeBallMode::Mixed:
            ballConfig.mode = BallMode::Mixed;
            break;
        case Lib3MF::eBeamLatticeBallMode::All:
            ballConfig.mode = BallMode::All;
            break;
        default:
            ballConfig.mode = BallMode::None;
            break;
        }

        ballConfig.defaultRadius = static_cast<float>(lib3mfBallRadius);

        return ballConfig;
    }
}
