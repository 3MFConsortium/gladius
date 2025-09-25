#include "ImageStackCreator.h"
#include "ImageExtractor.h"
#include <fmt/format.h>
#include <lodepng.h>

namespace gladius::io
{
    Lib3MF::PImageStack
    ImageStackCreator::addImageStackFromDirectory(Lib3MF::PModel model,
                                                  std::filesystem::path const & path)
    {
        auto files = getFiles(path);
        if (files.empty())
        {
            return {};
        }

        determineImageStackSize(files);

        auto stack = model->AddImageStack(m_cols, m_rows, m_numSheets);

        unsigned int i = 0u;

        for (auto & file : files)
        {
            auto newFileName =
              fmt::format("/volume/Image_{}_layer_{}.png", file.stem().string(), i);
            stack->CreateSheetFromFile(i, newFileName, file.string());
            ++i;
        }

        return stack;
    }

    Lib3MF::PFunctionFromImage3D
    ImageStackCreator::importDirectoryAsFunctionFromImage3D(Lib3MF::PModel model,
                                                            std::filesystem::path const & path)
    {
        auto stack = addImageStackFromDirectory(model, path);
        if (stack)
        {
            auto function = model->AddFunctionFromImage3D(stack.get());
            return function;
        }

        return {};
    }

    Files ImageStackCreator::getFiles(std::filesystem::path const & path) const
    {
        if (!std::filesystem::is_directory(path))
        {
            return {};
        }

        // get all png files in the direcotry and order them alpahnumerically by name
        Files files;
        for (auto & p : std::filesystem::directory_iterator(path))
        {
            if (p.path().extension() == ".png")
            {
                files.push_back(p.path());
            }
        }

        std::sort(files.begin(),
                  files.end(),
                  [](std::filesystem::path const & a, std::filesystem::path const & b)
                  { return a.filename().string() < b.filename().string(); });

        return files;
    }

    void ImageStackCreator::determineImageStackSize(Files const & files)
    {
        if (files.empty())
        {
            return;
        }

        // get the size of the first image
        std::vector<unsigned char> buffer;
        if (0 != lodepng::load_file(buffer, files.front().string()))
        {
            throw std::runtime_error("Error loading image file: " + files.front().string());
        }

        lodepng::State state;
        lodepng_inspect(&m_cols, &m_rows, &state, &buffer[0], buffer.size());
        m_numSheets = static_cast<unsigned int>(files.size());
    }
}