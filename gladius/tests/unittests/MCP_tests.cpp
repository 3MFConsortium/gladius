#include "FunctionArgument.h"
#include "mcp/ApplicationMCPAdapter.h"
#include "mcp/MCPApplicationInterface.h"
#include "mcp/MCPServer.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <nlohmann/json.hpp>

namespace gladius::tests
{
    using json = nlohmann::json;

    // Type aliases to avoid comma issues in MOCK_METHOD
    using Float3Array = std::array<float, 3>;
    using Float6Array = std::array<float, 6>;

    // Mock implementation of MCPApplicationInterface for testing
    class MockMCPApplication : public MCPApplicationInterface
    {
      public:
        MOCK_METHOD(std::string, getVersion, (), (const, override));
        MOCK_METHOD(bool, isRunning, (), (const, override));
        MOCK_METHOD(std::string, getApplicationName, (), (const, override));
        MOCK_METHOD(std::string, getStatus, (), (const, override));
        MOCK_METHOD(bool, hasActiveDocument, (), (const, override));
        MOCK_METHOD(std::string, getActiveDocumentPath, (), (const, override));

        MOCK_METHOD(bool, createNewDocument, (), (override));
        MOCK_METHOD(bool, openDocument, (const std::string &), (override));
        MOCK_METHOD(bool, saveDocument, (), (override));
        MOCK_METHOD(bool, saveDocumentAs, (const std::string &), (override));
        MOCK_METHOD(bool, exportDocument, (const std::string &, const std::string &), (override));

        MOCK_METHOD(bool,
                    setFloatParameter,
                    (uint32_t, const std::string &, const std::string &, float),
                    (override));
        MOCK_METHOD(float,
                    getFloatParameter,
                    (uint32_t, const std::string &, const std::string &),
                    (override));
        MOCK_METHOD(bool,
                    setStringParameter,
                    (uint32_t, const std::string &, const std::string &, const std::string &),
                    (override));
        MOCK_METHOD(std::string,
                    getStringParameter,
                    (uint32_t, const std::string &, const std::string &),
                    (override));

        MOCK_METHOD((std::pair<bool, uint32_t>),
                    createFunctionFromExpression,
                    (const std::string &,
                     const std::string &,
                     const std::string &,
                     const std::vector<FunctionArgument> &,
                     const std::string &),
                    (override));

        MOCK_METHOD(std::string, getLastErrorMessage, (), (const, override));

        // New MCP interface methods
        MOCK_METHOD(bool, validateDocumentFor3MF, (), (const, override));
        MOCK_METHOD(bool, exportDocumentAs3MF, (const std::string &, bool), (const, override));
        MOCK_METHOD((std::pair<bool, uint32_t>),
                    createSDFFunction,
                    (const std::string &, const std::string &),
                    (override));
        MOCK_METHOD(
          (std::pair<bool, uint32_t>),
          createCSGOperation,
          (const std::string &, const std::string &, const std::vector<std::string> &, bool, float),
          (override));

        // New 3MF Resource creation methods
        MOCK_METHOD((std::pair<bool, uint32_t>), createLevelSet, (uint32_t, int), (override));
        MOCK_METHOD((std::pair<bool, uint32_t>),
                    createImage3DFunction,
                    (const std::string &, const std::string &, float, float),
                    (override));
        MOCK_METHOD((std::pair<bool, uint32_t>),
                    createVolumetricColor,
                    (uint32_t, const std::string &),
                    (override));
        MOCK_METHOD((std::pair<bool, uint32_t>),
                    createVolumetricProperty,
                    (const std::string &, uint32_t, const std::string &),
                    (override));
        MOCK_METHOD(
          bool,
          applyTransformToFunction,
          (const std::string &, const Float3Array &, const Float3Array &, const Float3Array &),
          (override));
        MOCK_METHOD(nlohmann::json,
                    analyzeFunctionProperties,
                    (const std::string &),
                    (const, override));
        MOCK_METHOD(nlohmann::json,
                    generateMeshFromFunction,
                    (const std::string &, int, const Float6Array &),
                    (const, override));
        MOCK_METHOD(nlohmann::json, getSceneHierarchy, (), (const, override));
        MOCK_METHOD(nlohmann::json, getDocumentInfo, (), (const, override));
        MOCK_METHOD(std::vector<std::string>, listAvailableFunctions, (), (const, override));
        MOCK_METHOD(nlohmann::json,
                    validateForManufacturing,
                    (const std::vector<std::string> &, const nlohmann::json &),
                    (const, override));
        MOCK_METHOD(bool, executeBatchOperations, (const nlohmann::json &, bool), (override));
    };

    class MCPServerTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            m_mockApp = std::make_unique<MockMCPApplication>();
            m_server = std::make_unique<mcp::MCPServer>(m_mockApp.get());
        }

        void TearDown() override
        {
            m_server.reset();
            m_mockApp.reset();
        }

        std::unique_ptr<MockMCPApplication> m_mockApp;
        std::unique_ptr<mcp::MCPServer> m_server;
    };

    // Test basic tool registration and discovery
    TEST_F(MCPServerTest, ListTools_ServerInitialized_ReturnsExpectedTools)
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
        EXPECT_GE(tools.size(), 10); // Should have at least 10 tools

        // Check for essential tools that are actually implemented
        std::vector<std::string> expectedTools = {"ping",
                                                  "get_status",
                                                  "create_document",
                                                  "open_document",
                                                  "save_document_as",
                                                  "create_function_from_expression",
                                                  "create_levelset",
                                                  "create_image3d_function",
                                                  "create_volumetric_color",
                                                  "create_volumetric_property"};

        for (const auto & expectedTool : expectedTools)
        {
            bool found = false;
            for (const auto & tool : tools)
            {
                if (tool["name"] == expectedTool)
                {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found) << "Expected tool '" << expectedTool << "' not found";
        }
    }

    // Test ping tool
    TEST_F(MCPServerTest, PingTool_WithMessage_ReturnsEcho)
    {
        // Arrange
        json request = {
          {"jsonrpc", "2.0"},
          {"id", 1},
          {"method", "tools/call"},
          {"params", {{"name", "ping"}, {"arguments", {{"message", "test message"}}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_EQ(response["id"], 1);
        ASSERT_TRUE(response.contains("result"));
        ASSERT_TRUE(response["result"].contains("content"));

        auto content = response["result"]["content"];
        ASSERT_TRUE(content.is_array() && !content.empty());
        EXPECT_EQ(content[0]["type"], "text");

        // Get the JSON response text (it's already parsed)
        std::string jsonString = content[0]["text"];
        json pingResult = json::parse(jsonString);
        EXPECT_EQ(pingResult["response"], "test message");
        EXPECT_TRUE(pingResult.contains("timestamp"));
    }

    // Test get_status tool
    TEST_F(MCPServerTest, GetStatusTool_MockApplication_ReturnsApplicationInfo)
    {
        // Arrange
        EXPECT_CALL(*m_mockApp, getApplicationName()).WillOnce(::testing::Return("Gladius"));
        EXPECT_CALL(*m_mockApp, getVersion()).WillOnce(::testing::Return("1.0.0"));
        EXPECT_CALL(*m_mockApp, isRunning()).WillOnce(::testing::Return(true));
        EXPECT_CALL(*m_mockApp, getStatus()).WillOnce(::testing::Return("running"));
        EXPECT_CALL(*m_mockApp, hasActiveDocument()).WillOnce(::testing::Return(false));
        EXPECT_CALL(*m_mockApp, getActiveDocumentPath()).WillOnce(::testing::Return(""));

        json request = {{"jsonrpc", "2.0"},
                        {"id", 1},
                        {"method", "tools/call"},
                        {"params", {{"name", "get_status"}, {"arguments", {}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_EQ(response["id"], 1);
        ASSERT_TRUE(response.contains("result"));

        auto content = response["result"]["content"];
        ASSERT_TRUE(content.is_array() && !content.empty());

        std::string jsonString = content[0]["text"];
        json statusResult = json::parse(jsonString);
        EXPECT_EQ(statusResult["application"], "Gladius");
        EXPECT_EQ(statusResult["version"], "1.0.0");
        EXPECT_EQ(statusResult["is_running"], true);
        EXPECT_EQ(statusResult["status"], "running");
        EXPECT_EQ(statusResult["has_active_document"], false);
    }

    // Test document creation
    TEST_F(MCPServerTest, CreateDocumentTool_Success_ReturnsSuccessMessage)
    {
        // Arrange
        EXPECT_CALL(*m_mockApp, createNewDocument()).WillOnce(::testing::Return(true));

        json request = {{"jsonrpc", "2.0"},
                        {"id", 1},
                        {"method", "tools/call"},
                        {"params", {{"name", "create_document"}, {"arguments", {}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_EQ(response["id"], 1);
        ASSERT_TRUE(response.contains("result"));

        auto content = response["result"]["content"];
        std::string jsonString = content[0]["text"];
        json createResult = json::parse(jsonString);
        EXPECT_EQ(createResult["success"], true);
        EXPECT_EQ(createResult["message"], "New 3MF document created");
    }

    // Test document creation failure
    TEST_F(MCPServerTest, CreateDocumentTool_Failure_ReturnsFailureMessage)
    {
        // Arrange
        EXPECT_CALL(*m_mockApp, createNewDocument()).WillOnce(::testing::Return(false));

        json request = {{"jsonrpc", "2.0"},
                        {"id", 1},
                        {"method", "tools/call"},
                        {"params", {{"name", "create_document"}, {"arguments", {}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        auto content = response["result"]["content"];
        std::string jsonString = content[0]["text"];
        json createResult = json::parse(jsonString);
        EXPECT_EQ(createResult["success"], false);
        EXPECT_EQ(createResult["message"], "Failed to create document");
    }

    // Test expression function creation
    TEST_F(MCPServerTest, CreateFunctionFromExpressionTool_ValidExpression_CallsAdapter)
    {
        // Arrange
        EXPECT_CALL(*m_mockApp,
                    createFunctionFromExpression(
                      "test_function", "sin(x) + cos(y)", "float", ::testing::_, ::testing::_))
          .WillOnce(::testing::Return(std::make_pair(true, 123u)));

        json request = {{"jsonrpc", "2.0"},
                        {"id", 1},
                        {"method", "tools/call"},
                        {"params",
                         {{"name", "create_function_from_expression"},
                          {"arguments",
                           {{"name", "test_function"},
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
        EXPECT_EQ(functionResult["success"], true);
        EXPECT_EQ(functionResult["resource_id"], 123u);
        EXPECT_EQ(functionResult["function_name"], "test_function");
        EXPECT_EQ(functionResult["expression"], "sin(x) + cos(y)");
        EXPECT_EQ(functionResult["output_type"], "float");
    }

    // Test parameter setting
    TEST_F(MCPServerTest, SetParameterTool_FloatParameter_CallsAdapter)
    {
        // Arrange
        EXPECT_CALL(*m_mockApp, setFloatParameter(1, "test_node", "test_param", 42.5f))
          .WillOnce(::testing::Return(true));

        json request = {{"jsonrpc", "2.0"},
                        {"id", 1},
                        {"method", "tools/call"},
                        {"params",
                         {{"name", "set_parameter"},
                          {"arguments",
                           {{"model_id", 1},
                            {"node_name", "test_node"},
                            {"parameter_name", "test_param"},
                            {"value", 42.5},
                            {"type", "float"}}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        auto content = response["result"]["content"];
        std::string jsonString = content[0]["text"];
        json paramResult = json::parse(jsonString);
        EXPECT_EQ(paramResult["success"], true);
        EXPECT_EQ(paramResult["model_id"], 1);
        EXPECT_EQ(paramResult["node_name"], "test_node");
        EXPECT_EQ(paramResult["parameter_name"], "test_param");
        EXPECT_EQ(paramResult["value"], 42.5);
    }

    // Test invalid JSON-RPC request
    TEST_F(MCPServerTest, ProcessRequest_InvalidJSONRPC_ReturnsError)
    {
        // Arrange
        json request = {
          {"method", "tools/list"} // Missing jsonrpc and id
        };

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        EXPECT_EQ(response["jsonrpc"], "2.0");
        EXPECT_TRUE(response.contains("error"));
        EXPECT_EQ(response["error"]["code"], -32600); // Invalid Request
    }

    // Test unknown tool
    TEST_F(MCPServerTest, CallTool_UnknownTool_ReturnsError)
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

    // Test missing required parameters
    TEST_F(MCPServerTest, CreateFunctionFromExpressionTool_MissingName_ReturnsError)
    {
        // Arrange
        json request = {{"jsonrpc", "2.0"},
                        {"id", 1},
                        {"method", "tools/call"},
                        {"params",
                         {{"name", "create_function_from_expression"},
                          {"arguments",
                           {
                             {"expression", "sin(x) + cos(y)"} // Missing name
                           }}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        auto content = response["result"]["content"];
        std::string jsonString = content[0]["text"];
        json functionResult = json::parse(jsonString);
        EXPECT_TRUE(functionResult.contains("error"));
        EXPECT_THAT(functionResult["error"].get<std::string>(),
                    ::testing::HasSubstr("Missing required parameter"));
    }

    // Test TPMS expression patterns
    TEST_F(MCPServerTest, CreateFunctionFromExpressionTool_GyroidExpression_ValidatesPattern)
    {
        // Arrange
        EXPECT_CALL(*m_mockApp,
                    createFunctionFromExpression(
                      "gyroid",
                      ::testing::HasSubstr("sin"), // Should contain sin functions for gyroid
                      "float",
                      ::testing::_,
                      ::testing::_))
          .WillOnce(::testing::Return(std::make_pair(true, 456u)));

        json request = {{"jsonrpc", "2.0"},
                        {"id", 1},
                        {"method", "tools/call"},
                        {"params",
                         {{"name", "create_function_from_expression"},
                          {"arguments",
                           {{"name", "gyroid"},
                            {"expression",
                             "sin(x*2*pi/10)*cos(y*2*pi/10) + sin(y*2*pi/10)*cos(z*2*pi/10) + "
                             "sin(z*2*pi/10)*cos(x*2*pi/10) - 0.2"}}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        auto content = response["result"]["content"];
        std::string jsonString = content[0]["text"];
        json functionResult = json::parse(jsonString);
        EXPECT_EQ(functionResult["success"], true);
        EXPECT_EQ(functionResult["function_name"], "gyroid");
        EXPECT_THAT(functionResult["expression"].get<std::string>(),
                    ::testing::AllOf(::testing::HasSubstr("sin(x"),
                                     ::testing::HasSubstr("cos(y"),
                                     ::testing::HasSubstr("sin(z")));
    }

    // ========================================
    // Save Document As Tests
    // ========================================

    TEST_F(MCPServerTest, SaveDocumentAsTool_ValidPath_ReturnsSuccess)
    {
        // Arrange
        EXPECT_CALL(*m_mockApp, saveDocumentAs("/tmp/test.3mf")).WillOnce(::testing::Return(true));
        EXPECT_CALL(*m_mockApp, getLastErrorMessage())
          .WillOnce(::testing::Return("Document saved successfully to /tmp/test.3mf"));

        json request = {
          {"jsonrpc", "2.0"},
          {"id", 1},
          {"method", "tools/call"},
          {"params", {{"name", "save_document_as"}, {"arguments", {{"path", "/tmp/test.3mf"}}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        auto content = response["result"]["content"];
        std::string jsonString = content[0]["text"];
        json saveResult = json::parse(jsonString);
        EXPECT_EQ(saveResult["success"], true);
        EXPECT_EQ(saveResult["path"], "/tmp/test.3mf");
        EXPECT_THAT(saveResult["message"].get<std::string>(),
                    ::testing::HasSubstr("Document saved successfully"));
    }

    TEST_F(MCPServerTest, SaveDocumentAsTool_InvalidPath_ReturnsDetailedError)
    {
        // Arrange
        EXPECT_CALL(*m_mockApp, saveDocumentAs("invalid_path")).WillOnce(::testing::Return(false));
        EXPECT_CALL(*m_mockApp, getLastErrorMessage())
          .WillOnce(::testing::Return("File must have .3mf extension. Current path: invalid_path"));

        json request = {
          {"jsonrpc", "2.0"},
          {"id", 1},
          {"method", "tools/call"},
          {"params", {{"name", "save_document_as"}, {"arguments", {{"path", "invalid_path"}}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        auto content = response["result"]["content"];
        std::string jsonString = content[0]["text"];
        json saveResult = json::parse(jsonString);
        EXPECT_EQ(saveResult["success"], false);
        EXPECT_EQ(saveResult["path"], "invalid_path");
        EXPECT_THAT(saveResult["message"].get<std::string>(),
                    ::testing::HasSubstr("File must have .3mf extension"));
    }

    TEST_F(MCPServerTest, SaveDocumentAsTool_NoActiveDocument_ReturnsDetailedError)
    {
        // Arrange
        EXPECT_CALL(*m_mockApp, saveDocumentAs("/tmp/test.3mf")).WillOnce(::testing::Return(false));
        EXPECT_CALL(*m_mockApp, getLastErrorMessage())
          .WillOnce(::testing::Return(
            "No active document available. Please create or open a document first."));

        json request = {
          {"jsonrpc", "2.0"},
          {"id", 1},
          {"method", "tools/call"},
          {"params", {{"name", "save_document_as"}, {"arguments", {{"path", "/tmp/test.3mf"}}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        auto content = response["result"]["content"];
        std::string jsonString = content[0]["text"];
        json saveResult = json::parse(jsonString);
        EXPECT_EQ(saveResult["success"], false);
        EXPECT_EQ(saveResult["path"], "/tmp/test.3mf");
        EXPECT_THAT(saveResult["message"].get<std::string>(),
                    ::testing::HasSubstr("No active document available"));
    }

    TEST_F(MCPServerTest, SaveDocumentAsTool_ExceptionDuringSave_ReturnsDetailedError)
    {
        // Arrange
        EXPECT_CALL(*m_mockApp, saveDocumentAs("/tmp/test.3mf")).WillOnce(::testing::Return(false));
        EXPECT_CALL(*m_mockApp, getLastErrorMessage())
          .WillOnce(::testing::Return("Exception while saving document: Permission denied"));

        json request = {
          {"jsonrpc", "2.0"},
          {"id", 1},
          {"method", "tools/call"},
          {"params", {{"name", "save_document_as"}, {"arguments", {{"path", "/tmp/test.3mf"}}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        auto content = response["result"]["content"];
        std::string jsonString = content[0]["text"];
        json saveResult = json::parse(jsonString);
        EXPECT_EQ(saveResult["success"], false);
        EXPECT_EQ(saveResult["path"], "/tmp/test.3mf");
        EXPECT_THAT(saveResult["message"].get<std::string>(),
                    ::testing::HasSubstr("Exception while saving document"));
        EXPECT_THAT(saveResult["message"].get<std::string>(),
                    ::testing::HasSubstr("Permission denied"));
    }

    TEST_F(MCPServerTest, SaveDocumentAsTool_MissingPathParameter_ReturnsError)
    {
        // Arrange - no expectations as the request should fail validation

        json request = {
          {"jsonrpc", "2.0"},
          {"id", 1},
          {"method", "tools/call"},
          {"params", {{"name", "save_document_as"}, {"arguments", {}}}}}; // Missing path parameter

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        auto content = response["result"]["content"];
        std::string jsonString = content[0]["text"];
        json saveResult = json::parse(jsonString);
        EXPECT_TRUE(saveResult.contains("error"));
        EXPECT_THAT(saveResult["error"].get<std::string>(),
                    ::testing::HasSubstr("Missing required parameter"));
    }

    // ========================================
    // Save Document Tests (for comparison)
    // ========================================

    TEST_F(MCPServerTest, SaveDocumentTool_HasCurrentFile_ReturnsSuccess)
    {
        // Arrange
        EXPECT_CALL(*m_mockApp, saveDocument()).WillOnce(::testing::Return(true));
        EXPECT_CALL(*m_mockApp, getLastErrorMessage())
          .WillOnce(::testing::Return("Document saved successfully to /current/file.3mf"));

        json request = {{"jsonrpc", "2.0"},
                        {"id", 1},
                        {"method", "tools/call"},
                        {"params", {{"name", "save_document"}, {"arguments", {}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        auto content = response["result"]["content"];
        std::string jsonString = content[0]["text"];
        json saveResult = json::parse(jsonString);
        EXPECT_EQ(saveResult["success"], true);
        EXPECT_THAT(saveResult["message"].get<std::string>(),
                    ::testing::HasSubstr("Document saved successfully"));
    }

    TEST_F(MCPServerTest, SaveDocumentTool_NoCurrentFile_ReturnsDetailedError)
    {
        // Arrange
        EXPECT_CALL(*m_mockApp, saveDocument()).WillOnce(::testing::Return(false));
        EXPECT_CALL(*m_mockApp, getLastErrorMessage())
          .WillOnce(::testing::Return(
            "Document has not been saved before. Use 'save_document_as' to specify a filename."));

        json request = {{"jsonrpc", "2.0"},
                        {"id", 1},
                        {"method", "tools/call"},
                        {"params", {{"name", "save_document"}, {"arguments", {}}}}};

        // Act
        json response = m_server->processJSONRPCRequest(request);

        // Assert
        auto content = response["result"]["content"];
        std::string jsonString = content[0]["text"];
        json saveResult = json::parse(jsonString);
        EXPECT_EQ(saveResult["success"], false);
        EXPECT_THAT(saveResult["message"].get<std::string>(),
                    ::testing::HasSubstr("Document has not been saved before"));
        EXPECT_THAT(saveResult["message"].get<std::string>(),
                    ::testing::HasSubstr("Use 'save_document_as'"));
    }
}
