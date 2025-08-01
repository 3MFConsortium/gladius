#include "Application.h"
#include <filesystem>
#include <iostream>
#include <string>

using namespace std;

void printUsage()
{
    std::cout << "Usage: gladius [options] [file]\n";
    std::cout << "Options:\n";
    std::cout << "  --mcp-server [port]  Enable MCP server (default port: 8080)\n";
    std::cout << "  --help              Show this help message\n";
    std::cout << "Examples:\n";
    std::cout << "  gladius                           # Start with welcome screen\n";
    std::cout << "  gladius model.3mf                # Open specific file\n";
    std::cout << "  gladius --mcp-server              # Start with MCP server on port 8080\n";
    std::cout << "  gladius --mcp-server 8081         # Start with MCP server on port 8081\n";
}

int main(int argc, char ** argv)
{
    bool enableMCP = false;
    int mcpPort = 8080;
    std::optional<std::filesystem::path> filename;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--mcp-server")
        {
            enableMCP = true;
            // Check if next argument is a port number
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                try
                {
                    mcpPort = std::stoi(argv[++i]);
                    if (mcpPort <= 0 || mcpPort > 65535)
                    {
                        std::cerr << "Invalid port number: " << mcpPort << std::endl;
                        return 1;
                    }
                }
                catch (const std::exception &)
                {
                    std::cerr << "Invalid port number: " << argv[i] << std::endl;
                    return 1;
                }
            }
        }
        else if (arg == "--help")
        {
            printUsage();
            return 0;
        }
        else if (!arg.starts_with("--"))
        {
            filename = std::filesystem::path(arg);
        }
        else
        {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage();
            return 1;
        }
    }

    // Set working directory
    std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());

    // Create application based on arguments
    gladius::Application app;

    // Enable MCP server if requested (before starting main loop)
    if (enableMCP)
    {
        if (app.enableMCPServer(mcpPort))
        {
            std::cout << "MCP Server enabled on port " << mcpPort << std::endl;
        }
        else
        {
            std::cerr << "Failed to enable MCP Server on port " << mcpPort << std::endl;
            return 1;
        }
    }

    // Open file if specified and exists
    if (filename)
    {
        if (std::filesystem::exists(*filename))
        {
            std::cout << "Opening file: " << *filename << std::endl;
            app.getMainWindow().open(*filename);
        }
        else
        {
            std::cout << "File does not exist: " << *filename << std::endl;
        }
    }

    // Start the main loop (this will block until the application exits)
    app.startMainLoop();

    // Clean up MCP server before exit
    if (enableMCP)
    {
        app.disableMCPServer();
    }

    return 0;
}
