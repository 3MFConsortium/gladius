#include "testdata.h"
#include "testhelper.h"

#include <gladius_dynamic.hpp>

#include <array>
#include <filesystem>
#include <iostream>
#include <optional>

#include <gtest/gtest.h>

namespace gladius_integration_tests
{
    struct TestParameter
    {
        std::string filename;
        float z_mm;
        float expectedNumberOfContours;
        float expected_area_mm2;
    };

    class Slicer_test : public ::testing::TestWithParam<TestParameter>
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
                std::filesystem::current_path(originalWorkingDirectory);

                m_gladius = m_wrapper->CreateGladius();
            }
            catch (std::exception & e)
            {
                std::cout << e.what() << std::endl;
                std::filesystem::current_path(originalWorkingDirectory);
            }
        }

        void TearDown() override
        {
            m_gladius.reset();
            m_wrapper.reset();
            Test::TearDown();
        }

        [[nodiscard]] GladiusLib::PWrapper getWrapper() const
        {
            return m_wrapper;
        }

        GladiusLib::PGladius getGladius()
        {
            if (!m_gladius)
            {
                throw std::runtime_error("gladius has to be instantiated first");
            }
            return m_gladius;
        }

      private:
        GladiusLib::PWrapper m_wrapper;
        GladiusLib::PGladius m_gladius;
    };

    TEST_P(Slicer_test, GenerateContour_SpecifiedZHeight_AreaAndNumberOfContoursMatchExpectation)
    {
        auto const testParameter = GetParam();
        auto const gladius = getGladius();

        gladius->LoadAssembly(testParameter.filename);
        auto const contourAccessor = getGladius()->GenerateContour(testParameter.z_mm, 0.f);

        EXPECT_EQ(contourAccessor->GetSize(), testParameter.expectedNumberOfContours);

        float areaSum = 0.f;
        do
        {
            areaSum += contourAccessor->GetCurrentPolygon()->GetArea();
        } while (contourAccessor->Next());
        auto constexpr tolerance = 2.E-1f;
        EXPECT_NEAR(areaSum,
                    testParameter.expected_area_mm2,
                    tolerance); // NOLINT(clang-diagnostic-double-promotion)
    }

    std::array<TestParameter, 1> implicitGyroidParameter{
      // filename, z_mm, expected number of contours, expected area
      {{FileNames::SimpleGyroid, 5.f, 3u, 4.966f}},
    };

    INSTANTIATE_TEST_SUITE_P(ImplicitGyroid,
                             Slicer_test,
                             ::testing::ValuesIn(implicitGyroidParameter));
}