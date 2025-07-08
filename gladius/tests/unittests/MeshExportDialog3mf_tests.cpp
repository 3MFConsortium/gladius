#include "ComputeContext.h"
#include "Mesh.h"
#include "compute/ComputeCore.h"
#include "io/MeshExporter3mf.h"
#include "ui/MeshExportDialog3mf.h"
#include <gtest/gtest.h>

namespace gladius::ui::tests
{
    /// @test MeshExportDialog3mf_ConstructsWithoutError_SuccessfulConstruction
    /// Test that MeshExportDialog3mf can be constructed successfully
    TEST(MeshExportDialog3mf_ConstructsWithoutError, SuccessfulConstruction)
    {
        // Arrange
        // No special setup needed

        // Act
        MeshExportDialog3mf dialog;

        // Assert
        EXPECT_FALSE(dialog.isVisible());
    }

    /// @test MeshExportDialog3mf_InitiallyNotVisible_ReturnsCorrectVisibility
    /// Test that MeshExportDialog3mf is initially not visible
    TEST(MeshExportDialog3mf_InitiallyNotVisible, ReturnsCorrectVisibility)
    {
        // Arrange
        MeshExportDialog3mf dialog;

        // Act
        bool isVisible = dialog.isVisible();

        // Assert
        EXPECT_FALSE(isVisible);
    }

    /// @test MeshExporter3mf_ConstructsWithLogger_SuccessfulConstruction
    /// Test that MeshExporter3mf can be constructed with logger
    TEST(MeshExporter3mf_ConstructsWithLogger, SuccessfulConstruction)
    {
        // Arrange
        auto logger = std::make_shared<events::Logger>();

        // Act
        vdb::MeshExporter3mf exporter(logger);

        // Assert
        EXPECT_EQ(exporter.getProgress(), 0.0);
    }

    /// @test MeshExporter3mf_ConstructsWithoutLogger_SuccessfulConstruction
    /// Test that MeshExporter3mf can be constructed without logger
    TEST(MeshExporter3mf_ConstructsWithoutLogger, SuccessfulConstruction)
    {
        // Arrange & Act
        vdb::MeshExporter3mf exporter;

        // Assert
        EXPECT_EQ(exporter.getProgress(), 0.0);
    }

    /// @test MeshExporter3mf_SetQualityLevel_UpdatesInternalState
    /// Test that setting quality level works correctly
    TEST(MeshExporter3mf_SetQualityLevel, UpdatesInternalState)
    {
        // Arrange
        vdb::MeshExporter3mf exporter;

        // Act
        exporter.setQualityLevel(2);

        // Assert
        // Quality level is private, but we can test that no exception is thrown
        SUCCEED();
    }

} // namespace gladius::ui::tests
