#pragma once

#include "Primitives.h"
#include <filesystem>
#include <fstream>

namespace gladius
{
    /// Quick and dirty ASCII CLI Reader that only reads Hatches (of all layers)
    class CliReader
    {
      public:
        /**
         * Reads the specified file and populates the provided Primitives object.
         * @param fileName The path to the file to be read.
         * @param primitives The Primitives object to be populated.
         */
        void read(const std::filesystem::path & fileName, Primitives & primitives);

      private:
        /**
         * Calculates the bounding volumes for the provided Primitives object.
         * @param primitives The Primitives object for which to calculate bounding volumes.
         */
        static void calculateBoundingVolumes(Primitives & primitives);

        std::ifstream m_file; ///< Input file stream for reading the file.
    };
}