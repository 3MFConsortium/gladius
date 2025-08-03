#include "EventLogger.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <thread>

namespace gladius::events::tests
{
    /// @brief Test fixture for EventLogger tests
    class EventLoggerTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            // Create a fresh logger for each test
            m_logger = std::make_shared<Logger>();
        }

        void TearDown() override
        {
            // Clean up log files after each test
            if (m_logger && m_logger->isFileLoggingEnabled())
            {
                auto logPath = m_logger->getLogFilePath();
                if (std::filesystem::exists(logPath))
                {
                    std::filesystem::remove(logPath);
                }

                // Also try to remove the log directory if it's empty
                auto logDir = logPath.parent_path();
                try
                {
                    if (std::filesystem::exists(logDir) && std::filesystem::is_empty(logDir))
                    {
                        std::filesystem::remove(logDir);

                        // Try to remove parent gladius directory if empty too
                        auto gladiusDir = logDir.parent_path();
                        if (std::filesystem::exists(gladiusDir) &&
                            std::filesystem::is_empty(gladiusDir))
                        {
                            std::filesystem::remove(gladiusDir);
                        }
                    }
                }
                catch (...)
                {
                    // Ignore cleanup errors
                }
            }
        }

        std::shared_ptr<Logger> m_logger;
    };

    /// @test EventLogger_Initialize_CreatesLogFile
    /// Test that initialization creates a log file
    TEST_F(EventLoggerTest, Initialize_CreatesLogFile)
    {
        // Arrange & Act
        m_logger->initialize();

        // Assert
        EXPECT_TRUE(m_logger->isFileLoggingEnabled());
        auto logPath = m_logger->getLogFilePath();
        EXPECT_FALSE(logPath.empty());
        EXPECT_TRUE(logPath.filename().string().starts_with("gladius_"));
        EXPECT_EQ(logPath.extension(), ".log");
    }

    /// @test EventLogger_LogInfo_AddsEventToMemoryAndFile
    /// Test that logging info messages works correctly
    TEST_F(EventLoggerTest, LogInfo_AddsEventToMemoryAndFile)
    {
        // Arrange
        m_logger->initialize();
        std::string const testMessage = "Test info message";

        // Act
        m_logger->logInfo(testMessage);

        // Assert - Check in-memory storage
        EXPECT_EQ(m_logger->size(), 2); // 1 for initialization + 1 for our message
        EXPECT_EQ(m_logger->getErrorCount(), 0);
        EXPECT_EQ(m_logger->getWarningCount(), 0);

        // Find our test message
        bool foundMessage = false;
        for (auto const & event : *m_logger)
        {
            if (event.getMessage() == testMessage)
            {
                EXPECT_EQ(event.getSeverity(), Severity::Info);
                foundMessage = true;
                break;
            }
        }
        EXPECT_TRUE(foundMessage);
    }

    /// @test EventLogger_LogError_IncrementsErrorCount
    /// Test that error logging increments error count
    TEST_F(EventLoggerTest, LogError_IncrementsErrorCount)
    {
        // Arrange
        m_logger->initialize();

        // Act
        m_logger->logError("Test error");

        // Assert
        EXPECT_EQ(m_logger->getErrorCount(), 1);
        EXPECT_EQ(m_logger->getWarningCount(), 0);
    }

    /// @test EventLogger_LogWarning_IncrementsWarningCount
    /// Test that warning logging increments warning count
    TEST_F(EventLoggerTest, LogWarning_IncrementsWarningCount)
    {
        // Arrange
        m_logger->initialize();

        // Act
        m_logger->logWarning("Test warning");

        // Assert
        EXPECT_EQ(m_logger->getErrorCount(), 0);
        EXPECT_EQ(m_logger->getWarningCount(), 1);
    }

    /// @test EventLogger_LogFatalError_IncrementsErrorCount
    /// Test that fatal error logging increments error count
    TEST_F(EventLoggerTest, LogFatalError_IncrementsErrorCount)
    {
        // Arrange
        m_logger->initialize();

        // Act
        m_logger->logFatalError("Test fatal error");

        // Assert
        EXPECT_EQ(m_logger->getErrorCount(), 1);
        EXPECT_EQ(m_logger->getWarningCount(), 0);
    }

    /// @test EventLogger_Clear_ResetsCountsAndEvents
    /// Test that clear() resets all counters and events
    TEST_F(EventLoggerTest, Clear_ResetsCountsAndEvents)
    {
        // Arrange
        m_logger->initialize();
        m_logger->logError("Test error");
        m_logger->logWarning("Test warning");

        // Act
        m_logger->clear();

        // Assert
        EXPECT_EQ(m_logger->size(), 0);
        EXPECT_EQ(m_logger->getErrorCount(), 0);
        EXPECT_EQ(m_logger->getWarningCount(), 0);
    }

    /// @test EventLogger_SetFileLoggingEnabled_ControlsFileLogging
    /// Test that file logging can be enabled/disabled
    TEST_F(EventLoggerTest, SetFileLoggingEnabled_ControlsFileLogging)
    {
        // Arrange & Act - Disable file logging
        m_logger->setFileLoggingEnabled(false);

        // Assert
        EXPECT_FALSE(m_logger->isFileLoggingEnabled());
        EXPECT_TRUE(m_logger->getLogFilePath().empty());

        // Act - Re-enable file logging
        m_logger->setFileLoggingEnabled(true);

        // Assert
        EXPECT_TRUE(m_logger->isFileLoggingEnabled());
        EXPECT_FALSE(m_logger->getLogFilePath().empty());
    }

    /// @test EventLogger_OutputMode_ControlsConsoleOutput
    /// Test that output mode controls console output behavior
    TEST_F(EventLoggerTest, OutputMode_ControlsConsoleOutput)
    {
        // Arrange
        m_logger->initialize();

        // Act & Assert - Test Console mode
        m_logger->setOutputMode(OutputMode::Console);
        EXPECT_EQ(m_logger->getOutputMode(), OutputMode::Console);

        // Act & Assert - Test Silent mode
        m_logger->setOutputMode(OutputMode::Silent);
        EXPECT_EQ(m_logger->getOutputMode(), OutputMode::Silent);
    }

    /// @test EventLogger_FileExists_AfterMultipleLogs
    /// Test that log file contains entries after multiple log calls
    TEST_F(EventLoggerTest, FileExists_AfterMultipleLogs)
    {
        // Arrange
        m_logger->initialize();
        auto logPath = m_logger->getLogFilePath();

        // Act - Log multiple messages to trigger file write
        for (int i = 0; i < 15; ++i) // More than the 10-message threshold
        {
            m_logger->logInfo("Test message " + std::to_string(i));
        }

        // Flush any pending file operations
        m_logger->flush();

        // Assert - Check that log file exists and has content
        EXPECT_TRUE(std::filesystem::exists(logPath));

        std::ifstream logFile(logPath);
        EXPECT_TRUE(logFile.is_open());

        std::string line;
        bool hasContent = false;
        while (std::getline(logFile, line))
        {
            if (!line.empty())
            {
                hasContent = true;
                // Check log format: should contain timestamp, severity, and message
                EXPECT_TRUE(line.find("[INFO]") != std::string::npos ||
                            line.find("[WARN]") != std::string::npos ||
                            line.find("[ERROR]") != std::string::npos ||
                            line.find("[FATAL]") != std::string::npos);
                break;
            }
        }
        EXPECT_TRUE(hasContent);
    }

    /// @test EventLogger_ConstructorWithOutputMode_SetsMode
    /// Test that constructor with OutputMode parameter sets the mode correctly
    TEST_F(EventLoggerTest, ConstructorWithOutputMode_SetsMode)
    {
        // Arrange & Act
        auto silentLogger = std::make_shared<Logger>(OutputMode::Silent);

        // Assert
        EXPECT_EQ(silentLogger->getOutputMode(), OutputMode::Silent);
    }

    /// @test EventLogger_LogDirectory_UsesCorrectPath
    /// Test that log directory is created in correct location
    TEST_F(EventLoggerTest, LogDirectory_UsesCorrectPath)
    {
        // Arrange & Act
        m_logger->initialize();
        auto logPath = m_logger->getLogFilePath();

        // Assert
        auto expectedParent = std::filesystem::temp_directory_path() / "gladius" / "logs";
        EXPECT_EQ(logPath.parent_path(), expectedParent);
        EXPECT_TRUE(std::filesystem::exists(expectedParent));
    }

    /// @test EventLogger_Destructor_FlushesAllPendingEvents
    /// Test that destructor writes all pending events to file
    TEST_F(EventLoggerTest, Destructor_FlushesAllPendingEvents)
    {
        // Arrange
        std::filesystem::path logPath;
        {
            auto scopedLogger = std::make_shared<Logger>();
            scopedLogger->initialize();
            logPath = scopedLogger->getLogFilePath();

            // Act - Add events but don't flush (should be batched)
            for (int i = 0; i < 5; ++i) // Less than 10 to avoid auto-flush
            {
                scopedLogger->logInfo("Pending message " + std::to_string(i));
            }

            // Don't call flush() - destructor should handle it
        } // Logger goes out of scope here, destructor should flush pending events

        // Assert - Check that log file contains the events
        EXPECT_TRUE(std::filesystem::exists(logPath));

        std::ifstream logFile(logPath);
        EXPECT_TRUE(logFile.is_open());

        std::vector<std::string> logLines;
        std::string line;
        while (std::getline(logFile, line))
        {
            if (!line.empty())
            {
                logLines.push_back(line);
            }
        }

        // Should have at least 5 lines (the pending messages) plus initialization message
        EXPECT_GE(logLines.size(), 5);

        // Check that our pending messages are present
        int foundPendingMessages = 0;
        for (const auto & logLine : logLines)
        {
            if (logLine.find("Pending message") != std::string::npos)
            {
                foundPendingMessages++;
            }
        }
        EXPECT_EQ(foundPendingMessages, 5);
    }
}
