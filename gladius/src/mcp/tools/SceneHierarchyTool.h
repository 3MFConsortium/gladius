/**
 * @file SceneHierarchyTool.h
 * @brief MCP tool for scene inspection and hierarchy operations
 */

#pragma once

#include "MCPToolBase.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace gladius::mcp::tools
{
    /**
     * @brief MCP tool for scene inspection and hierarchy queries
     *
     * Handles read-only operations related to scene structure, document information,
     * and model hierarchy inspection.
     */
    class SceneHierarchyTool : public MCPToolBase
    {
      public:
        /**
         * @brief Construct a new SceneHierarchyTool object
         * @param app Pointer to the Application instance
         */
        explicit SceneHierarchyTool(Application * app);

        /**
         * @brief Virtual destructor
         */
        virtual ~SceneHierarchyTool() = default;

        // Scene and hierarchy query methods
        nlohmann::json getSceneHierarchy() const;
        nlohmann::json getDocumentInfo() const;
        nlohmann::json getModelBoundingBox() const;
        std::vector<std::string> listAvailableFunctions() const;
    };
} // namespace gladius::mcp::tools
