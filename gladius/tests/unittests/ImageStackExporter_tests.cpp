#include "io/ImageStackExporter.h"
#include "io/vdb.h"
#include <filesystem>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <typeindex>

// Mock classes for filesystem and OpenVDB grid
class MockFileSystem
{
  public:
    MOCK_METHOD(bool, exists, (const std::filesystem::path &), (const));
    MOCK_METHOD(void, create_directories, (const std::filesystem::path &) );
};

class MockFloatGrid : public openvdb::FloatGrid
{
  public:
    using Ptr = std::shared_ptr<MockFloatGrid>;
    MOCK_METHOD(openvdb::GridClass, getGridClass, (), (const override));

    MOCK_METHOD(openvdb::math::Transform::Ptr, transformPtr, (), (const));

    template <typename T>
    bool isType() const
    {
        return isTypeMock(typeid(T));
    }

    MOCK_METHOD(bool, isTypeMock, (std::type_index), (const));
    // Add more mocked methods as needed for the test
};

class ImageStackExporterTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Setup code for each test
    }

    void TearDown() override
    {
        // Cleanup code for each test
    }

    MockFileSystem mockFileSystem;
    MockFloatGrid::Ptr mockGrid = std::make_shared<MockFloatGrid>();
};

bool operator==(Lib3MF::sPosition const & lhs, Lib3MF::sPosition const & rhs)
{
    return lhs.m_Coordinates[0] == rhs.m_Coordinates[0] &&
           lhs.m_Coordinates[1] == rhs.m_Coordinates[1] &&
           lhs.m_Coordinates[2] == rhs.m_Coordinates[2];
}

TEST(ImageStackExporterTest, AddBoundingBoxAsMesh)
{
    using namespace gladius::io;
    using namespace gladius;

    // Create a bounding box
    BoundingBox bb;
    bb.min = {0.0, 0.0, 0.0};
    bb.max = {1.0, 2.0, 3.0};

    // create a model
    auto model = Lib3MF::CWrapper::loadLibrary()->CreateModel();
    auto mesh = addBoundingBoxAsMesh(model, bb);

    // Check if the mesh has the correct number of vertices and triangles
    ASSERT_EQ(mesh->GetVertexCount(), 8);
    ASSERT_EQ(mesh->GetTriangleCount(), 12);

    // Check if the mesh has the correct name
    ASSERT_EQ(mesh->GetName(), "Bounding Box");

    // Check if the vertices are correct
    auto v0 = mesh->GetVertex(0);
    auto v1 = mesh->GetVertex(1);
    auto v2 = mesh->GetVertex(2);
    auto v3 = mesh->GetVertex(3);
    auto v4 = mesh->GetVertex(4);
    auto v5 = mesh->GetVertex(5);
    auto v6 = mesh->GetVertex(6);
    auto v7 = mesh->GetVertex(7);

    ASSERT_TRUE(v0 == Lib3MF::sPosition({0.0, 0.0, 0.0}));
    ASSERT_TRUE(v1 == Lib3MF::sPosition({1.0, 0.0, 0.0}));
    ASSERT_TRUE(v2 == Lib3MF::sPosition({1.0, 2.0, 0.0}));
    ASSERT_TRUE(v3 == Lib3MF::sPosition({0.0, 2.0, 0.0}));

    ASSERT_TRUE(v4 == Lib3MF::sPosition({0.0, 0.0, 3.0}));
    ASSERT_TRUE(v5 == Lib3MF::sPosition({1.0, 0.0, 3.0}));
    ASSERT_TRUE(v6 == Lib3MF::sPosition({1.0, 2.0, 3.0}));
    ASSERT_TRUE(v7 == Lib3MF::sPosition({0.0, 2.0, 3.0}));

    // Check if the triangles are correct
    ASSERT_EQ(mesh->GetTriangle(0).m_Indices[0], 0);
    ASSERT_EQ(mesh->GetTriangle(0).m_Indices[1], 2);
    ASSERT_EQ(mesh->GetTriangle(0).m_Indices[2], 1);

    ASSERT_EQ(mesh->GetTriangle(1).m_Indices[0], 0);
    ASSERT_EQ(mesh->GetTriangle(1).m_Indices[1], 3);
    ASSERT_EQ(mesh->GetTriangle(1).m_Indices[2], 2);

    ASSERT_EQ(mesh->GetTriangle(2).m_Indices[0], 4);
    ASSERT_EQ(mesh->GetTriangle(2).m_Indices[1], 5);
    ASSERT_EQ(mesh->GetTriangle(2).m_Indices[2], 6);

    ASSERT_EQ(mesh->GetTriangle(3).m_Indices[0], 4);
    ASSERT_EQ(mesh->GetTriangle(3).m_Indices[1], 6);
    ASSERT_EQ(mesh->GetTriangle(3).m_Indices[2], 7);

    ASSERT_EQ(mesh->GetTriangle(4).m_Indices[0], 0);
    ASSERT_EQ(mesh->GetTriangle(4).m_Indices[1], 5);
    ASSERT_EQ(mesh->GetTriangle(4).m_Indices[2], 4);

    ASSERT_EQ(mesh->GetTriangle(5).m_Indices[0], 0);
    ASSERT_EQ(mesh->GetTriangle(5).m_Indices[1], 1);
    ASSERT_EQ(mesh->GetTriangle(5).m_Indices[2], 5);

    ASSERT_EQ(mesh->GetTriangle(6).m_Indices[0], 3);
    ASSERT_EQ(mesh->GetTriangle(6).m_Indices[1], 6);
    ASSERT_EQ(mesh->GetTriangle(6).m_Indices[2], 2);

    ASSERT_EQ(mesh->GetTriangle(7).m_Indices[0], 3);
    ASSERT_EQ(mesh->GetTriangle(7).m_Indices[1], 7);
    ASSERT_EQ(mesh->GetTriangle(7).m_Indices[2], 6);

    ASSERT_EQ(mesh->GetTriangle(8).m_Indices[0], 0);
    ASSERT_EQ(mesh->GetTriangle(8).m_Indices[1], 7);
    ASSERT_EQ(mesh->GetTriangle(8).m_Indices[2], 3);

    ASSERT_EQ(mesh->GetTriangle(9).m_Indices[0], 0);
    ASSERT_EQ(mesh->GetTriangle(9).m_Indices[1], 4);
    ASSERT_EQ(mesh->GetTriangle(9).m_Indices[2], 7);

    ASSERT_EQ(mesh->GetTriangle(10).m_Indices[0], 1);
    ASSERT_EQ(mesh->GetTriangle(10).m_Indices[1], 6);
    ASSERT_EQ(mesh->GetTriangle(10).m_Indices[2], 5);

    ASSERT_EQ(mesh->GetTriangle(11).m_Indices[0], 1);
    ASSERT_EQ(mesh->GetTriangle(11).m_Indices[1], 2);
    ASSERT_EQ(mesh->GetTriangle(11).m_Indices[2], 6);

    // Check if the mesh has the correct number of vertices and triangles
    ASSERT_EQ(mesh->GetVertexCount(), 8);
    ASSERT_EQ(mesh->GetTriangleCount(), 12);
}

// Additional tests can be added here