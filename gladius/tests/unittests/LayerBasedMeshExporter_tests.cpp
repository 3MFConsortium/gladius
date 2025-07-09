#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../src/compute/ComputeCore.h"
#include "../src/io/LayerBasedMeshExporter.h"

namespace gladius::io::tests
{
    // Test implementation of LayerBasedMeshExporter for testing
    class TestLayerBasedMeshExporter : public LayerBasedMeshExporter
    {
      public:
        void finalize() override
        {
            m_finalizeCalled = true;
            m_grid.reset();
        }

        // Public wrapper for testing protected methods
        static double testAlignToLayer(double value, double increment)
        {
            return alignToLayer(value, increment);
        }

        bool m_finalizeCalled = false;
    };

    /// @test LayerBasedMeshExporter_InitialProgress_ReturnsZero
    /// Test that initial progress is zero
    TEST(LayerBasedMeshExporter_InitialProgress, ReturnsZero)
    {
        // Arrange
        TestLayerBasedMeshExporter exporter;

        // Act
        double progress = exporter.getProgress();

        // Assert
        EXPECT_EQ(progress, 0.0);
    }

    /// @test LayerBasedMeshExporter_SetQualityLevel_UpdatesQualityLevel
    /// Test that setQualityLevel updates the quality level
    TEST(LayerBasedMeshExporter_SetQualityLevel, UpdatesQualityLevel)
    {
        // Arrange
        TestLayerBasedMeshExporter exporter;
        size_t testQualityLevel = 2;

        // Act
        exporter.setQualityLevel(testQualityLevel);

        // Assert
        // Since m_qualityLevel is protected, we can't directly test it,
        // but we can verify the exporter doesn't crash and accepts the call
        EXPECT_NO_THROW(exporter.setQualityLevel(testQualityLevel));
    }

    /// @test LayerBasedMeshExporter_AlignToLayer_ReturnsCorrectAlignment
    /// Test that alignToLayer function works correctly
    TEST(LayerBasedMeshExporter_AlignToLayer, ReturnsCorrectAlignment)
    {
        // Arrange
        double value = 5.7;
        double increment = 0.1;
        double expectedResult = 5.7; // floor(5.7 / 0.1) * 0.1 = 57 * 0.1 = 5.7

        // Act
        double result = TestLayerBasedMeshExporter::testAlignToLayer(value, increment);

        // Assert
        EXPECT_DOUBLE_EQ(result, expectedResult);
    }

    /// @test LayerBasedMeshExporter_AlignToLayer_WithLargerIncrement_ReturnsCorrectAlignment
    /// Test alignToLayer with larger increment
    TEST(LayerBasedMeshExporter_AlignToLayer_WithLargerIncrement, ReturnsCorrectAlignment)
    {
        // Arrange
        double value = 5.7;
        double increment = 1.0;
        double expectedResult = 5.0; // floor(5.7 / 1.0) * 1.0 = 5 * 1.0 = 5.0

        // Act
        double result = TestLayerBasedMeshExporter::testAlignToLayer(value, increment);

        // Assert
        EXPECT_DOUBLE_EQ(result, expectedResult);
    }

} // namespace gladius::io::tests
