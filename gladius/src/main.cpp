#include "Application.h"
#include <atomic>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>

using namespace std;

void printUsage()
{
    std::cout << "Usage: gladius [options] [file]\n";
    std::cout << "Options:\n";
    std::cout
      << "  --mcp-server [port]  Enable MCP server with HTTP transport (default port: 8080)\n";
    std::cout << "  --mcp-stdio          Enable MCP server with stdio transport (for VS Code)\n";
    std::cout << "  --headless          Run without starting the UI (headless mode)\n";
    std::cout << "  --help              Show this help message\n";
    std::cout << "Examples:\n";
    std::cout << "  gladius                           # Start with welcome screen\n";
    std::cout << "  gladius model.3mf                # Open specific file\n";
    std::cout << "  gladius --mcp-server              # Start with MCP server on port 8080\n";
    std::cout << "  gladius --mcp-server 8081         # Start with MCP server on port 8081\n";
    std::cout
      << "  gladius --mcp-stdio               # Start with MCP server using stdio (VS Code mode)\n";
}

int main(int argc, char ** argv)
{
    bool enableMCP = false;
    bool mcpStdio = false;
    int mcpPort = 8080;
    bool headless = false;
    std::optional<std::filesystem::path> filename;

    // Graceful termination flag for headless MCP mode
    static std::atomic<bool> terminateRequested{false};
    auto signalHandler = [](int) { terminateRequested.store(true); };
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Parse command line arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--mcp-server")
        {
            enableMCP = true;
            mcpStdio = false;
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
        else if (arg == "--mcp-stdio")
        {
            enableMCP = true;
            mcpStdio = true;
        }
        else if (arg == "--headless")
        {
            headless = true;
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
    gladius::Application app(headless);

    // Enable MCP server if requested (before starting main loop)
    if (enableMCP)
    {
        bool success;
        if (mcpStdio)
        {
            // In stdio mode, redirect stdout to stderr to avoid interfering with JSON-RPC
            // Save original stdout for restoration later
            std::streambuf * orig_cout = std::cout.rdbuf();
            std::cout.rdbuf(std::cerr.rdbuf());

            // Set logger to silent mode for stdio transport to avoid interfering with JSON-RPC
            app.setLoggerOutputMode(gladius::events::OutputMode::Silent);

            success = app.enableMCPServerStdio();

            // Restore stdout for MCP protocol
            std::cout.rdbuf(orig_cout);

            if (!success)
            {
                // Use stderr for error since stdout is reserved for MCP protocol
                std::cerr << "Failed to enable MCP Server with stdio transport" << std::endl;
                return 1;
            }
            // In stdio mode, we start the main loop which will handle stdio communication
        }
        else
        {
            success = app.enableMCPServer(mcpPort);
            if (success)
            {
                auto logger = app.getGlobalLogger();
                if (logger)
                {
                    logger->logInfo("MCP Server enabled on port " + std::to_string(mcpPort));
                }
            }
            else
            {
                std::cerr << "Failed to enable MCP Server on port " << mcpPort << std::endl;
                return 1;
            }
        }
    }

    // Open file if specified and exists
    if (filename)
    {
        if (std::filesystem::exists(*filename))
        {
            if (!mcpStdio)
            {
                auto logger = app.getGlobalLogger();
                if (logger)
                {
                    logger->logInfo("Opening file: " + filename->string());
                }
            }
            app.getMainWindow().open(*filename);
        }
        else
        {
            if (!mcpStdio)
            {
                auto logger = app.getGlobalLogger();
                if (logger)
                {
                    logger->logError("File does not exist: " + filename->string());
                }
            }
        }
    }

    // UI/main loop and headless behavior
    if (!headless)
    {
        // Normal mode: run UI loop (blocks until exit)
        app.startMainLoop();

        // Clean up MCP server before exit
        if (enableMCP)
        {
            app.disableMCPServer();
        }
    }
    else
    {
        // Headless mode: keep process alive while MCP server is running.
        // For stdio transport, do not print to stdout (reserved for protocol).
        while (!terminateRequested.load())
        {
            // If MCP was not enabled, nothing to serve – break immediately
            if (!enableMCP)
            {
                break;
            }
            // If server has stopped externally, exit the loop
            if (!app.isMCPServerEnabled())
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        // Clean up MCP server before exit (HTTP or stdio)
        if (enableMCP && app.isMCPServerEnabled())
        {
            app.disableMCPServer();
        }
    }

    return 0;
}
