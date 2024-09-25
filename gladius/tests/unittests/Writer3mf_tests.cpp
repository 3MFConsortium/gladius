
#include "io/3mf/Writer3mf.h"
#include "nodes/types.h"
#include "nodes/utils.h"

#include <gmock/gmock.h>

namespace gladius_tests
{

    using namespace gladius;

    TEST(Writer3mfTest, ConvertMatrix4x4)
    {
        // Test case 1: Identity matrix
        gladius::nodes::Matrix4x4 identityMat = identityMatrix();
        
        Lib3MF::sMatrix4x4 convertedMatrix = io::convertMatrix4x4(identityMat);
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                if (row == col)
                    EXPECT_EQ(convertedMatrix.m_Field[row][col], 1.0);
                else
                    EXPECT_EQ(convertedMatrix.m_Field[row][col], 0.0);
            }
        }

        // Test case 2: Arbitrary matrix
        gladius::nodes::Matrix4x4 arbitraryMatrix = zeroMatrix();
        
        arbitraryMatrix[0] [0] = 1.0;
        arbitraryMatrix[1] [1] = 2.0;
        arbitraryMatrix[2] [2] = 3.0;
        arbitraryMatrix[3] [3] = 4.0;
        
        convertedMatrix = io::convertMatrix4x4(arbitraryMatrix);
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                if (row == col)
                    EXPECT_EQ(convertedMatrix.m_Field[row][col], arbitraryMatrix[row][col]);
                else
                    EXPECT_EQ(convertedMatrix.m_Field[row][col], 0.0);
            }
        }
    }
} // namespace gladius_tests