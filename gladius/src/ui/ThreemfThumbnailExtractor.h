#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>
#include <memory>
#include <GL/gl.h>
#include <lib3mf_implicit.hpp>
#include "../EventLogger.h"

namespace gladius::ui
{
    /**
     * @brief Class for extracting and handling thumbnails from 3MF files
     */
    class ThreemfThumbnailExtractor
    {
    public:
        /**
         * @brief Structure to hold thumbnail information
         */
        struct ThumbnailInfo
        {
            std::filesystem::path filePath;  ///< Path to the 3MF file
            std::string fileName;            ///< Name of the file (without extension)
            std::vector<unsigned char> thumbnailData; ///< Raw PNG data
            bool hasThumbnail = false;       ///< Whether the file has a thumbnail
            bool thumbnailLoaded = false;    ///< Whether the thumbnail has been loaded
            GLuint thumbnailTextureId = 0;   ///< OpenGL texture ID
            unsigned int thumbnailWidth = 0;  ///< Width of the thumbnail
            unsigned int thumbnailHeight = 0; ///< Height of the thumbnail
            std::time_t timestamp = 0;       ///< Last modified timestamp
        };

        /**
         * @brief Construct a new Threemf Thumbnail Extractor
         * 
         * @param logger Event logger for error reporting
         */
        explicit ThreemfThumbnailExtractor(events::SharedLogger logger);
        
        /**
         * @brief Destroy the Threemf Thumbnail Extractor
         */
        ~ThreemfThumbnailExtractor();

        /**
         * @brief Extract thumbnail data from a 3MF file
         * 
         * @param filePath Path to the 3MF file
         * @return std::vector<unsigned char> Raw PNG data
         */
        std::vector<unsigned char> extractThumbnail(const std::filesystem::path& filePath);
        
        /**
         * @brief Load thumbnail data for a file
         * 
         * @param info Thumbnail info to be updated
         */
        void loadThumbnail(ThumbnailInfo& info);
        
        /**
         * @brief Create an OpenGL texture from thumbnail data
         * 
         * @param info Thumbnail info containing the PNG data
         */
        void createThumbnailTexture(ThumbnailInfo& info);
        
        /**
         * @brief Release resources associated with a thumbnail
         * 
         * @param info Thumbnail info to clean up
         */
        void releaseThumbnail(ThumbnailInfo& info);
        
        /**
         * @brief Create a thumbnail info object from a file path
         * 
         * @param filePath Path to the 3MF file
         * @param timestamp Optional timestamp (defaults to file's last modified time)
         * @return ThumbnailInfo New thumbnail info object
         */
        ThumbnailInfo createThumbnailInfo(
            const std::filesystem::path& filePath, 
            std::time_t timestamp = 0);

    private:
        events::SharedLogger m_logger; ///< Logger for error reporting
        Lib3MF::PWrapper m_wrapper;    ///< 3MF library wrapper
    };
}
