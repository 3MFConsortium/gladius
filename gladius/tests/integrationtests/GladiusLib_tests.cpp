
#include "testdata.h"
#include "testhelper.h"

#include <gladius_dynamic.hpp>

#include <array>
#include <cmath>
#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>

#include "BBox.h"

#define EXPECT_NEAR_F(A, B, TOL)                                                                   \
    EXPECT_NEAR(static_cast<double>(A), static_cast<double>(B), static_cast<double>(TOL))

namespace gladius_integration_tests
{
    class GladiusLib_test : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            Test::SetUp();
            auto const originalWorkingDirectory = std::filesystem::current_path();
            try
            {
                auto const gladiusSharedLibPath = findGladiusSharedLib();
                if (!gladiusSharedLibPath.has_value())
                {
                    throw std::runtime_error(
                      "Could not find directory containing  gladius shared library or dll");
                }

                std::filesystem::current_path(gladiusSharedLibPath.value().parent_path());
                m_wrapper =
                  GladiusLib::CWrapper::loadLibrary(gladiusSharedLibPath.value().string());
            }
            catch (std::exception & e)
            {
                std::cout << e.what() << std::endl;
            }
            std::filesystem::current_path(originalWorkingDirectory);
        }

        [[nodiscard]] GladiusLib::PWrapper getWrapper() const
        {
            return m_wrapper;
        }

        [[nodiscard]] size_t numDownSkinPixel(std::string const & filename) const
        {
            auto constexpr tolerance = 1.E-3f;
            auto constexpr z_mm = 5.f;
            auto constexpr expectedDistance = z_mm;

            auto const gladius = getWrapper()->CreateGladius();
            EXPECT_TRUE(gladius);

            gladius->LoadAssembly(filename);

            const auto channelAccessor = gladius->GetChannels();
            EXPECT_TRUE(channelAccessor->SwitchToChannel("DownSkin"));
            channelAccessor->Evaluate(z_mm, 0.1f, 0.1f);

            auto const numElements =
              channelAccessor->GetMetaInfo().m_Size[0] * channelAccessor->GetMetaInfo().m_Size[1];
            std::vector<float> downSkinBuffer;
            downSkinBuffer.resize(numElements);

            channelAccessor->Copy(reinterpret_cast<GladiusLib_int64>(downSkinBuffer.data()));

            auto const numElementsWithExpectedDistance = std::count_if(
              downSkinBuffer.begin(),
              downSkinBuffer.end(),
              [](auto const value) { return fabs(value - expectedDistance) < tolerance; });

            auto path = std::filesystem::path(filename);
            auto const outputPath = std::filesystem::absolute(path.replace_extension("png"));

            saveBitmapLayer(outputPath,
                            downSkinBuffer,
                            channelAccessor->GetMetaInfo().m_Size[0],
                            channelAccessor->GetMetaInfo().m_Size[1]);

            return numElementsWithExpectedDistance;
        }

      private:
        GladiusLib::PWrapper m_wrapper;
    };

    TEST_F(GladiusLib_test, GladiusWrapper_CreateGladius_InstanceCreated)
    {
        EXPECT_NO_THROW({
            auto const gladius = getWrapper()->CreateGladius();
            EXPECT_TRUE(gladius);
        });
    }

    TEST_F(GladiusLib_test, GetVersion_NoInput_ReturnsLibVersion)
    {
        unsigned int major, minor, micro;
        getWrapper()->GetVersion(major, minor, micro);
        EXPECT_EQ(major, 1u);
        EXPECT_EQ(minor, 2u);
        EXPECT_EQ(micro, 0u);
    }

    TEST_F(GladiusLib_test, GladiusWrapper_GeneratePreviewMesh_NoException)
    {

        auto gladius = getWrapper()->CreateGladius();
        ASSERT_TRUE(gladius);

        gladius->LoadAssembly(FileNames::SimpleGyroid);

        auto faceIterator = gladius->GeneratePreviewMesh();
        EXPECT_TRUE(faceIterator);
        EXPECT_GT(faceIterator->GetSize(), 0u);
    }

    float length(GladiusLib::sVector3f const & vector)
    {
        return sqrtf(vector.m_Coordinates[0] * vector.m_Coordinates[0] +
                     vector.m_Coordinates[1] * vector.m_Coordinates[1] +
                     vector.m_Coordinates[2] * vector.m_Coordinates[2]);
    }

    /// Check DetailedErrorAccessor after loading a non-existing file
    TEST_F(GladiusLib_test,
           DetailedErrorAccessor_LoadNonExistingFile_DetailedErrorAccessorContainsError)
    {
        auto const gladius = getWrapper()->CreateGladius();
        EXPECT_TRUE(gladius);

        gladius->LoadAssembly("NonExistingFile");

        auto const detailedErrorAccessor = gladius->GetDetailedErrorAccessor();
        EXPECT_GT(detailedErrorAccessor->GetSize(), 0u);
    }

    /// Loop over all DetailedErrorAccessor entries and check if they are valid using next() in a
    /// do-while loop
    TEST_F(GladiusLib_test,
           DetailedErrorAccessor_LoadNonExistingFile_DetailedErrorAccessorContainsValidEntries)
    {
        auto const gladius = getWrapper()->CreateGladius();
        EXPECT_TRUE(gladius);

        gladius->LoadAssembly("NonExistingFile");

        auto const detailedErrorAccessor = gladius->GetDetailedErrorAccessor();
        EXPECT_GT(detailedErrorAccessor->GetSize(), 0u);

        do
        {
            EXPECT_TRUE(detailedErrorAccessor);
            EXPECT_NO_THROW(std::cout << "severity:" << detailedErrorAccessor->GetSeverity()
                                      << " msg:" << detailedErrorAccessor->GetDescription()
                                      << std::endl;);
        } while (detailedErrorAccessor->Next());
    }

    TEST_F(GladiusLib_test, DetailedErrorAccessor_ClearDetailedErrors_DetailedErrorAccessorIsEmpty)
    {
        auto const gladius = getWrapper()->CreateGladius();
        EXPECT_TRUE(gladius);

        gladius->LoadAssembly("NonExistingFile");

        auto const detailedErrorAccessor = gladius->GetDetailedErrorAccessor();
        EXPECT_GT(detailedErrorAccessor->GetSize(), 0u);

        gladius->ClearDetailedErrors();

        EXPECT_EQ(detailedErrorAccessor->GetSize(), 0u);
    }

    /// Test VariableVoronoi file loading and bounding box computation
    TEST_F(GladiusLib_test, VariableVoronoi_LoadAssembly_BoundingBoxIsValid)
    {
        auto const gladius = getWrapper()->CreateGladius();
        EXPECT_TRUE(gladius);

        // Load the variable voronoi 3MF file
        gladius->LoadAssembly(FileNames::VariableVoronoi);

        // Compute the bounding box
        auto boundingBox = gladius->ComputeBoundingBox();
        EXPECT_TRUE(boundingBox);

        // Get min and max points
        auto minPoint = boundingBox->GetMin();
        auto maxPoint = boundingBox->GetMax();

        // Verify that bounding box values are finite (no NaN or inf)
        EXPECT_TRUE(std::isfinite(minPoint.m_Coordinates[0]))
          << "Min X coordinate should be finite";
        EXPECT_TRUE(std::isfinite(minPoint.m_Coordinates[1]))
          << "Min Y coordinate should be finite";
        EXPECT_TRUE(std::isfinite(minPoint.m_Coordinates[2]))
          << "Min Z coordinate should be finite";

        EXPECT_TRUE(std::isfinite(maxPoint.m_Coordinates[0]))
          << "Max X coordinate should be finite";
        EXPECT_TRUE(std::isfinite(maxPoint.m_Coordinates[1]))
          << "Max Y coordinate should be finite";
        EXPECT_TRUE(std::isfinite(maxPoint.m_Coordinates[2]))
          << "Max Z coordinate should be finite";

        // Verify that min <= max for all coordinates
        EXPECT_LE(minPoint.m_Coordinates[0], maxPoint.m_Coordinates[0])
          << "Min X should be <= Max X";
        EXPECT_LE(minPoint.m_Coordinates[1], maxPoint.m_Coordinates[1])
          << "Min Y should be <= Max Y";
        EXPECT_LE(minPoint.m_Coordinates[2], maxPoint.m_Coordinates[2])
          << "Min Z should be <= Max Z";

        // Print bounding box for debugging purposes
        std::cout << "Bounding box - Min: (" << minPoint.m_Coordinates[0] << ", "
                  << minPoint.m_Coordinates[1] << ", " << minPoint.m_Coordinates[2] << ")"
                  << std::endl;
        std::cout << "Bounding box - Max: (" << maxPoint.m_Coordinates[0] << ", "
                  << maxPoint.m_Coordinates[1] << ", " << maxPoint.m_Coordinates[2] << ")"
                  << std::endl;
    }

    /// Test Implicit3mf file loading and bounding box computation
    TEST_F(GladiusLib_test, Implicit3mf_LoadAssembly_BoundingBoxIsValid)
    {
        auto const gladius = getWrapper()->CreateGladius();
        EXPECT_TRUE(gladius);

        // Load the implicit 3MF file
        gladius->LoadAssembly(FileNames::Implicit3mf);

        // Compute the bounding box
        auto boundingBox = gladius->ComputeBoundingBox();
        EXPECT_TRUE(boundingBox);

        // Get min and max points
        auto minPoint = boundingBox->GetMin();
        auto maxPoint = boundingBox->GetMax();

        // Verify that bounding box values are finite (no NaN or inf)
        EXPECT_TRUE(std::isfinite(minPoint.m_Coordinates[0]))
          << "Min X coordinate should be finite";
        EXPECT_TRUE(std::isfinite(minPoint.m_Coordinates[1]))
          << "Min Y coordinate should be finite";
        EXPECT_TRUE(std::isfinite(minPoint.m_Coordinates[2]))
          << "Min Z coordinate should be finite";

        EXPECT_TRUE(std::isfinite(maxPoint.m_Coordinates[0]))
          << "Max X coordinate should be finite";
        EXPECT_TRUE(std::isfinite(maxPoint.m_Coordinates[1]))
          << "Max Y coordinate should be finite";
        EXPECT_TRUE(std::isfinite(maxPoint.m_Coordinates[2]))
          << "Max Z coordinate should be finite";

        // Verify that min <= max for all coordinates
        EXPECT_LE(minPoint.m_Coordinates[0], maxPoint.m_Coordinates[0])
          << "Min X should be <= Max X";
        EXPECT_LE(minPoint.m_Coordinates[1], maxPoint.m_Coordinates[1])
          << "Min Y should be <= Max Y";
        EXPECT_LE(minPoint.m_Coordinates[2], maxPoint.m_Coordinates[2])
          << "Min Z should be <= Max Z";

        // Print bounding box for debugging purposes
        std::cout << "Bounding box - Min: (" << minPoint.m_Coordinates[0] << ", "
                  << minPoint.m_Coordinates[1] << ", " << minPoint.m_Coordinates[2] << ")"
                  << std::endl;
        std::cout << "Bounding box - Max: (" << maxPoint.m_Coordinates[0] << ", "
                  << maxPoint.m_Coordinates[1] << ", " << maxPoint.m_Coordinates[2] << ")"
                  << std::endl;
    }
}