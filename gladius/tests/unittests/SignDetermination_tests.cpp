
#include "CLMath.h"
#include "testhelper.h"

#include <Eigen/Core>
#include <Eigen/src/Core/Matrix.h>
#include <cmath>

#include <functional>
#include <gtest/gtest.h>
#include <ios>
#include <iostream>
#include <limits>
#include <random>
#include <utility>

namespace gladius_tests
{
    namespace testee
    {
        auto determineBoundaryCrossings(const float3 & pos,
                                        const float3 & direction,
                                        ShapeFunction & model) -> int
        {
            auto currentpos = pos;
            float distToSurface = fabs(model(currentpos));

            auto const precision = std::clamp(distToSurface, 1.E-9f, 1.E-2f);
            auto constexpr outsideDistance = 1.E3f;
            int numberCrossings = 0;
            bool atBoundary = false;
            auto numSteps = 0;
            int constexpr maxSteps = 1000;

            while (distToSurface < outsideDistance && (currentpos - pos).norm() < outsideDistance &&
                   numSteps < maxSteps)
            {
                numSteps++;
                if (atBoundary && distToSurface > precision)
                {
                    atBoundary = false;
                    numberCrossings++;
                }

                if (distToSurface < precision && !atBoundary)
                {
                    atBoundary = true;
                }

                float stepSize = std::max(distToSurface, precision);
                currentpos += stepSize * direction;
                distToSurface = fabs(model(currentpos));
            }
            return numberCrossings;
        }

        auto determineSign(const float3 & pos, ShapeFunction & model) -> float
        {
            auto const distToSurface = fabs(model(pos));
            if (fabs(distToSurface) < std::numeric_limits<float>::epsilon())
            {
                return 0.f;
            }

            float3 const directions[] = {{1.f, 0.f, 0.f},
                                         {-1.f, 0.f, 0.f},
                                         {0.f, 1.f, 0.f},
                                         {0.f, -1.f, 0.f},
                                         {0.f, 0.f, 1.f},
                                         {0.f, 0.f, -1.f},
                                         {1.f, 1.f, 0.f},
                                         {-1.f, -1.f, 0.f},
                                         {0.f, 1.f, 1.f},
                                         {0.f, -1.f, -1.f}};

            auto positiveSigns = 0;
            auto negativeSigns = 0;
            for (const auto & direction : directions)
            {
                auto const numCrossings = determineBoundaryCrossings(pos, direction, model);
                bool const even = numCrossings % 2 == 0;
                if (even)
                {
                    positiveSigns++;
                }
                else
                {
                    negativeSigns++;
                }
                if (abs(negativeSigns - positiveSigns) > 1)
                {
                    break;
                }
            }
            // std::cout << "positiveSigns = " << positiveSigns << "\n";
            // std::cout << "negativeSigns = " << negativeSigns << "\n";
            auto const sig = (positiveSigns >= negativeSigns) ? 1.f : -1.f;
            return sig;
        }
    }

    class SignDetermination : public ::testing::Test
    {
        void SetUp() override
        {
            std::random_device randomDevice;
            std::mt19937 randomEngine(randomDevice());
            std::uniform_real_distribution<float> distribution(-12.f, 12.f);

            auto constexpr numTestPositions = 100;
            m_testPositions.reserve(numTestPositions);

            for (auto i = 0; i < numTestPositions; ++i)
            {
                m_testPositions.emplace_back(distribution(randomEngine),
                                             distribution(randomEngine),
                                             distribution(randomEngine));
            }

            m_testPositions.emplace_back(-1.16356385e+00f, -4.90426826e+00f, 3.97338480e-01f);

            m_testPositions.emplace_back(-4.29429579e+00f, -1.86590481e+00f, -9.82463074e+00f);
            m_testPositions.emplace_back(-8.83533001e+00f,
                                         2.39840698e+00f,
                                         -4.02300787e+00f); // this point is exactly on the boundary

            m_testPositions.emplace_back(7.70643425e+00f, 2.43984103e+00f, -5.88710976e+00f);
        }

      protected:
        std::vector<float3> m_testPositions;
    };

    TEST_F(SignDetermination, DetermineSign_Sphere_SignIsSameAsFromSourceSdf)
    {
        ShapeFunction mediumSizedSphere = [](float3 pos) {
            return helper::sphere(std::move(pos), 10.f);
        };
        std::cout.precision(8);
        std::cout << std::scientific;
        int i = 0;
        int numFailed = 0;
        for (auto const & pos : m_testPositions)
        {

            float const modelSdf = mediumSizedSphere(pos);
            // std::cout << "modelSdf = " << modelSdf << "\n"; // without using modelsdf or the
            // unnessary "+0.f" the lambda mediumSizedSphere will be inlined by the compiler
            // (clang version 9.0.1-14) instead. As it turns out, the sign of a lambda is zero.

            bool sameSign =
              testee::determineSign(pos, mediumSizedSphere) == gladius::sign<float>(modelSdf + 0.f);
            EXPECT_TRUE(sameSign);
            EXPECT_EQ(testee::determineSign(pos, mediumSizedSphere),
                      gladius::sign<float>(modelSdf + 0.f));
            i++;
            if (!sameSign)
            {
                std::cout << "Test " << i << " of " << m_testPositions.size()
                          << " with pos=" << pos.transpose() << std::endl;
                ++numFailed;
            }
        }

        if (numFailed > 0)
        {
            std::cout << numFailed << " of " << m_testPositions.size()
                      << " evaluations resulted in the wrong sign\n";
        }
    }
}