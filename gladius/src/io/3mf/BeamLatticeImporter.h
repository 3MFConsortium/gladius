#pragma once

#include "BeamLatticeResource.h"
#include "EventLogger.h"
#include "kernel/types.h"

#include <lib3mf_implicit.hpp>
#include <map>
#include <set>
#include <vector>

namespace gladius::io
{
    /**
     * @brief Unified importer for beam lattice data from 3MF files.
     *
     * This class efficiently processes both beams and balls from a mesh object
     * with beam lattice data, ensuring single-pass reading and proper ball
     * generation according to the 3MF specification.
     */
    class BeamLatticeImporter
    {
      public:
        /**
         * @brief Construct a beam lattice importer.
         * @param logger Event logger for warnings and errors
         */
        explicit BeamLatticeImporter(events::SharedLogger logger = nullptr);

        /**
         * @brief Process beam lattice data from a mesh object.
         * @param meshObject 3MF mesh object containing beam lattice data
         * @return True if processing was successful, false otherwise
         */
        bool process(Lib3MF::PMeshObject meshObject);

        /**
         * @brief Get the processed beam data.
         * @return Vector of beam data structures
         */
        const std::vector<BeamData> & getBeams() const
        {
            return m_beams;
        }

        /**
         * @brief Get the processed ball data.
         * @return Vector of ball data structures
         */
        const std::vector<BallData> & getBalls() const
        {
            return m_balls;
        }

        /**
         * @brief Get the ball configuration.
         * @return Ball configuration used for processing
         */
        const BeamLatticeBallConfig & getBallConfig() const
        {
            return m_ballConfig;
        }

        /**
         * @brief Check if the importer has valid beam lattice data.
         * @return True if beam lattice was processed successfully
         */
        bool hasBeamLattice() const
        {
            return m_hasBeamLattice;
        }

        /**
         * @brief Clear all processed data.
         */
        void clear();

      private:
        /**
         * @brief Process beams from the beam lattice and collect vertex indices.
         * @param beamLattice 3MF beam lattice object
         * @param meshObject 3MF mesh object for vertex resolution
         * @return True if successful
         */
        bool processBeams(Lib3MF::PBeamLattice beamLattice, Lib3MF::PMeshObject meshObject);

        /**
         * @brief Process ball configuration from the beam lattice.
         * @param beamLattice 3MF beam lattice object
         * @return True if successful
         */
        bool processBallConfig(Lib3MF::PBeamLattice beamLattice);

        /**
         * @brief Generate balls based on ball mode and explicit ball data.
         * @param beamLattice 3MF beam lattice object
         * @param meshObject 3MF mesh object for vertex resolution
         * @return True if successful
         */
        bool generateBalls(Lib3MF::PBeamLattice beamLattice, Lib3MF::PMeshObject meshObject);

        /**
         * @brief Convert 3MF ball mode to internal representation.
         * @param lib3mfBallMode Ball mode from lib3mf
         * @param lib3mfBallRadius Ball radius from lib3mf
         * @return Internal ball configuration
         */
        BeamLatticeBallConfig convertBallMode(Lib3MF::eBeamLatticeBallMode lib3mfBallMode,
                                              double lib3mfBallRadius);

        events::SharedLogger m_eventLogger;

        std::vector<BeamData> m_beams;
        std::vector<BallData> m_balls;
        std::set<uint32_t> m_beamVertexIndices;
        BeamLatticeBallConfig m_ballConfig;

        bool m_hasBeamLattice = false;
    };
}
