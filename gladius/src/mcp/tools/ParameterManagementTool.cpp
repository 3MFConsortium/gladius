#include "ParameterManagementTool.h"
#include "../../Application.h"
#include "../../Document.h"
#include "../../EventLogger.h"
#include <cstdint>
#include <sstream>
#include <stdexcept>

namespace gladius::mcp::tools
{

    ParameterManagementTool::ParameterManagementTool(gladius::Application * application)
        : MCPToolBase(application)
    {
    }

    bool ParameterManagementTool::setFloatParameter(uint32_t modelId,
                                                    const std::string & nodeName,
                                                    const std::string & parameterName,
                                                    float value)
    {

        try
        {
            if (!m_application)
            {
                std::string errorMsg = "Application instance is null";
                setErrorMessage(errorMsg);
                return false;
            }

            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                std::ostringstream oss;
                oss << "No current document available for setting parameter '" << parameterName
                    << "' in node '" << nodeName << "' (modelId: " << modelId << ")";
                std::string errorMsg = oss.str();
                setErrorMessage(errorMsg);
                m_application->getGlobalLogger()->logError("ParameterManagementTool: " + errorMsg);
                return false;
            }

            document->setFloatParameter(modelId, nodeName, parameterName, value);

            // Log successful parameter setting for debugging
            std::ostringstream successMsg;
            successMsg << "Successfully set float parameter '" << parameterName << "' = " << value
                       << " in node '" << nodeName << "' (modelId: " << modelId << ")";
            m_application->getGlobalLogger()->logInfo("ParameterManagementTool: " +
                                                      successMsg.str());

            return true;
        }
        catch (const std::exception & e)
        {
            std::ostringstream oss;
            oss << "Failed to set float parameter '" << parameterName << "' = " << value
                << " in node '" << nodeName << "' (modelId: " << modelId << "): " << e.what();
            std::string errorMsg = oss.str();
            setErrorMessage(errorMsg);

            if (m_application && m_application->getGlobalLogger())
            {
                m_application->getGlobalLogger()->logError("ParameterManagementTool: " + errorMsg);
            }
            return false;
        }
    }

    float ParameterManagementTool::getFloatParameter(uint32_t modelId,
                                                     const std::string & nodeName,
                                                     const std::string & parameterName)
    {

        try
        {
            if (!m_application)
            {
                std::string errorMsg = "Application instance is null";
                setErrorMessage(errorMsg);
                return 0.0f;
            }

            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                std::ostringstream oss;
                oss << "No current document available for getting parameter '" << parameterName
                    << "' from node '" << nodeName << "' (modelId: " << modelId << ")";
                std::string errorMsg = oss.str();
                setErrorMessage(errorMsg);
                m_application->getGlobalLogger()->logError("ParameterManagementTool: " + errorMsg);
                return 0.0f;
            }

            float result = document->getFloatParameter(modelId, nodeName, parameterName);

            // Log successful parameter retrieval for debugging
            std::ostringstream successMsg;
            successMsg << "Successfully retrieved float parameter '" << parameterName
                       << "' = " << result << " from node '" << nodeName
                       << "' (modelId: " << modelId << ")";
            m_application->getGlobalLogger()->logInfo("ParameterManagementTool: " +
                                                      successMsg.str());

            return result;
        }
        catch (const std::exception & e)
        {
            std::ostringstream oss;
            oss << "Failed to get float parameter '" << parameterName << "' from node '" << nodeName
                << "' (modelId: " << modelId << "): " << e.what();
            std::string errorMsg = oss.str();
            setErrorMessage(errorMsg);

            if (m_application && m_application->getGlobalLogger())
            {
                m_application->getGlobalLogger()->logError("ParameterManagementTool: " + errorMsg);
            }
            return 0.0f;
        }
    }

    bool ParameterManagementTool::setStringParameter(uint32_t modelId,
                                                     const std::string & nodeName,
                                                     const std::string & parameterName,
                                                     const std::string & value)
    {

        try
        {
            if (!m_application)
            {
                std::string errorMsg = "Application instance is null";
                setErrorMessage(errorMsg);
                if (auto logger = m_application ? m_application->getGlobalLogger() : nullptr)
                {
                    logger->logError("ParameterManagementTool: " + errorMsg);
                }
                return false;
            }

            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                std::ostringstream oss;
                oss << "No current document available for setting parameter '" << parameterName
                    << "' in node '" << nodeName << "' (modelId: " << modelId << ")";
                std::string errorMsg = oss.str();
                setErrorMessage(errorMsg);
                m_application->getGlobalLogger()->logError("ParameterManagementTool: " + errorMsg);
                return false;
            }

            document->setStringParameter(modelId, nodeName, parameterName, value);

            // Log successful parameter setting for debugging (truncate long values)
            std::ostringstream successMsg;
            std::string truncatedValue =
              value.length() > 100 ? value.substr(0, 100) + "..." : value;
            successMsg << "Successfully set string parameter '" << parameterName << "' = \""
                       << truncatedValue << "\" in node '" << nodeName << "' (modelId: " << modelId
                       << ")";
            m_application->getGlobalLogger()->logInfo("ParameterManagementTool: " +
                                                      successMsg.str());

            return true;
        }
        catch (const std::exception & e)
        {
            std::ostringstream oss;
            std::string truncatedValue =
              value.length() > 100 ? value.substr(0, 100) + "..." : value;
            oss << "Failed to set string parameter '" << parameterName << "' = \"" << truncatedValue
                << "\" in node '" << nodeName << "' (modelId: " << modelId << "): " << e.what();
            std::string errorMsg = oss.str();
            setErrorMessage(errorMsg);

            if (m_application && m_application->getGlobalLogger())
            {
                m_application->getGlobalLogger()->logError("ParameterManagementTool: " + errorMsg);
            }
            return false;
        }
    }

    std::string ParameterManagementTool::getStringParameter(uint32_t modelId,
                                                            const std::string & nodeName,
                                                            const std::string & parameterName)
    {

        try
        {
            if (!m_application)
            {
                std::string errorMsg = "Application instance is null";
                setErrorMessage(errorMsg);
                return "";
            }

            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                std::ostringstream oss;
                oss << "No current document available for getting parameter '" << parameterName
                    << "' from node '" << nodeName << "' (modelId: " << modelId << ")";
                std::string errorMsg = oss.str();
                setErrorMessage(errorMsg);
                m_application->getGlobalLogger()->logError("ParameterManagementTool: " + errorMsg);
                return "";
            }

            std::string result = document->getStringParameter(modelId, nodeName, parameterName);

            // Log successful parameter retrieval for debugging (truncate long values)
            std::ostringstream successMsg;
            std::string truncatedResult =
              result.length() > 100 ? result.substr(0, 100) + "..." : result;
            successMsg << "Successfully retrieved string parameter '" << parameterName << "' = \""
                       << truncatedResult << "\" from node '" << nodeName
                       << "' (modelId: " << modelId << ")";
            m_application->getGlobalLogger()->logInfo("ParameterManagementTool: " +
                                                      successMsg.str());

            return result;
        }
        catch (const std::exception & e)
        {
            std::ostringstream oss;
            oss << "Failed to get string parameter '" << parameterName << "' from node '"
                << nodeName << "' (modelId: " << modelId << "): " << e.what();
            std::string errorMsg = oss.str();
            setErrorMessage(errorMsg);

            if (m_application && m_application->getGlobalLogger())
            {
                m_application->getGlobalLogger()->logError("ParameterManagementTool: " + errorMsg);
            }
            return "";
        }
    }

} // namespace gladius::mcp::tools
