/**
 * @file Writer3mfBase.h
 * @brief Base class for 3MF writers with common functionality
 */

#pragma once

#include "EventLogger.h"

#include <lib3mf_implicit.hpp>

#include <filesystem>

namespace gladius
{
    class Document;
} // namespace gladius

namespace gladius::io
{

    /**
     * @class Writer3mfBase
     * @brief Base class providing common functionality for 3MF writers
     */
    class Writer3mfBase
    {
      public:
        /**
         * @brief Constructs a Writer3mfBase object with the specified logger.
         * @param logger The logger to be used for logging events.
         */
        explicit Writer3mfBase(events::SharedLogger logger);

        /**
         * @brief Virtual destructor
         */
        virtual ~Writer3mfBase() = default;

      protected:
        /**
         * @brief Updates the thumbnail of the 3MF model from document data.
         * @param doc The document containing the thumbnail data.
         * @param model3mf The 3MF model to update with thumbnail.
         */
        void updateThumbnail(Document & doc, Lib3MF::PModel model3mf);

        /**
         * @brief Adds default metadata to the 3MF model.
         * @param model3mf The 3MF model to add metadata to.
         */
        void addDefaultMetadata(Lib3MF::PModel model3mf);

        /**
         * @brief Copies metadata from source document to target 3MF model.
         * @param sourceDocument The source document containing metadata.
         * @param targetModel The target 3MF model to copy metadata to.
         */
        void copyMetadata(Document const & sourceDocument, Lib3MF::PModel targetModel);

        events::SharedLogger m_logger;
        Lib3MF::PWrapper m_wrapper;
    };

} // namespace gladius::io
