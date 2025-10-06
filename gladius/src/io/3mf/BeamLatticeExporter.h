#pragma once

#include "BeamLatticeResource.h"
#include "EventLogger.h"
#include "kernel/types.h"

#include <Bindings/Cpp/lib3mf_implicit.hpp>
#include <memory>
#include <vector>

namespace gladius::io
{
    /**
     * @brief Exporter for beam lattice data to 3MF files.
     *
     * This class handles the conversion of internal beam lattice structures
     * to 3MF beam lattice format using the lib3mf API.
     */
    class BeamLatticeExporter
    {
      public:
        /**
         * @brief Construct a beam lattice exporter.
         * @param logger Event logger for warnings and errors
         */
        explicit BeamLatticeExporter(events::SharedLogger logger = nullptr);

        /**
         * @brief Export beam data to a 3MF mesh object.
         *
         * @param meshObject The 3MF mesh object to add beam lattice data to
         * @param beams Vector of beam data structures to export
         * @param balls Vector of ball data structures to export
         * @param ballConfig Configuration for ball generation
         * @return True if export was successful, false otherwise
         */
        bool exportToMeshObject(Lib3MF::PMeshObject meshObject,
                                std::vector<BeamData> const & beams,
                                std::vector<BallData> const & balls,
                                BeamLatticeBallConfig const & ballConfig);

      private:
        events::SharedLogger m_eventLogger;
    };
}
