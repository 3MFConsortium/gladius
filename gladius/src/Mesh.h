#pragma once

#include "Buffer.h"
#include "types.h"

#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/src/Core/Matrix.h>

#include <array>
#include <vector>

namespace gladius
{

    struct Face
    {
        Vector3 normal;
        std::array<Vector3, 3> vertices;
        std::array<Vector3, 3> vertexNormals;
    };

    using Faces = std::vector<Face>;

    class Mesh
    {
      public:
        explicit Mesh(ComputeContext & context)
            : m_vertices(context)
            , m_faceNormals(context)
            , m_vertexNormals(context)
        {
        }

        /**
         * \brief
         * \return Number of faces
         */
        size_t getNumberOfFaces() const
        {
            return m_vertices.getSize() / 3;
        }

        size_t getNumberOfVertices() const
        {
            return m_vertices.getSize();
        }

        Face getFace(size_t index)
        {
            Face face;
            auto normal = m_faceNormals.getData()[index];
            face.normal = {normal.x, normal.y, normal.z};

            for (int i = 0; i < 3; ++i)
            {
                const auto vertex = m_vertices.getData()[index * 3 + i];
                face.vertices[i] = {vertex.x, vertex.y, vertex.z};

                const auto vertexNormal = m_vertexNormals.getData()[index * 3 + i];
                face.vertexNormals[i] = {vertexNormal.x, vertexNormal.y, vertexNormal.z};
            }
            return face;
        }

        void addFace(const Face & face)
        {
            m_faceNormals.getData().push_back(
              cl_float4{{face.normal.x(), face.normal.y(), face.normal.z(), 0.f}});

            for (int i = 0; i < 3; ++i)
            {
                const auto vertex = face.vertices[i];
                m_vertices.getData().push_back({{vertex.x(), vertex.y(), vertex.z(), 0.f}});

                const auto vertexNormal = face.vertexNormals[i];
                m_vertexNormals.getData().push_back(
                  {{vertexNormal.x(), vertexNormal.y(), vertexNormal.z(), 0.f}});
            }
        }

        void addFace(Vector3 const & a, Vector3 const & b, Vector3 const & c)
        {
            // Calculate face normal using manual cross product to avoid Eigen linking issues
            auto edge1 = b - a;
            auto edge2 = c - a;

            // Manual cross product: edge1 × edge2
            Vector3 faceNormal;
            faceNormal[0] = edge1[1] * edge2[2] - edge1[2] * edge2[1];
            faceNormal[1] = edge1[2] * edge2[0] - edge1[0] * edge2[2];
            faceNormal[2] = edge1[0] * edge2[1] - edge1[1] * edge2[0];
            faceNormal.normalize();

            Face face{faceNormal, {a, b, c}, {faceNormal, faceNormal, faceNormal}};
            addFace(face);
        }

        void write()
        {
            m_vertices.write();
            m_vertexNormals.write();
        }

        void read()
        {
            m_vertices.read();
            m_vertexNormals.read();
        }

        [[nodiscard]] const Buffer<cl_float4> & getFaceNormals() const;
        [[nodiscard]] const Buffer<cl_float4> & getVertices() const;
        [[nodiscard]] const Buffer<cl_float4> & getVertexNormals() const;
        [[nodiscard]] Buffer<cl_float4> & getVertices();

      private:
        Buffer<cl_float4> m_vertices;
        Buffer<cl_float4> m_faceNormals;
        Buffer<cl_float4> m_vertexNormals;
    };

    inline const Buffer<cl_float4> & Mesh::getFaceNormals() const
    {
        return m_faceNormals;
    }

    inline const Buffer<cl_float4> & Mesh::getVertices() const
    {
        return m_vertices;
    }

    inline Buffer<cl_float4> & Mesh::getVertices()
    {
        return m_vertices;
    }

    inline const Buffer<cl_float4> & Mesh::getVertexNormals() const
    {
        return m_vertexNormals;
    }

    using SharedMesh = std::shared_ptr<Mesh>;
}
