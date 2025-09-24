#include "Document.h"
#include "testhelper.h"
#include <compute/ComputeCore.h>
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

    class ComputeCore_Test : public ::testing::Test
    {
        void SetUp() override
        {
            m_logger = std::make_shared<events::Logger>();
        }

      protected:
        std::shared_ptr<ComputeCore> load3mf(std::filesystem::path const & path)
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

            return m_core;
        }

      private:
        std::shared_ptr<ComputeCore> m_core;
        std::shared_ptr<Document> m_doc;
        events::SharedLogger m_logger;
    };

    TEST_F(ComputeCore_Test, PreComputeSDF_LoadedAssembly_EqualsExpectedResult)
    {
        auto core = load3mf("testdata/ImplicitGyroid.3mf");
        auto primitives = core->getPrimitives();
        auto const & payloadData = primitives->data.getData();
        auto const payloadDataHash = helper::computeHash(payloadData.cbegin(), payloadData.cend());
        EXPECT_EQ(payloadDataHash, 0u);

        auto resources = core->getResourceContext();
        auto const parameter = resources->getParameterBuffer().getData();
        for (auto const & param : parameter)
        {
            std::cout << param << std::endl;
        }

        auto const parameterHash = helper::computeHash(parameter.cbegin(), parameter.cend());
        constexpr auto expectedHash = 6494502327630714298u;
        EXPECT_EQ(parameterHash, expectedHash);
        EXPECT_TRUE(core->precomputeSdfForWholeBuildPlatform());

        // Reuse the previously defined resources variable instead of redefining it
        auto & preComp = resources->getPrecompSdfBuffer();
        preComp.read();
        auto const bufSize = preComp.getData().size();
        EXPECT_EQ(bufSize, 16777216u);

        auto const & data = preComp.getData();
        auto const hash = helper::computeHash(data.cbegin(), data.cend());
        EXPECT_EQ(hash, 13095517456146691086u);

        auto bBox = core->getBoundingBox();
        EXPECT_TRUE(bBox.has_value());

        auto const tolerance = 1E-3f;
        EXPECT_NEAR(bBox->min.x, -7.6475257873535156f, tolerance);
        EXPECT_NEAR(bBox->min.y, -1.9666776657104492f, tolerance);
        EXPECT_NEAR(bBox->min.z, -0.00098828284535557032f, tolerance);

        EXPECT_NEAR(bBox->max.x, 64.728408813476562f, tolerance);
        EXPECT_NEAR(bBox->max.y, 74.136703491210938f, tolerance);
        EXPECT_NEAR(bBox->max.z, 50.00640869140625f, tolerance);
    }
}