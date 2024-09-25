#include "testhelper.h"
#include <Document.h>
#include <fmt/core.h>
#include <nodes/Assembly.h>
#include <nodes/Model.h>
#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>
#include <algorithm>
#include <gtest/gtest.h>

namespace gladius_tests
{

    using namespace gladius;

    class Assembly_Test : public ::testing::Test
    {
        void SetUp() override
        {
            m_logger = std::make_shared<events::Logger>();
        }

      protected:
        nodes::SharedAssembly load3mf(std::filesystem::path const & path)
        {

            auto context = std::make_shared<ComputeContext>(EnableGLOutput::disabled);

            if (!context->isValid())
            {
                throw std::runtime_error(
                  "Failed to create OpenCL Context. Did you install proper GPU drivers?");
            }

            m_core =
              std::make_shared<ComputeCore>(context, RequiredCapabilities::ComputeOnly, m_logger);
            m_doc = std::make_shared<Document>(m_core);

            m_doc->load(path);

            return m_doc->getAssembly();
        }

      private:
        std::shared_ptr<Document> m_doc;
        std::shared_ptr<ComputeCore> m_core;
        events::SharedLogger m_logger;
    };

    TEST_F(Assembly_Test, CopyAssigment_LoadedAssembly_EqualsOriginal)
    {
        nodes::Assembly dolly;
        auto source = load3mf("testdata/ImplicitGyroid.3mf");
        dolly = *source;

        EXPECT_TRUE(dolly.equals(*source));
    }
}