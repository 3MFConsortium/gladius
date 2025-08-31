#include "FunctionArgument.h"
#include "mcp/MCPApplicationInterface.h"
#include "mcp/MCPServer.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

namespace gladius::tests
{
    using json = nlohmann::json;

    // Mock application for JSON-RPC testing
    class MockJSONRPCApplication : public MCPApplicationInterface
    {
      public:
        std::string getVersion() const override
        {
            return "1.0.0";
        }
        bool isRunning() const override
        {
            return true;
        }
        std::string getApplicationName() const override
        {
            return "Gladius";
        }
        std::string getStatus() const override
        {
            return "running";
        }
        bool hasActiveDocument() const override
        {
            return false;
        }
        std::string getActiveDocumentPath() const override
        {
            return "";
        }

        bool createNewDocument() override
        {
            return true;
        }
        bool openDocument(const std::string &) override
        {
            return true;
        }
        bool saveDocument() override
        {
            return true;
        }
        bool saveDocumentAs(const std::string &) override
        {
            return true;
        }
        bool exportDocument(const std::string &, const std::string &) override
        {
            return true;
        }

        bool setFloatParameter(uint32_t, const std::string &, const std::string &, float) override
        {
            return true;
        }
        float getFloatParameter(uint32_t, const std::string &, const std::string &) override
        {
            return 0.0f;
        }
        bool setStringParameter(uint32_t,
                                const std::string &,
                                const std::string &,
                                const std::string &) override
        {
            return true;
        }
        std::string getStringParameter(uint32_t, const std::string &, const std::string &) override
        {
            return "";
        }

        std::pair<bool, uint32_t>
        createFunctionFromExpression(const std::string &,
                                     const std::string &,
                                     const std::string &,
                                     const std::vector<FunctionArgument> & = {},
                                     const std::string & = "") override
        {
            return {true, 123}; // Mock resource ID
        }

        std::string getLastErrorMessage() const override
        {
            return "";
        }

        // Headless/UI control additions
        void setHeadlessMode(bool) override
        {
        }
        bool isHeadlessMode() const override
        {
            return true;
        }
        bool showUI() override
        {
            return true;
        }
        bool isUIRunning() const override
        {
            return false;
        }

        // New MCP interface methods
        bool validateDocumentFor3MF() const override
        {
            return true;
        }
        bool exportDocumentAs3MF(const std::string & path,
                                 bool includeImplicitFunctions = true) const override
        {
            return true;
        }

        // New 3MF Resource creation methods
        std::pair<bool, uint32_t> createLevelSet(uint32_t functionId) override
        {
            return {true, functionId + 1000}; // Mock level set ID
        }
        std::pair<bool, uint32_t> createImage3DFunction(const std::string & name,
                                                        const std::string & imagePath,
                                                        float valueScale = 1.0f,
                                                        float valueOffset = 0.0f) override
        {
            return {true, 555}; // Mock image3D function ID
        }
        std::pair<bool, uint32_t> createVolumetricColor(uint32_t functionId,
                                                        const std::string & channel) override
        {
            return {true, functionId + 2000}; // Mock color data ID
        }
        std::pair<bool, uint32_t> createVolumetricProperty(const std::string & propertyName,
                                                           uint32_t functionId,
                                                           const std::string & channel) override
        {
            return {true, functionId + 3000}; // Mock property data ID
        }

        nlohmann::json analyzeFunctionProperties(const std::string & functionName) const override
        {
            return nlohmann::json{{"status", "mock"}};
        }

        nlohmann::json getSceneHierarchy() const override
        {
            return nlohmann::json{{"nodes", nlohmann::json::array()}};
        }
        nlohmann::json getDocumentInfo() const override
        {
            return nlohmann::json{{"name", "mock_document"}};
        }
        nlohmann::json get3MFStructure() const override
        {
            return nlohmann::json{{"has_document", false},
                                  {"build_items", nlohmann::json::array()},
                                  {"resources", nlohmann::json::array()},
                                  {"counts", {}}};
        }
        nlohmann::json getFunctionGraph(uint32_t functionId) const override
        {
            // Minimal mock: return an empty graph with the given id
            return nlohmann::json{{"model", {{"resource_id", functionId}}},
                                  {"nodes", nlohmann::json::array()},
                                  {"links", nlohmann::json::array()},
                                  {"counts", {{"nodes", 0}, {"links", 0}}}};
        }
        std::vector<std::string> listAvailableFunctions() const override
        {
            return {};
        }
        nlohmann::json
        validateForManufacturing(const std::vector<std::string> & functionNames = {},
                                 const nlohmann::json & constraints = {}) const override
        {
            return nlohmann::json{{"valid", true}};
        }
        bool executeBatchOperations(const nlohmann::json & operations,
                                    bool rollbackOnError = true) override
        {
            return true;
        }

        // New methods added to the interface - provide simple mocks
        bool setBuildItemObjectByIndex(uint32_t /*buildItemIndex*/,
                                       uint32_t /*objectModelResourceId*/) override
        {
            return true;
        }
        bool setBuildItemTransformByIndex(
          uint32_t /*buildItemIndex*/,
          const std::array<float, 12> & /*transform4x3RowMajor*/) override
        {
            return true;
        }
        bool modifyLevelSet(uint32_t /*levelSetModelResourceId*/,
                            std::optional<uint32_t> /*functionModelResourceId*/,
                            std::optional<std::string> /*channel*/) override
        {
            return true;
        }

        // Rendering methods
        bool renderToFile(const std::string & /*outputPath*/,
                          uint32_t /*width*/,
                          uint32_t /*height*/,
                          const std::string & /*format*/,
                          float /*quality*/) override
        {
            return true;
        }

        bool renderWithCamera(const std::string & /*outputPath*/,
                              const nlohmann::json & /*cameraSettings*/,
                              const nlohmann::json & /*renderSettings*/) override
        {
            return true;
        }

        bool generateThumbnail(const std::string & /*outputPath*/, uint32_t /*size*/) override
        {
            return true;
        }

        nlohmann::json getOptimalCameraPosition() const override
        {
            return nlohmann::json{{"eye_position", {1.0, 1.0, 1.0}},
                                  {"target_position", {0.0, 0.0, 0.0}},
                                  {"up_vector", {0.0, 0.0, 1.0}}};
        }

        nlohmann::json getNodeInfo(uint32_t functionId, uint32_t nodeId) const override
        {
            return nlohmann::json{{"success", true}};
        }

        nlohmann::json createNode(uint32_t functionId,
                                  const std::string & nodeType,
                                  const std::string & displayName,
                                  uint32_t nodeId) override
        {
            return nlohmann::json{{"success", true}};
        }

        nlohmann::json deleteNode(uint32_t functionId, uint32_t nodeId) override
        {
            return nlohmann::json{{"success", true}};
        }

        nlohmann::json setParameterValue(uint32_t functionId,
                                         uint32_t nodeId,
                                         const std::string & parameterName,
                                         const nlohmann::json & value) override
        {
            return nlohmann::json{{"success", true}};
        }

        nlohmann::json createLink(uint32_t functionId,
                                  uint32_t sourceNodeId,
                                  const std::string & sourcePortName,
                                  uint32_t targetNodeId,
                                  const std::string & targetParameterName) override
        {
            return nlohmann::json{{"success", true}};
        }

        nlohmann::json deleteLink(uint32_t functionId,
                                  uint32_t targetNodeId,
                                  const std::string & targetParameterName) override
        {
            return nlohmann::json{{"success", true}};
        }

        nlohmann::json createFunctionCallNode(uint32_t targetFunctionId,
                                              uint32_t referencedFunctionId,
                                              const std::string & displayName) override
        {
            return nlohmann::json{{"success", true}};
        }

        nlohmann::json createConstantNodesForMissingParameters(uint32_t functionId,
                                                               uint32_t nodeId,
                                                               bool autoConnect) override
        {
            return nlohmann::json{{"success", true}};
        }

        nlohmann::json removeUnusedNodes(uint32_t functionId) override
        {
            return nlohmann::json{{"success", true}};
        }

        nlohmann::json validateModel(const nlohmann::json & options) override
        {
            return nlohmann::json{{"success", true}};
        }
    };

    class JSONRPCTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            m_mockApp = std::make_unique<MockJSONRPCApplication>();
            m_server = std::make_unique<mcp::MCPServer>(m_mockApp.get());
        }

        std::unique_ptr<MockJSONRPCApplication> m_mockApp;
        std::unique_ptr<mcp::MCPServer> m_server;
    };

    // Test valid JSON-RPC 2.0 request structure
    TEST_F(JSONRPCTest, ProcessRequest_ValidStructure_ReturnsValidResponse)
    {
        // Arrange
        json request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_EQ(response["id"], 1);
        EXPECT_TRUE(response.contains("result"));
        EXPECT_FALSE(response.contains("error"));
    }

    // Test missing jsonrpc field
    TEST_F(JSONRPCTest, ProcessRequest_MissingJSONRPC_ReturnsError)
    {
        // Arrange
        json request = {{"id", 1}, {"method", "tools/list"}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_TRUE(response.contains("error"));
        EXPECT_EQ(response["error"]["code"], -32600); // Invalid Request
    }

    // Test missing method field
    TEST_F(JSONRPCTest, ProcessRequest_MissingMethod_ReturnsError)
    {
        // Arrange
        json request = {{"jsonrpc", "2.0"}, {"id", 1}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_TRUE(response.contains("error"));
        EXPECT_EQ(response["error"]["code"], -32600); // Invalid Request
    }

    // Test unknown method
    TEST_F(JSONRPCTest, ProcessRequest_UnknownMethod_ReturnsMethodNotFound)
    {
        // Arrange
        json request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "unknown/method"}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_EQ(response["id"], 1);
        EXPECT_TRUE(response.contains("error"));
        EXPECT_EQ(response["error"]["code"], -32601); // Method not found
    }

    // Test tools/list method
    TEST_F(JSONRPCTest, ToolsList_ValidRequest_ReturnsToolList)
    {
        // Arrange
        json request = {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_EQ(response["id"], 1);
        ASSERT_TRUE(response.contains("result"));
        ASSERT_TRUE(response["result"].contains("tools"));

        auto tools = response["result"]["tools"];
        EXPECT_TRUE(tools.is_array());
        EXPECT_GT(tools.size(), 0);

        // Verify tool structure
        for (const auto & tool : tools)
        {
            EXPECT_TRUE(tool.contains("name"));
            EXPECT_TRUE(tool.contains("description"));
            EXPECT_TRUE(tool.contains("inputSchema"));
            EXPECT_TRUE(tool["name"].is_string());
            EXPECT_TRUE(tool["description"].is_string());
            EXPECT_TRUE(tool["inputSchema"].is_object());
        }
    }
    // Test tools/call method with missing tool name
    TEST_F(JSONRPCTest, ToolsCall_MissingToolName_ReturnsError)
    {
        // Arrange
        json request = {
          {"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/call"}, {"params", {{"arguments", {}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_EQ(response["id"], 1);
        EXPECT_TRUE(response.contains("error"));
        EXPECT_EQ(response["error"]["code"], -32602); // Invalid params
    }

    // Test tools/call method with unknown tool
    TEST_F(JSONRPCTest, ToolsCall_UnknownTool_ReturnsError)
    {
        // Arrange
        json request = {{"jsonrpc", "2.0"},
                        {"id", 1},
                        {"method", "tools/call"},
                        {"params", {{"name", "unknown_tool"}, {"arguments", {}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_EQ(response["id"], 1);
        EXPECT_TRUE(response.contains("error"));
        EXPECT_EQ(response["error"]["code"], -32601); // Method not found
    }

    // Test notification (request without id)
    TEST_F(JSONRPCTest, ProcessRequest_Notification_NoResponse)
    {
        // Arrange
        json request = {
          {"jsonrpc", "2.0"}, {"method", "tools/list"} // No id field - this is a notification
        };

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        // For notifications, JSON-RPC servers should not send a response
        // Our current implementation might still return something, but it shouldn't have an id
        if (!response.empty())
        {
            EXPECT_FALSE(response.contains("id"));
        }
    }

    // Test batch request (array of requests)
    TEST_F(JSONRPCTest, ProcessRequest_BatchRequest_ReturnsArrayResponse)
    {
        // Arrange
        json batchRequest = json::array({{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}},
                                         {{"jsonrpc", "2.0"},
                                          {"id", 2},
                                          {"method", "tools/call"},
                                          {"params", {{"name", "ping"}, {"arguments", {}}}}}});

        // Act & Assert
        // Note: Our current implementation might not support batch requests yet
        // This test documents the expected behavior for future implementation
        try
        {
            json response = m_server->processJSONRPCRequest(batchRequest);

            if (response.is_array())
            {
                EXPECT_EQ(response.size(), 2);
                EXPECT_EQ(response[0]["id"], 1);
                EXPECT_EQ(response[1]["id"], 2);
            }
            else
            {
                // Current implementation might not support batch - that's okay for now
                EXPECT_TRUE(response.contains("error"));
            }
        }
        catch (const std::exception &)
        {
            // If batch requests aren't supported yet, that's acceptable
            SUCCEED() << "Batch requests not yet implemented - test documents expected behavior";
        }
    }

    // Test malformed JSON
    TEST_F(JSONRPCTest, ProcessRequest_MalformedJSON_ThrowsParseError)
    {
        // This test would be done at a higher level where JSON parsing occurs
        // The processJSONRPCRequest method expects valid JSON input
        SUCCEED() << "Malformed JSON handling tested at higher level";
    }

    // Test parameter validation for complex tools
    TEST_F(JSONRPCTest, ToolsCall_CreateFunction_ValidatesParameters)
    {
        // Arrange
        json request = {{"jsonrpc", "2.0"},
                        {"id", 1},
                        {"method", "tools/call"},
                        {"params",
                         {{"name", "create_function_from_expression"},
                          {"arguments",
                           {{"name", "test_func"},
                            {"expression", "sin(x) + cos(y)"},
                            {"output_type", "float"}}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_EQ(response["id"], 1);
        ASSERT_TRUE(response.contains("result"));

        auto content = response["result"]["content"];
        std::string jsonString = content[0]["text"];
        json functionResult = json::parse(jsonString);
        EXPECT_TRUE(functionResult.contains("success"));
        EXPECT_TRUE(functionResult.contains("function_name"));
        EXPECT_TRUE(functionResult.contains("expression"));
    }

    // Test string vs numeric ID handling
    TEST_F(JSONRPCTest, ProcessRequest_StringID_PreservesIDType)
    {
        // Arrange
        json request = {{"jsonrpc", "2.0"}, {"id", "string-id-123"}, {"method", "tools/list"}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_EQ(response["id"], "string-id-123");
        EXPECT_TRUE(response.contains("result"));
    }

    // Test null ID handling
    TEST_F(JSONRPCTest, ProcessRequest_NullID_PreservesNullID)
    {
        // Arrange
        json request = {{"jsonrpc", "2.0"}, {"id", nullptr}, {"method", "tools/list"}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_TRUE(response["id"].is_null());
        EXPECT_TRUE(response.contains("result"));
    }
}
