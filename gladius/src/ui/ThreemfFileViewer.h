#pragma once

#include "../Document.h"
#include "../io/3mf/ImageExtractor.h"
#include <filesystem>
#include <lib3mf_implicit.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gladius::ui
{
    /**
     * @struct ThreemfFileInfo
     * @brief Stores information about a 3MF file including its thumbnail
     */
    struct ThreemfFileInfo
    {
        std::filesystem::path filePath;           ///< Path to the 3MF file
        std::string fileName;                     ///< File name (without path)
        unsigned int thumbnailTextureId = 0;      ///< OpenGL texture ID for the thumbnail
        std::vector<unsigned char> thumbnailData; ///< Raw thumbnail data
        unsigned int thumbnailWidth = 0;          ///< Thumbnail width
        unsigned int thumbnailHeight = 0;         ///< Thumbnail height
        bool hasThumbnail = false;                ///< Whether the file has a thumbnail
        bool thumbnailLoaded = false;             ///< Whether the thumbnail has been loaded
    };

    /**
     * @class ThreemfFileViewer
     * @brief Widget that shows 3MF files in a given directory with their thumbnails
     *
     * This is a pure widget that can be embedded in any container. It does not create
     * its own window and should be placed inside another widget or window.
     */
    class ThreemfFileViewer
    {
      public:
        /**
         * @brief Constructor
         * @param logger The logger to use for events
         */
        explicit ThreemfFileViewer(events::SharedLogger logger);

        /**
         * @brief Destructor
         */
        ~ThreemfFileViewer();

        /**
         * @brief Set the directory to scan for 3MF files
         * @param directory The directory path
         */
        void setDirectory(const std::filesystem::path & directory);

        /**
         * @brief Get the current directory
         * @return The current directory path
         */
        [[nodiscard]] const std::filesystem::path & getDirectory() const
        {
            return m_directory;
        }

        /**
         * @brief Force a refresh of the directory contents
         */
        void refreshDirectory();

        /**
         * @brief Render the 3MF file viewer UI widget
         * @param doc The current document (for loading selected files)
         */
        void render(SharedDocument doc);

      private:
        /**
         * @brief Scan the directory for 3MF files
         */
        void scanDirectory();

        /**
         * @brief Load thumbnail for a 3MF file
         * @param fileInfo The file info to load the thumbnail for
         */
        void loadThumbnail(ThreemfFileInfo & fileInfo);

        /**
         * @brief Create OpenGL texture from thumbnail data
         * @param fileInfo The file info containing the thumbnail data
         */
        void createThumbnailTexture(ThreemfFileInfo & fileInfo);

        /**
         * @brief Extract thumbnail from a 3MF file using lib3mf
         * @param filePath Path to the 3MF file
         * @return Vector of bytes containing the PNG data, or empty vector if no thumbnail
         */
        std::vector<unsigned char> extractThumbnail(const std::filesystem::path & filePath);

        std::filesystem::path m_directory;    ///< Directory to scan
        std::vector<ThreemfFileInfo> m_files; ///< Found 3MF files
        bool m_needsRefresh = true;           ///< Whether the directory needs to be rescanned
        events::SharedLogger m_logger;        ///< Logger for events
        Lib3MF::PWrapper m_wrapper;           ///< lib3mf wrapper
        float m_thumbnailSize = 150.0f;       ///< Size of thumbnails in the UI
        int m_columns = 3;                    ///< Number of columns in the grid
    };

} // namespace gladius::ui
