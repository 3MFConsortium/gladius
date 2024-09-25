#pragma once

#include "GLImageBuffer.h"
#include "ImageRGBA.h"
#include "Primitives.h"
#include "ProgramBase.h"
#include "ResourceContext.h"

namespace gladius
{
    class Mesh;

    class SlicerProgram : public ProgramBase
    {
      public:
        explicit SlicerProgram(SharedComputeContext context, const SharedResources & resources);

        void readBuffer() const;

        void renderFirstLayer(const Primitives & lines, cl_float isoValue, cl_float z_mm);
        void renderLayers(const Primitives & lines, cl_float isoValue, cl_float z_mm);
        void renderResultImageReadPixel(DistanceMap & sourceImage, GLImageBuffer & targetImage);
        void precomputeSdf(const Primitives & lines, BoundingBox boundingBox);

        void calculateNormals(const Primitives & lines, const Mesh & mesh);

        void
        renderDownSkinDistance(DepthBuffer & targetImage, Primitives const & lines, cl_float z_mm);

        void
        renderUpSkinDistance(DepthBuffer & targetImage, Primitives const & lines, cl_float z_mm);

        /**
         * \brief Moves the given points to the surface, useful for determining a convex hull or
         * determine the bounding box
         */
        void
        movePointsToSurface(Primitives const & lines, VertexBuffer & input, VertexBuffer & output);


        /**
         * \brief Adopts the vertex positions of the given mesh to the surface
         */
        void
        adoptVertexOfMeshToSurface(Primitives const & lines, VertexBuffer & input, VertexBuffer & output);
        

        void computeMarchingSquareState(const Primitives & lines, cl_float z_mm);

        void adoptVertexPositions2d(const Primitives & lines,
                                    Vertex2dBuffer & input,
                                    Vertex2dBuffer & output,
                                    cl_float z_mm);

        void setKernelReplacements(SharedKernelReplacements replacements);

      private:
        [[nodiscard]] cl_float determineBranchThreshold(const cl_int2 & res,
                                                        cl_float isoValue) const;

        std::mutex m_queueMutex;
    };
}
