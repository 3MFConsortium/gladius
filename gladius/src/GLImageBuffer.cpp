#include "GLImageBuffer.h"

namespace gladius
{
    void GLImageBuffer::allocateOnDevice()
    {
        m_width = std::max(static_cast<size_t>(1), m_width);
        m_height = std::max(static_cast<size_t>(1), m_height);

        m_data.resize(m_width * m_height);
        std::fill(m_data.begin(), m_data.end(), cl_float4());

        // create texture
        glGenTextures(1, &m_textureID);
        glBindTexture(GL_TEXTURE_2D, m_textureID);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA32F,
                     static_cast<GLsizei>(m_width),
                     static_cast<GLsizei>(m_height),
                     0,
                     GL_RGBA,
                     GL_FLOAT,
                     m_data.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, 0);
        glFinish();

        if (m_ComputeContext.outputMethod() == OutputMethod::interop)
        {
            setupForInterOp();
        }
        if (m_ComputeContext.outputMethod() == OutputMethod::readpixel)
        {
            setupForReadPixel();
        }
    }

    GLImageBuffer::~GLImageBuffer()
    {
        if (glIsTexture(m_textureID) != 0u)
        {
            glDeleteTextures(1, &m_textureID);
        }
    }

    void GLImageBuffer::bind()
    {
        if (m_dirty)
        {
            transferPixelInReadPixelMode();
        }
        if (glIsTexture(m_textureID) != 0u)
        {
            glBindTexture(GL_TEXTURE_2D, m_textureID);
        }
    }

    void GLImageBuffer::unbind()
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    GLuint GLImageBuffer::GetTextureId() const
    {
        return m_textureID;
    }

    void GLImageBuffer::transferPixelInReadPixelMode()
    {
        if (m_ComputeContext.outputMethod() == OutputMethod::readpixel)
        {
            transferPixels();
            m_dirty = false;
        }
    }

    void GLImageBuffer::invalidateContent()
    {
        m_dirty = true;
    }

    void GLImageBuffer::setupForInterOp()
    {
        cl_int err = 0;
        m_buffer = std::make_unique<cl::ImageGL>(
            m_ComputeContext.GetContext(), CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, m_textureID, &err);
        CL_ERROR(err);
    }

    void GLImageBuffer::setupForReadPixel()
    {
        static const cl::ImageFormat format = {CL_RGBA, CL_FLOAT};
        cl_int err = 0;
        m_buffer = std::make_unique<cl::Image2D>(m_ComputeContext.GetContext(),
                                                 CL_MEM_READ_WRITE,
                                                 format,
                                                 m_width,
                                                 m_height,
                                                 0,
                                                 nullptr,
                                                 &err);
        CL_ERROR(err);
    }

    void GLImageBuffer::transferPixels()
    {
        if (!m_buffer)
        {
            return;
        }

        m_ComputeContext.GetQueue().enqueueReadImage(
            *m_buffer, CL_TRUE, {}, {m_width, m_height, 1u}, 0, 0, m_data.data());
        CL_ERROR(m_ComputeContext.GetQueue().finish());

        glBindTexture(GL_TEXTURE_2D, m_textureID);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA32F,
                     static_cast<GLsizei>(m_width),
                     static_cast<GLsizei>(m_height),
                     0,
                     GL_RGBA,
                     GL_FLOAT,
                     m_data.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
