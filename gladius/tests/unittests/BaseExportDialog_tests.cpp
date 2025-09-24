#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../src/io/IExporter.h"
#include "../src/io/LayerBasedMeshExporter.h"
#include "../src/ui/BaseExportDialog.h"

namespace gladius::ui::tests
{
    // Mock exporter for testing
    class MockExporter : public io::IExporter
    {
      public:
        MOCK_METHOD(void,
                    beginExport,
                    (std::filesystem::path const & fileName, ComputeCore & generator),
                    (override));
        MOCK_METHOD(bool, advanceExport, (ComputeCore & generator), (override));
        MOCK_METHOD(void, finalize, (), (override));
        MOCK_METHOD(double, getProgress, (), (const, override));
    };

    // Test implementation of BaseExportDialog
    class TestExportDialog : public BaseExportDialog
    {
      public:
        TestExportDialog() = default;

        void beginExport(std::filesystem::path const & filename, ComputeCore & core) override
        {
            m_visible = true;
            m_mockExporter.beginExport(filename, core);
        }

        // Public accessors for testing
        void setVisible(bool visible)
        {
            m_visible = visible;
        }
        std::string getTestWindowTitle() const
        {
            return getWindowTitle();
        }
        std::string getTestExportMessage() const
        {
            return getExportMessage();
        }

      protected:
        [[nodiscard]] std::string getWindowTitle() const override
        {
            return "Test Export Dialog";
        }

        [[nodiscard]] std::string getExportMessage() const override
        {
            return "Testing export...";
        }

        [[nodiscard]] io::IExporter & getExporter() override
        {
            return m_mockExporter;
        }

      public:
        MockExporter m_mockExporter;
    };

    /// @test BaseExportDialog_InitiallyNotVisible_ReturnsCorrectVisibility
    /// Test that BaseExportDialog is initially not visible
    TEST(BaseExportDialog_InitiallyNotVisible, ReturnsCorrectVisibility)
    {
        // Arrange
        TestExportDialog dialog;

        // Act
        bool isVisible = dialog.isVisible();

        // Assert
        EXPECT_FALSE(isVisible);
    }

    /// @test BaseExportDialog_AfterHide_BecomesNotVisible
    /// Test that calling hide() makes dialog not visible
    TEST(BaseExportDialog_AfterHide, BecomesNotVisible)
    {
        // Arrange
        TestExportDialog dialog;
        dialog.setVisible(true); // Make it visible first

        // Act
        dialog.hide();

        // Assert
        EXPECT_FALSE(dialog.isVisible());
    }

    /// @test BaseExportDialog_GetWindowTitle_ReturnsCorrectTitle
    /// Test that getWindowTitle returns the expected title
    TEST(BaseExportDialog_GetWindowTitle, ReturnsCorrectTitle)
    {
        // Arrange
        TestExportDialog dialog;

        // Act
        std::string title = dialog.getTestWindowTitle();

        // Assert
        EXPECT_EQ(title, "Test Export Dialog");
    }

    /// @test BaseExportDialog_GetExportMessage_ReturnsCorrectMessage
    /// Test that getExportMessage returns the expected message
    TEST(BaseExportDialog_GetExportMessage, ReturnsCorrectMessage)
    {
        // Arrange
        TestExportDialog dialog;

        // Act
        std::string message = dialog.getTestExportMessage();

        // Assert
        EXPECT_EQ(message, "Testing export...");
    }

} // namespace gladius::ui::tests
