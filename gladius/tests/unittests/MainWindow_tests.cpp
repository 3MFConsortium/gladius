/**
 * @file MainWindow_tests.cpp
 * @brief Unit tests for MainWindow save before file operation functionality
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <filesystem>
#include <optional>

namespace gladius::ui::tests
{
    /**
     * @brief Enum for tracking pending file operations
     * This mirrors the enum defined in MainWindow.h
     */
    enum class PendingFileOperation
    {
        None,
        NewModel,
        OpenFile
    };

    /**
     * @brief Simple test class that mimics MainWindow's save dialog logic
     * This allows us to test the core functionality without UI dependencies
     */
    class SaveBeforeFileOperationLogic
    {
      public:
        SaveBeforeFileOperationLogic() = default;

        void setFileChanged(bool changed) { m_fileChanged = changed; }
        bool getFileChanged() const { return m_fileChanged; }

        void setCurrentAssemblyFileName(const std::optional<std::filesystem::path>& filename) 
        { 
            m_currentAssemblyFileName = filename; 
        }
        
        std::optional<std::filesystem::path> getCurrentAssemblyFileName() const 
        { 
            return m_currentAssemblyFileName; 
        }

        bool shouldShowSaveDialog() const { return m_showSaveBeforeFileOperation; }
        PendingFileOperation getPendingOperation() const { return m_pendingFileOperation; }
        std::optional<std::filesystem::path> getPendingFilename() const { return m_pendingOpenFilename; }

        /// Simulates the newModel() method logic
        bool tryNewModel()
        {
            if (m_fileChanged)
            {
                m_pendingFileOperation = PendingFileOperation::NewModel;
                m_pendingOpenFilename.reset();
                m_showSaveBeforeFileOperation = true;
                return false; // Operation pending
            }
            
            // Would proceed with new model creation
            return true; // Operation completed
        }

        /// Simulates the open() method logic (file dialog)
        bool tryOpen()
        {
            if (m_fileChanged)
            {
                m_pendingFileOperation = PendingFileOperation::OpenFile;
                m_pendingOpenFilename.reset();
                m_showSaveBeforeFileOperation = true;
                return false; // Operation pending
            }
            
            // Would show file dialog and proceed
            return true; // Operation completed
        }

        /// Simulates the open(filename) method logic
        bool tryOpen(const std::filesystem::path& filename)
        {
            if (m_fileChanged)
            {
                m_pendingFileOperation = PendingFileOperation::OpenFile;
                m_pendingOpenFilename = filename;
                m_showSaveBeforeFileOperation = true;
                return false; // Operation pending
            }
            
            // Would open the file directly
            m_currentAssemblyFileName = filename;
            return true; // Operation completed
        }

        /// Simulates completing the pending operation after save
        void completePendingOperation()
        {
            if (m_pendingFileOperation == PendingFileOperation::NewModel)
            {
                // Would create new model
                m_currentAssemblyFileName.reset();
            }
            else if (m_pendingFileOperation == PendingFileOperation::OpenFile)
            {
                if (m_pendingOpenFilename.has_value())
                {
                    // Would open specific file
                    m_currentAssemblyFileName = m_pendingOpenFilename.value();
                }
                else
                {
                    // Would show file dialog - for testing, just clear current file
                    m_currentAssemblyFileName.reset();
                }
            }
            
            resetPendingState();
        }

        /// Simulates cancelling the pending operation
        void cancelPendingOperation()
        {
            resetPendingState();
        }

      private:
        void resetPendingState()
        {
            m_showSaveBeforeFileOperation = false;
            m_pendingFileOperation = PendingFileOperation::None;
            m_pendingOpenFilename.reset();
        }

        bool m_fileChanged{false};
        bool m_showSaveBeforeFileOperation{false};
        PendingFileOperation m_pendingFileOperation{PendingFileOperation::None};
        std::optional<std::filesystem::path> m_pendingOpenFilename;
        std::optional<std::filesystem::path> m_currentAssemblyFileName;
    };

    class MainWindowSaveBeforeFileOperation_Test : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            m_logic = std::make_unique<SaveBeforeFileOperationLogic>();
        }

        void TearDown() override
        {
            m_logic.reset();
        }

        std::unique_ptr<SaveBeforeFileOperationLogic> m_logic;
    };

    /**
     * @brief Test that newModel triggers save dialog when file has changed
     * Expected behavior: When file has unsaved changes and user creates new model,
     * save dialog should be shown and pending operation should be set
     */
    TEST_F(MainWindowSaveBeforeFileOperation_Test, NewModel_WithUnsavedChanges_ShowsSaveDialog)
    {
        // Arrange
        m_logic->setFileChanged(true);
        m_logic->setCurrentAssemblyFileName("/test/path/test.3mf");

        // Act
        bool operationCompleted = m_logic->tryNewModel();

        // Assert
        EXPECT_FALSE(operationCompleted);
        EXPECT_TRUE(m_logic->shouldShowSaveDialog());
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::NewModel);
        EXPECT_FALSE(m_logic->getPendingFilename().has_value());
    }

    /**
     * @brief Test that newModel proceeds directly when file has no changes
     * Expected behavior: When file has no unsaved changes, new model should be created directly
     */
    TEST_F(MainWindowSaveBeforeFileOperation_Test, NewModel_WithoutUnsavedChanges_ProceedsDirectly)
    {
        // Arrange
        m_logic->setFileChanged(false);

        // Act
        bool operationCompleted = m_logic->tryNewModel();

        // Assert
        EXPECT_TRUE(operationCompleted);
        EXPECT_FALSE(m_logic->shouldShowSaveDialog());
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::None);
    }

    /**
     * @brief Test that open() triggers save dialog when file has changed
     * Expected behavior: When file has unsaved changes and user opens file dialog,
     * save dialog should be shown and pending operation should be set
     */
    TEST_F(MainWindowSaveBeforeFileOperation_Test, Open_WithUnsavedChanges_ShowsSaveDialog)
    {
        // Arrange
        m_logic->setFileChanged(true);
        m_logic->setCurrentAssemblyFileName("/test/path/current.3mf");

        // Act
        bool operationCompleted = m_logic->tryOpen();

        // Assert
        EXPECT_FALSE(operationCompleted);
        EXPECT_TRUE(m_logic->shouldShowSaveDialog());
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::OpenFile);
        EXPECT_FALSE(m_logic->getPendingFilename().has_value());
    }

    /**
     * @brief Test that open(filename) triggers save dialog when file has changed
     * Expected behavior: When file has unsaved changes and user opens specific file,
     * save dialog should be shown with the target filename stored
     */
    TEST_F(MainWindowSaveBeforeFileOperation_Test, OpenWithFilename_WithUnsavedChanges_ShowsSaveDialog)
    {
        // Arrange
        m_logic->setFileChanged(true);
        m_logic->setCurrentAssemblyFileName("/test/path/current.3mf");
        const std::filesystem::path targetFile = "/test/path/target.3mf";

        // Act
        bool operationCompleted = m_logic->tryOpen(targetFile);

        // Assert
        EXPECT_FALSE(operationCompleted);
        EXPECT_TRUE(m_logic->shouldShowSaveDialog());
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::OpenFile);
        EXPECT_TRUE(m_logic->getPendingFilename().has_value());
        EXPECT_EQ(m_logic->getPendingFilename().value(), targetFile);
    }

    /**
     * @brief Test that open(filename) proceeds directly when file has no changes
     * Expected behavior: When file has no unsaved changes, file should be opened directly
     */
    TEST_F(MainWindowSaveBeforeFileOperation_Test, OpenWithFilename_WithoutUnsavedChanges_ProceedsDirectly)
    {
        // Arrange
        m_logic->setFileChanged(false);
        const std::filesystem::path targetFile = "/test/path/target.3mf";

        // Act
        bool operationCompleted = m_logic->tryOpen(targetFile);

        // Assert
        EXPECT_TRUE(operationCompleted);
        EXPECT_FALSE(m_logic->shouldShowSaveDialog());
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::None);
        EXPECT_EQ(m_logic->getCurrentAssemblyFileName().value(), targetFile);
    }

    /**
     * @brief Test PendingFileOperation enum values
     * Expected behavior: Enum should have correct values for type safety
     */
    TEST(MainWindowSaveBeforeFileOperation_EnumTest, PendingFileOperation_HasCorrectValues)
    {
        // Assert enum values are distinct
        EXPECT_NE(PendingFileOperation::None, PendingFileOperation::NewModel);
        EXPECT_NE(PendingFileOperation::None, PendingFileOperation::OpenFile);
        EXPECT_NE(PendingFileOperation::NewModel, PendingFileOperation::OpenFile);

        // Assert enum can be used in comparisons
        PendingFileOperation operation = PendingFileOperation::NewModel;
        EXPECT_EQ(operation, PendingFileOperation::NewModel);
        EXPECT_NE(operation, PendingFileOperation::OpenFile);
    }

    /**
     * @brief Test completing pending operation after save
     * Expected behavior: When save dialog is completed by saving,
     * pending operation should be executed and state reset
     */
    TEST_F(MainWindowSaveBeforeFileOperation_Test, CompletePendingOperation_ExecutesAndResets)
    {
        // Arrange - set up a pending open operation
        m_logic->setFileChanged(true);
        const std::filesystem::path targetFile = "/test/path/target.3mf";
        m_logic->tryOpen(targetFile);

        // Verify initial state
        EXPECT_TRUE(m_logic->shouldShowSaveDialog());
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::OpenFile);
        EXPECT_TRUE(m_logic->getPendingFilename().has_value());

        // Act - complete the operation (simulates saving and continuing)
        m_logic->completePendingOperation();

        // Assert - operation completed and state reset
        EXPECT_FALSE(m_logic->shouldShowSaveDialog());
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::None);
        EXPECT_FALSE(m_logic->getPendingFilename().has_value());
        EXPECT_EQ(m_logic->getCurrentAssemblyFileName().value(), targetFile);
    }

    /**
     * @brief Test cancelling pending operation
     * Expected behavior: When save dialog is cancelled,
     * pending operation should be discarded and state reset
     */
    TEST_F(MainWindowSaveBeforeFileOperation_Test, CancelPendingOperation_DiscardsAndResets)
    {
        // Arrange - set up a pending new model operation
        m_logic->setFileChanged(true);
        m_logic->setCurrentAssemblyFileName("/test/path/current.3mf");
        m_logic->tryNewModel();

        // Verify initial state
        EXPECT_TRUE(m_logic->shouldShowSaveDialog());
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::NewModel);

        auto const originalFilename = m_logic->getCurrentAssemblyFileName();

        // Act - cancel the operation
        m_logic->cancelPendingOperation();

        // Assert - operation cancelled and state reset, original file unchanged
        EXPECT_FALSE(m_logic->shouldShowSaveDialog());
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::None);
        EXPECT_FALSE(m_logic->getPendingFilename().has_value());
        EXPECT_EQ(m_logic->getCurrentAssemblyFileName(), originalFilename);
    }

    /**
     * @brief Test edge case with empty filename
     * Expected behavior: System should handle empty/invalid filenames gracefully
     */
    TEST_F(MainWindowSaveBeforeFileOperation_Test, OpenWithEmptyFilename_HandlesGracefully)
    {
        // Arrange
        m_logic->setFileChanged(true);
        const std::filesystem::path emptyFile = "";

        // Act
        bool operationCompleted = m_logic->tryOpen(emptyFile);

        // Assert - should still trigger save dialog
        EXPECT_FALSE(operationCompleted);
        EXPECT_TRUE(m_logic->shouldShowSaveDialog());
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::OpenFile);
        EXPECT_TRUE(m_logic->getPendingFilename().has_value());
        EXPECT_EQ(m_logic->getPendingFilename().value(), emptyFile);
    }

    /**
     * @brief Test multiple consecutive operations
     * Expected behavior: Only the last operation should be pending
     */
    TEST_F(MainWindowSaveBeforeFileOperation_Test, MultipleOperations_LastOperationTakesPrecedence)
    {
        // Arrange
        m_logic->setFileChanged(true);
        
        // Act - perform multiple operations
        m_logic->tryNewModel();
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::NewModel);
        
        const std::filesystem::path targetFile = "/test/path/target.3mf";
        m_logic->tryOpen(targetFile);
        
        // Assert - last operation should take precedence
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::OpenFile);
        EXPECT_TRUE(m_logic->getPendingFilename().has_value());
        EXPECT_EQ(m_logic->getPendingFilename().value(), targetFile);
    }

    /**
     * @brief Test with no current assembly filename
     * Expected behavior: System should handle case where no file is currently open
     */
    TEST_F(MainWindowSaveBeforeFileOperation_Test, NoCurrentFile_StillShowsSaveDialog)
    {
        // Arrange
        m_logic->setFileChanged(true);
        // Don't set a current assembly filename (simulates new/unsaved document)

        // Act
        bool operationCompleted = m_logic->tryNewModel();

        // Assert
        EXPECT_FALSE(operationCompleted);
        EXPECT_TRUE(m_logic->shouldShowSaveDialog());
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::NewModel);
        EXPECT_FALSE(m_logic->getCurrentAssemblyFileName().has_value());
    }

    /**
     * @brief Test completing new model operation
     * Expected behavior: After saving and continuing with new model,
     * current assembly filename should be cleared
     */
    TEST_F(MainWindowSaveBeforeFileOperation_Test, CompleteNewModelOperation_ClearsCurrentFile)
    {
        // Arrange
        m_logic->setFileChanged(true);
        m_logic->setCurrentAssemblyFileName("/test/path/current.3mf");
        m_logic->tryNewModel();

        // Act
        m_logic->completePendingOperation();

        // Assert
        EXPECT_FALSE(m_logic->getCurrentAssemblyFileName().has_value());
        EXPECT_FALSE(m_logic->shouldShowSaveDialog());
        EXPECT_EQ(m_logic->getPendingOperation(), PendingFileOperation::None);
    }
}
