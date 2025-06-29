#pragma once

#include <filesystem>
#include <lib3mf_implicit.hpp>

namespace gladius::io
{
    using Files = std::vector<std::filesystem::path>;



    class ImageStackCreator
    {
        public:
            Lib3MF::PFunctionFromImage3D importDirectoryAsFunctionFromImage3D(Lib3MF::PModel model, std::filesystem::path const& path);
            Lib3MF::PImageStack addImageStackFromDirectory(Lib3MF::PModel model, std::filesystem::path const& path);
            Files getFiles(std::filesystem::path const& path) const;
        private:
            unsigned m_rows = 0;
            unsigned m_cols = 0;
            unsigned m_numSheets = 0;
            

            void determineImageStackSize(Files const& files);
    };
}