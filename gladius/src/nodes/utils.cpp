
#include "utils.h"
#include "types.h"

#include <algorithm>
#include <eigen3/Eigen/Eigen>
#include <eigen3/Eigen/src/Core/Matrix.h>

namespace gladius
{
    std::string toLowerCase(std::string text)
    {
        std::transform(text.begin(),
                       text.end(),
                       text.begin(),
                       [](unsigned char c) { return std::tolower(static_cast<int>(c)); });
        return text;
    }

    nodes::Matrix4x4 zeroMatrix()
    {
        nodes::Matrix4x4 matrix;
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                matrix[row][col] = 0.0f;
            }
        }
        return matrix;
    }

    nodes::Matrix4x4 identityMatrix()
    {
        nodes::Matrix4x4 matrix = zeroMatrix();
        for (int i = 0; i < 4; ++i)
        {
            matrix[i][i] = 1.0f;
        }
        return matrix;
    }

    nodes::Matrix4x4 inverseMatrix(const nodes::Matrix4x4 & matrix)
    {
        Eigen::Matrix4f eigenMatrix;
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                eigenMatrix(row, col) = matrix[row][col];
            }
        }

        Eigen::Matrix4f inverted = eigenMatrix.inverse();

        nodes::Matrix4x4 result;
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                result[row][col] = inverted(row, col);
            }
        }

        return result;
    }
}
