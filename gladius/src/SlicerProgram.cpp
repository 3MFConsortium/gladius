#include "SlicerProgram.h"
#include "gpgpu.h"
#include "ProgramBase.h"

#include <algorithm>
#include <utility>

#include "Mesh.h"
#include "Profiling.h"

namespace gladius
{

#define PAYLOAD_ARGUMENTS                                                                          \
    m_resoures->getBuildArea(), lines.primitives.getBuffer(),                                      \
      static_cast<cl_uint>(lines.primitives.getSize()), lines.data.getBuffer(),                    \
      static_cast<cl_uint>(lines.data.getSize()), m_resoures->getRenderingSettings(),              \
      m_resoures->getPrecompSdfBuffer().getBuffer(), m_resoures->getParameterBuffer().getBuffer(), \
      m_resoures->getCommandBuffer().getBuffer(),                                                  \
      static_cast<cl_int>(m_resoures->getCommandBuffer().getData().size()),                        \
      m_resoures->getPreCompSdfBBox()

#define PAYLOAD_ARGUMENTS_CLIPPING_AREA                                                            \
    m_resoures->getClippingArea(), lines.primitives.getBuffer(),                                   \
      static_cast<cl_uint>(lines.primitives.getSize()), lines.data.getBuffer(),                    \
      static_cast<cl_uint>(lines.data.getSize()), m_resoures->getRenderingSettings(),              \
      m_resoures->getPrecompSdfBuffer().getBuffer(), m_resoures->getParameterBuffer().getBuffer(), \
      m_resoures->getCommandBuffer().getBuffer(),                                                  \
      static_cast<cl_int>(m_resoures->getCommandBuffer().getData().size()),                        \
      m_resoures->getPreCompSdfBBox()

    gladius::SlicerProgram::SlicerProgram(SharedComputeContext context,
                                          const SharedResources & resources)
        : ProgramBase(context, resources)
    {
        m_sourceFilesProgram = {"arguments.h",
                                "types.h",
                                "sdf.h",
                                "sampler.h",
                                "rendering.h",
                                "sdf_generator.h",
                                "CNanoVDB.h",
                                "sdf.cl",
                                "rendering.cl",
                                "distanceUpDown.cl",
                                "sdf_generator.cl"};

        m_sourceFilesLib = {"arguments.h",
                            "types.h",
                            "CNanoVDB.h",
                            "sdf.h",
                            "sdf_generator.h",
                            "sampler.h",
                            "compensator.cl",
                            "distanceUpDown.cl"};

    }


    void SlicerProgram::readBuffer() const
    {
        
        m_resoures->getContourVertexPos().read();
    }

    void SlicerProgram::renderFirstLayer(const Primitives & lines, cl_float isoValue, cl_float z_mm)
    {
        ProfileFunction;
        swapProgramsIfNeeded();
        const auto res = m_resoures->getMipMapResolutions().front();
        const cl_float branchThreshold = determineBranchThreshold(res, isoValue);

        const cl::NDRange origin = {0, 0, 0};
        const cl::NDRange globalRange = {m_resoures->getDistanceMipMaps().front()->getWidth(),
                                         m_resoures->getDistanceMipMaps().front()->getHeight(),
                                         1u};

        m_programFront->run("renderSDFFirstLayer",
                            origin,
                            globalRange,
                            m_resoures->getDistanceMipMaps().front()->getBuffer(), // 0
                            branchThreshold,                                       // 1
                            PAYLOAD_ARGUMENTS_CLIPPING_AREA,
                            z_mm); // 12
    }

    void SlicerProgram::renderLayers(const Primitives & lines, cl_float isoValue, cl_float z_mm)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        ProfileFunction;
        m_resoures->getRenderingSettings().approximation = AM_HYBRID;
        swapProgramsIfNeeded();
        renderFirstLayer(lines, isoValue, z_mm);

        auto resIt = std::begin(m_resoures->getMipMapResolutions());
        auto distMapIt = std::begin(m_resoures->getDistanceMipMaps());

        for (++resIt, ++distMapIt; resIt != std::end(m_resoures->getMipMapResolutions()) &&
                                   distMapIt != std::end(m_resoures->getDistanceMipMaps());
             ++resIt, ++distMapIt)
        {
            const auto res = *resIt;
            const cl_float branchThreshold = determineBranchThreshold(res, isoValue);

            const auto previousLayer = (distMapIt - 1)->get();
            const cl::NDRange origin = {0, 0, 0};
            const cl::NDRange globalRange = {
              (*distMapIt)->getWidth(), (*distMapIt)->getHeight(), 1u};

            m_programFront->run("renderSDFLayer",
                                origin,
                                globalRange,
                                distMapIt->get()->getBuffer(), // 0
                                previousLayer->getBuffer(),    // 1
                                branchThreshold,               // 2
                                PAYLOAD_ARGUMENTS_CLIPPING_AREA,
                                z_mm); // 13
        }

        auto & lastLayer = *m_resoures->getDistanceMipMaps().back();
        lastLayer.read();
    }

    cl_float SlicerProgram::determineBranchThreshold(const cl_int2 & res, cl_float isoValue) const
    {
        ProfileFunction;
        const auto buildAreaWidth =
          m_resoures->getClippingArea().z - m_resoures->getClippingArea().x;
        const auto buildAreaHeight =
          m_resoures->getClippingArea().w - m_resoures->getClippingArea().y;
        const auto maxSize = std::max(buildAreaWidth / static_cast<float>(res.x),
                                      buildAreaHeight / static_cast<float>(res.y));
        const auto maxSizeGrid =
          std::max(buildAreaWidth / static_cast<float>(m_resoures->getGridSize().x),
                   buildAreaHeight / static_cast<float>(m_resoures->getGridSize().y));
        return fabs(isoValue) + std::max(maxSize, maxSizeGrid) * 2.0f;
    }

    void SlicerProgram::renderResultImageReadPixel(DistanceMap & sourceImage,
                                                   GLImageBuffer & targetImage)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        ProfileFunction;
        swapProgramsIfNeeded();
        const cl::NDRange origin = {0, 0, 0};
        const cl::NDRange range = {targetImage.getWidth(), targetImage.getHeight(), 1};
        m_programFront->run(
          "render", origin, range, targetImage.getBuffer(), sourceImage.getBuffer());
    }

    void SlicerProgram::precomputeSdf(const Primitives & lines, BoundingBox boundingBox)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        ProfileFunction;
        swapProgramsIfNeeded();
        auto & target = m_resoures->getPrecompSdfBuffer();
        m_resoures->getRenderingSettings().approximation = AM_FULL_MODEL;
        const cl::NDRange origin = {0, 0, 0};
        const cl::NDRange range = {target.getWidth(), target.getHeight(), target.getDepth()};
        m_programFront->run(
          "preComputeSdf", origin, range, target.getBuffer(), boundingBox, PAYLOAD_ARGUMENTS);
    }

    void SlicerProgram::calculateNormals(const Primitives & lines, const Mesh & mesh)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        ProfileFunction;
        swapProgramsIfNeeded();
        m_resoures->getRenderingSettings().approximation = AM_FULL_MODEL;
        const cl::NDRange origin = {0, 0, 0};
        const cl::NDRange range = {mesh.getNumberOfVertices(), 1, 1};
        m_programFront->run("calculateVertexNormals",
                            origin,
                            range,
                            mesh.getVertices().getBuffer(),
                            mesh.getVertexNormals().getBuffer(),
                            PAYLOAD_ARGUMENTS);
    }

    void SlicerProgram::renderDownSkinDistance(DepthBuffer & targetImage,
                                               Primitives const & lines,
                                               cl_float z_mm)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        ProfileFunction;
        swapProgramsIfNeeded();
        m_resoures->getRenderingSettings().approximation = AM_FULL_MODEL;
        const cl::NDRange origin = {0, 0, 0};
        const cl::NDRange range = {targetImage.getWidth(), targetImage.getHeight(), 1};

        m_programFront->run("distanceToBottom",
                            origin,
                            range,
                            targetImage.getBuffer(),
                            PAYLOAD_ARGUMENTS_CLIPPING_AREA);

        targetImage.read();
    }

    void SlicerProgram::renderUpSkinDistance(DepthBuffer & targetImage,
                                             Primitives const & lines,
                                             cl_float z_mm)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        ProfileFunction;
        swapProgramsIfNeeded();
        m_resoures->getRenderingSettings().approximation = AM_FULL_MODEL;
        const cl::NDRange origin = {0, 0, 0};
        const cl::NDRange range = {targetImage.getWidth(), targetImage.getHeight(), 1};
        m_programFront->run(
          "distanceToTop", origin, range, targetImage.getBuffer(), PAYLOAD_ARGUMENTS_CLIPPING_AREA);

        targetImage.read();
    }

    void SlicerProgram::movePointsToSurface(Primitives const & lines,
                                            VertexBuffer & input,
                                            VertexBuffer & output)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        ProfileFunction;
        swapProgramsIfNeeded();
        if (!isValid())
        {
            throw std::runtime_error("Internal error (movePointsToSurface): Program is not valid");
        }
        if (input.getSize() != output.getSize())
        {
            throw std::runtime_error("Internal error (movePointsToSurface): Input and Output "
                                     "buffer need to have the same size");
        }

        input.write();
        output.write();

        m_resoures->getRenderingSettings().approximation = AM_FULL_MODEL;
        const cl::NDRange origin = {0, 0, 0};
        const cl::NDRange range = {output.getSize(), 1, 1};
        m_programFront->run("movePointsToSurface",
                            origin,
                            range,
                            input.getBuffer(),
                            output.getBuffer(),
                            PAYLOAD_ARGUMENTS);
        output.read();
    }

    void SlicerProgram::adoptVertexOfMeshToSurface(Primitives const & lines,
                                                   VertexBuffer & input,
                                                   VertexBuffer & output)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        ProfileFunction;
        swapProgramsIfNeeded();

        if (input.getSize() != output.getSize())
        {
            throw std::runtime_error("Internal error (movePointsToSurface): Input and Output "
                                     "buffer need to have the same size");
        }

        input.write();
        output.write();

        m_resoures->getRenderingSettings().approximation = AM_FULL_MODEL;
        const cl::NDRange origin = {0, 0, 0};
        const cl::NDRange range = {output.getSize(), 1, 1};
        m_programFront->run("adoptVertexOfMeshToSurface",
                            origin,
                            range,
                            input.getBuffer(),
                            output.getBuffer(),
                            PAYLOAD_ARGUMENTS);
        output.read();
    }

    void SlicerProgram::computeMarchingSquareState(const Primitives & lines, cl_float z_mm)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        ProfileFunction;
        swapProgramsIfNeeded();
        const cl::NDRange origin = {0, 0, 0};
        const cl::NDRange globalRange = {m_resoures->getMarchingSquareStates().getWidth(),
                                         m_resoures->getMarchingSquareStates().getHeight(),
                                         1};
        /*   m_resoures->getRenderingSettings().approximation =
             static_cast<ApproximationMode>(AM_HYBRID | AM_DISABLE_INTERPOLATION);*/
        m_resoures->getRenderingSettings().approximation = AM_FULL_MODEL;
        m_programFront->run("computeMarchingSquareStates",
                            origin,
                            globalRange,
                            m_resoures->getMarchingSquareStates().getBuffer(), // 0
                            z_mm,
                            PAYLOAD_ARGUMENTS_CLIPPING_AREA);

        m_resoures->getMarchingSquareStates().read();
    }

    void SlicerProgram::adoptVertexPositions2d(const Primitives & lines,
                                               Vertex2dBuffer & input,
                                               Vertex2dBuffer & output,
                                               cl_float z_mm)
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        ProfileFunction;

        swapProgramsIfNeeded();

        if (input.getSize() != output.getSize())
        {
            throw std::runtime_error("Internal error (movePointsToSurface): Input and Output "
                                     "buffer need to have the same size");
        }

        input.write();
        output.write();

        m_resoures->getRenderingSettings().approximation = AM_FULL_MODEL;
        const cl::NDRange origin = {0, 0, 0};
        const cl::NDRange range = {output.getSize(), 1, 1};

        for (int i = 0; i < 3; ++i)
        {
            cl_int const numIterations = 1 + i * 5;
            m_programFront->run("adoptVertexPositions2d",
                                origin,
                                range,
                                input.getBuffer(),
                                output.getBuffer(),
                                static_cast<cl_int>(output.getSize()),
                                numIterations,
                                z_mm,
                                PAYLOAD_ARGUMENTS);

            m_programFront->run("adoptVertexPositions2d",
                                origin,
                                range,
                                output.getBuffer(),
                                input.getBuffer(),
                                static_cast<cl_int>(output.getSize()),
                                numIterations,
                                z_mm,
                                PAYLOAD_ARGUMENTS);
        }
        m_programFront->run("adoptVertexPositions2d",
                            origin,
                            range,
                            input.getBuffer(),
                            output.getBuffer(),
                            static_cast<cl_int>(output.getSize()),
                            5,
                            z_mm,
                            PAYLOAD_ARGUMENTS);
        output.read();
    }

    void SlicerProgram::setKernelReplacements(SharedKernelReplacements replacements)
    {
        m_programFront->setKernelReplacements(replacements);
    }
}
