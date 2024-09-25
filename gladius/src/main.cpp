#include "Application.h"
#include <filesystem>

using namespace std;

void start(int argc, char ** argv)
{
    std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());
    gladius::Application app{{}};
}

int main(int argc, char ** argv)
{
    // get filename from command line
    if (argc < 2)
    {
        std::cout << "No file specified" << std::endl;
        start(argc, argv);
        return 0;
    }
    std::filesystem::path filename{argv[1]};
    filename = std::filesystem::absolute(filename);
    if (!std::filesystem::exists(filename))
    {
        std::cout << "File does not exist: " << filename << std::endl;
        start(argc, argv);
        return 0;
    }
    std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());    gladius::Application app{filename};
    return 0;
}
