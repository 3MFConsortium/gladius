/**
 * @file Writer3mfBase.cpp
 * @brief Implementation of base class for 3MF writers with common functionality
 */

#include "io/3mf/Writer3mfBase.h"

#include "Document.h"
#include "EventLogger.h"

#include <lib3mf_abi.hpp>

#include <chrono>
#include <iomanip>
#include <sstream>

namespace gladius::io
{

    Writer3mfBase::Writer3mfBase(events::SharedLogger logger)
        : m_logger(std::move(logger))
    {
        try
        {
            m_wrapper = Lib3MF::CWrapper::loadLibrary();
        }
        catch (std::exception const & e)
        {
            if (m_logger)
            {
                m_logger->addEvent({fmt::format("Failed to initialize 3MF library: {}", e.what()),
                                    events::Severity::Error});
            }
            throw std::runtime_error("Failed to initialize 3MF library");
        }
    }

    void Writer3mfBase::updateThumbnail(Document & doc, Lib3MF::PModel model3mf)
    {
        if (!model3mf)
        {
            if (m_logger)
            {
                m_logger->addEvent({"No 3MF model to update.", events::Severity::Error});
            }
            return;
        }

        try
        {
            auto image = doc.getCore()->createThumbnailPng();

            if (model3mf->HasPackageThumbnailAttachment())
            {
                model3mf->RemovePackageThumbnailAttachment();
            }
            auto thumbNail = model3mf->CreatePackageThumbnailAttachment();
            thumbNail->ReadFromBuffer(image.data);

            if (m_logger)
            {
                m_logger->addEvent(
                  {"Successfully added thumbnail to 3MF model", events::Severity::Info});
            }
        }
        catch (std::exception const & e)
        {
            if (m_logger)
            {
                m_logger->addEvent(
                  {fmt::format("Failed to add thumbnail: {}", e.what()), events::Severity::Error});
            }
        }
    }

    void Writer3mfBase::addDefaultMetadata(Lib3MF::PModel model3mf)
    {
        try
        {
            auto metaDataGroup = model3mf->GetMetaDataGroup();
            if (!metaDataGroup)
            {
                return;
            }

            // Add application metadata
            try
            {
                auto existing = metaDataGroup->GetMetaDataByKey("", "Application");
                if (!existing)
                {
                    metaDataGroup->AddMetaData("", "Application", "Gladius", "string", true);
                }
            }
            catch (...)
            {
                metaDataGroup->AddMetaData("", "Application", "Gladius", "string", true);
            }

            // Add creation date
            try
            {
                auto existing = metaDataGroup->GetMetaDataByKey("", "CreationDate");
                if (!existing)
                {
                    // Get current time in ISO 8601 format using thread-safe approach
                    auto now = std::chrono::system_clock::now();
                    auto const time_t = std::chrono::system_clock::to_time_t(now);

                    // Use thread-safe gmtime_s on Windows, gmtime_r on POSIX
                    std::tm tm{};
#ifdef _WIN32
                    gmtime_s(&tm, &time_t);
#else
                    gmtime_r(&time_t, &tm);
#endif

                    // Format using fmt for consistency with project style
                    auto timeString = fmt::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}Z",
                                                  tm.tm_year + 1900,
                                                  tm.tm_mon + 1,
                                                  tm.tm_mday,
                                                  tm.tm_hour,
                                                  tm.tm_min,
                                                  tm.tm_sec);

                    metaDataGroup->AddMetaData("", "CreationDate", timeString, "dateTime", true);
                }
            }
            catch (...)
            {
                // Ignore if we can't add creation date
            }
        }
        catch (std::exception const & e)
        {
            if (m_logger)
            {
                m_logger->addEvent({fmt::format("Failed to add default metadata: {}", e.what()),
                                    events::Severity::Warning});
            }
        }
    }

    void Writer3mfBase::copyMetadata(Document const & sourceDocument, Lib3MF::PModel targetModel)
    {
        auto sourceModel = sourceDocument.get3mfModel();
        if (!sourceModel)
        {
            return; // No source metadata to copy
        }

        try
        {
            auto sourceMetaDataGroup = sourceModel->GetMetaDataGroup();
            auto targetMetaDataGroup = targetModel->GetMetaDataGroup();

            if (!sourceMetaDataGroup || !targetMetaDataGroup)
            {
                return;
            }

            Lib3MF_uint32 count = sourceMetaDataGroup->GetMetaDataCount();

            for (Lib3MF_uint32 i = 0; i < count; ++i)
            {
                auto sourceMetaData = sourceMetaDataGroup->GetMetaData(i);
                if (!sourceMetaData)
                {
                    continue;
                }

                std::string namespace_ = sourceMetaData->GetNameSpace();
                std::string name = sourceMetaData->GetName();
                std::string value = sourceMetaData->GetValue();
                std::string type = sourceMetaData->GetType();
                bool preserve = sourceMetaData->GetMustPreserve();

                // Skip if metadata already exists in target
                try
                {
                    auto existing = targetMetaDataGroup->GetMetaDataByKey(namespace_, name);
                    if (existing)
                    {
                        continue; // Already exists, don't overwrite
                    }
                }
                catch (...)
                {
                    // Metadata doesn't exist, we can add it
                }

                // Add metadata to target
                try
                {
                    targetMetaDataGroup->AddMetaData(namespace_, name, value, type, preserve);

                    if (m_logger)
                    {
                        m_logger->addEvent(
                          {fmt::format("Copied metadata: {}:{} = {}", namespace_, name, value),
                           events::Severity::Info});
                    }
                }
                catch (std::exception const & e)
                {
                    if (m_logger)
                    {
                        m_logger->addEvent(
                          {fmt::format(
                             "Failed to copy metadata {}:{}: {}", namespace_, name, e.what()),
                           events::Severity::Warning});
                    }
                }
            }
        }
        catch (std::exception const & e)
        {
            if (m_logger)
            {
                m_logger->addEvent(
                  {fmt::format("Error copying metadata: {}", e.what()), events::Severity::Warning});
            }
        }
    }

} // namespace gladius::io
