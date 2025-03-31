/**
 * @file Writer3mf.h
 * @brief This file contains the declaration of the Writer3mf class, which is responsible for saving
 * documents and functions to 3MF files.
 */

#pragma once

#include "EventLogger.h"
#include "nodes/types.h"

#include <lib3mf_implicit.hpp>

#include <filesystem>

namespace gladius
{
    class Document;

    namespace nodes
    {
        class Model;
    }
}

namespace Lib3MF
{
    class CModel;
    typedef std::shared_ptr<CModel> PModel;

    class CImplicitFunction;
    typedef std::shared_ptr<CImplicitFunction> PImplicitFunction;
}

namespace gladius::io
{

    /**
     * Converts a Matrix4x4 type to Lib3MF's sMatrix4x4 type.
     *
     * @param mat The Matrix4x4 object to be converted.
     * @return The converted sMatrix4x4 object.
     */
    Lib3MF::sMatrix4x4 convertMatrix4x4(gladius::nodes::Matrix4x4 const & mat);

    /**
     * @brief Represents a pointer to an Implicit Function in the Lib3MF library.
     */
    Lib3MF::PImplicitFunction findExistingFunction(Lib3MF::PModel model3mf,
                                                   gladius::nodes::Model const & function);

    /**
     * @class Writer3mf
     * @brief The Writer3mf class is responsible for saving documents and functions to 3MF files.
     */
    class Writer3mf
    {
      public:
        /**
         * @brief Constructs a Writer3mf object with the specified logger.
         * @param logger The logger to be used for logging events.
         */
        explicit Writer3mf(events::SharedLogger logger);

        /**
         * @brief Saves the document to a 3MF file with the specified filename.
         * @param filename The path to the output 3MF file.
         * @param doc The document to be saved.
         */
        void save(std::filesystem::path const & filename,
                  Document const & doc,
                  bool writeThumbnail = true);

        /**
         * @brief Saves the function to a 3MF file with the specified filename.
         * @param filename The path to the output 3MF file.
         * @param function The function to be saved.
         */
        void saveFunction(std::filesystem::path const & filename, gladius::nodes::Model & function);

        /**
         * @brief Updates the thumbnail of the document.
         * @param doc The document whose thumbnail will be updated.
         */
        void updateThumbnail(Document & doc);

        /**
         * @brief Updates the 3MF model based on the document.
         * @param doc The document to be updated.
         */
        void updateModel(Document const & doc);
      private:
        events::SharedLogger m_logger;
        Lib3MF::PWrapper m_wrapper{};



        /**
         * @brief Adds the function to the 3MF model.
         * @param model The model to which the function will be added.
         * @param model3mf The 3MF model.
         */
        void addFunctionTo3mf(gladius::nodes::Model & model, Lib3MF::PModel model3mf);

        /**
         * @brief Fills the function data in the 3MF model.
         * @param function The function to be filled.
         * @param model The model in which the function data will be filled.
         * @param model3mf The 3MF model.
         */
        void fillFunction(Lib3MF::PImplicitFunction function,
                          gladius::nodes::Model & model,
                          Lib3MF::PModel model3mf);

    };

    /**
     * @brief Saves the document to a 3MF file with the specified filename.
     * @param filename The path to the output 3MF file.
     * @param doc The document to be saved.
     * @param writeThumbnail Whether to write the thumbnail or not.
     */
    void saveTo3mfFile(std::filesystem::path const & filename,
                       Document const & doc,
                       bool writeThumbnail = true);

    /**
     * @brief Saves the function to a 3MF file with the specified filename.
     * @param filename The path to the output 3MF file.
     * @param function The function to be saved.
     */
    void saveFunctionTo3mfFile(std::filesystem::path const & filename,
                               gladius::nodes::Model & function);
}