#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace gladius
{
    /**
     * @brief Information about a backup file
     */
    struct BackupInfo
    {
        std::filesystem::path filePath;
        std::chrono::system_clock::time_point timestamp;
        std::string originalFileName;
        bool isFromPreviousSession;

        bool operator<(const BackupInfo & other) const
        {
            return timestamp > other.timestamp; // Sort by most recent first
        }
    };

    /**
     * @brief Manages automatic backups and session tracking for Gladius
     *
     * This class handles creating timestamped backups in the temp directory,
     * tracking sessions to identify backups from previous sessions, and
     * providing methods to list and restore backup files.
     */
    class BackupManager
    {
      public:
        /**
         * @brief Construct a new BackupManager object
         */
        BackupManager();

        /**
         * @brief Destroy the BackupManager object and mark session as ended
         */
        ~BackupManager();

        /**
         * @brief Initialize the backup manager and start a new session
         */
        void initialize();

        /**
         * @brief Create a backup of the specified file
         *
         * @param sourceFile Path to the file to backup
         * @param originalFileName Original filename (for display purposes)
         * @return true if backup was created successfully
         * @return false if backup creation failed
         */
        bool createBackup(const std::filesystem::path & sourceFile,
                          const std::string & originalFileName = "");

        /**
         * @brief Get all available backup files
         *
         * @return std::vector<BackupInfo> List of backup files sorted by timestamp (newest first)
         */
        std::vector<BackupInfo> getAvailableBackups() const;

        /**
         * @brief Check if there are any backups from previous sessions
         *
         * @return true if previous session backups exist
         * @return false if no previous session backups found
         */
        bool hasPreviousSessionBackups() const;

        /**
         * @brief Get the backup directory path
         *
         * @return std::filesystem::path Path to the backup directory
         */
        std::filesystem::path getBackupDirectory() const;

        /**
         * @brief Clean up old backup files (keep only the most recent N backups)
         *
         * @param maxBackupsToKeep Maximum number of backup files to retain
         */
        void cleanupOldBackups(size_t maxBackupsToKeep = 10);

      private:
        /// Path to the backup directory
        std::filesystem::path m_backupDirectory;

        /// Path to the session tracking file
        std::filesystem::path m_sessionFile;

        /// Current session ID
        std::string m_currentSessionId;

        /// Minimum time between backups (to avoid too frequent backups)
        std::chrono::minutes m_minBackupInterval{1};

        /// Last backup time
        std::chrono::system_clock::time_point m_lastBackupTime;

        /**
         * @brief Generate a unique session ID
         *
         * @return std::string Unique session identifier
         */
        std::string generateSessionId() const;

        /**
         * @brief Create the backup directory if it doesn't exist
         */
        void ensureBackupDirectoryExists();

        /**
         * @brief Load the previous session ID from the session file
         *
         * @return std::string Previous session ID, empty if none found
         */
        std::string loadPreviousSessionId() const;

        /**
         * @brief Save the current session ID to the session file
         */
        void saveCurrentSessionId();

        /**
         * @brief Parse backup filename to extract timestamp and session info
         *
         * @param filename Backup filename
         * @return BackupInfo Parsed backup information
         */
        BackupInfo parseBackupFilename(const std::filesystem::path & filename) const;

        /**
         * @brief Generate backup filename with timestamp and session info
         *
         * @param originalFileName Original filename for reference
         * @return std::string Generated backup filename
         */
        std::string generateBackupFilename(const std::string & originalFileName) const;
    };
}
