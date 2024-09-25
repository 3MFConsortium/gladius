#pragma once

#include <filesystem>
#include <lib3mf/Cpp/lib3mf_implicit.hpp>

namespace gladius::io
{
    using Files = std::vector<std::filesystem::path>;



    class ImageStackCreator
    {
        public:
            Lib3MF::PFunctionFromImage3D importDirectoryAsFunctionFromImage3D(Lib3MF::PModel model, std::filesystem::path const& path);
        private:
            unsigned m_rows = 0;
            unsigned m_cols = 0;
            unsigned m_numSheets = 0;
            
            Files getFiles(std::filesystem::path const& path) const;

            void determineImageStackSize(Files const& files);
            Lib3MF::PImageStack addImageStackFromDirectory(Lib3MF::PModel model, std::filesystem::path const& path);
    };
}