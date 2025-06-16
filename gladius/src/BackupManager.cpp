#include "BackupManager.h"
#include <fstream>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>

namespace gladius
{
    BackupManager::BackupManager()
    {
        // Initialize backup directory path
        m_backupDirectory = std::filesystem::temp_directory_path() / "gladius" / "backups";
        m_sessionFile = std::filesystem::temp_directory_path() / "gladius" / "session.id";
    }

    BackupManager::~BackupManager()
    {
        // Session cleanup handled automatically when process ends
    }

    void BackupManager::initialize()
    {
        ensureBackupDirectoryExists();
        m_currentSessionId = generateSessionId();
        saveCurrentSessionId();
    }

    bool BackupManager::createBackup(const std::filesystem::path& sourceFile, const std::string& originalFileName)
    {
        // Check if enough time has passed since last backup
        auto now = std::chrono::system_clock::now();
        if (m_lastBackupTime.time_since_epoch().count() > 0)
        {
            auto timeSinceLastBackup = std::chrono::duration_cast<std::chrono::minutes>(now - m_lastBackupTime);
            if (timeSinceLastBackup < m_minBackupInterval)
            {
                return false; // Skip backup, too soon
            }
        }

        try
        {
            ensureBackupDirectoryExists();
            
            std::string displayName = originalFileName.empty() ? "untitled" : originalFileName;
            std::string backupFilename = generateBackupFilename(displayName);
            std::filesystem::path backupPath = m_backupDirectory / backupFilename;

            // Copy the source file to backup location
            std::filesystem::copy_file(sourceFile, backupPath, std::filesystem::copy_options::overwrite_existing);
            
            m_lastBackupTime = now;
            return true;
        }
        catch (const std::exception& e)
        {
            // Log error but don't throw - backup failure shouldn't crash the application
            return false;
        }
    }

    std::vector<BackupInfo> BackupManager::getAvailableBackups() const
    {
        std::vector<BackupInfo> backups;

        if (!std::filesystem::exists(m_backupDirectory))
        {
            return backups;
        }

        try
        {
            for (const auto& entry : std::filesystem::directory_iterator(m_backupDirectory))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".3mf")
                {
                    BackupInfo info = parseBackupFilename(entry.path());
                    if (!info.filePath.empty())
                    {
                        backups.push_back(info);
                    }
                }
            }

            // Sort by timestamp (newest first)
            std::sort(backups.begin(), backups.end());
        }
        catch (const std::exception& e)
        {
            // Return empty list on error
        }

        return backups;
    }

    bool BackupManager::hasPreviousSessionBackups() const
    {
        auto backups = getAvailableBackups();
        for (const auto& backup : backups)
        {
            if (backup.isFromPreviousSession)
            {
                return true;
            }
        }
        return false;
    }

    std::filesystem::path BackupManager::getBackupDirectory() const
    {
        return m_backupDirectory;
    }

    void BackupManager::cleanupOldBackups(size_t maxBackupsToKeep)
    {
        auto backups = getAvailableBackups();
        
        if (backups.size() <= maxBackupsToKeep)
        {
            return;
        }

        // Remove oldest backups beyond the limit
        for (size_t i = maxBackupsToKeep; i < backups.size(); ++i)
        {
            try
            {
                std::filesystem::remove(backups[i].filePath);
            }
            catch (const std::exception& e)
            {
                // Continue cleanup even if one file fails
            }
        }
    }

    std::string BackupManager::generateSessionId() const
    {
        // Generate a unique session ID using timestamp + random component
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        
        std::ostringstream oss;
        oss << timestamp << "_" << dis(gen);
        return oss.str();
    }

    void BackupManager::ensureBackupDirectoryExists()
    {
        try
        {
            if (!std::filesystem::exists(m_backupDirectory))
            {
                std::filesystem::create_directories(m_backupDirectory);
            }
        }
        catch (const std::exception& e)
        {
            // Directory creation failure will be handled when trying to save
        }
    }

    std::string BackupManager::loadPreviousSessionId() const
    {
        try
        {
            if (std::filesystem::exists(m_sessionFile))
            {
                std::ifstream file(m_sessionFile);
                std::string sessionId;
                if (std::getline(file, sessionId))
                {
                    return sessionId;
                }
            }
        }
        catch (const std::exception& e)
        {
            // Return empty string on error
        }
        return "";
    }

    void BackupManager::saveCurrentSessionId()
    {
        try
        {
            // Ensure parent directory exists
            std::filesystem::path parentDir = m_sessionFile.parent_path();
            if (!std::filesystem::exists(parentDir))
            {
                std::filesystem::create_directories(parentDir);
            }

            std::ofstream file(m_sessionFile);
            if (file.is_open())
            {
                file << m_currentSessionId << std::endl;
            }
        }
        catch (const std::exception& e)
        {
            // Session tracking failure is not critical
        }
    }

    BackupInfo BackupManager::parseBackupFilename(const std::filesystem::path& filename) const
    {
        BackupInfo info;
        info.filePath = filename;

        // Expected format: YYYYMMDD_HHMMSS_sessionid_originalname.3mf
        std::string name = filename.stem().string();
        
        // Use regex to parse the filename
        std::regex pattern(R"((\d{8})_(\d{6})_([^_]+)_(.+))");
        std::smatch match;
        
        if (std::regex_match(name, match, pattern))
        {
            try
            {
                // Parse date and time
                std::string dateStr = match[1].str();
                std::string timeStr = match[2].str();
                std::string sessionId = match[3].str();
                std::string originalName = match[4].str();

                // Convert to time_point
                std::tm tm = {};
                std::istringstream ss(dateStr + timeStr);
                ss >> std::get_time(&tm, "%Y%m%d%H%M%S");
                
                if (!ss.fail())
                {
                    auto time_t_val = std::mktime(&tm);
                    info.timestamp = std::chrono::system_clock::from_time_t(time_t_val);
                    info.originalFileName = originalName;
                    info.isFromPreviousSession = (sessionId != m_currentSessionId);
                    
                    return info;
                }
            }
            catch (const std::exception& e)
            {
                // Return empty info on parse error
            }
        }

        // Try legacy pattern for old backup files that still have "backup_" prefix
        std::regex legacyPattern(R"(backup_(\d{8})_(\d{6})_([^_]+)_(.+))");
        if (std::regex_match(name, match, legacyPattern))
        {
            try
            {
                // Parse date and time
                std::string dateStr = match[1].str();
                std::string timeStr = match[2].str();
                std::string sessionId = match[3].str();
                std::string originalName = match[4].str();

                // Convert to time_point
                std::tm tm = {};
                std::istringstream ss(dateStr + timeStr);
                ss >> std::get_time(&tm, "%Y%m%d%H%M%S");
                
                if (!ss.fail())
                {
                    auto time_t_val = std::mktime(&tm);
                    info.timestamp = std::chrono::system_clock::from_time_t(time_t_val);
                    info.originalFileName = originalName;
                    info.isFromPreviousSession = (sessionId != m_currentSessionId);
                    
                    return info;
                }
            }
            catch (const std::exception& e)
            {
                // Return empty info on parse error
            }
        }

        // Try fallback pattern for very old legacy backups
        if (name == "backup")
        {
            try
            {
                auto ftime = std::filesystem::last_write_time(filename);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                
                info.timestamp = sctp;
                info.originalFileName = "legacy_backup";
                info.isFromPreviousSession = true; // Assume legacy backups are from previous sessions
                
                return info;
            }
            catch (const std::exception& e)
            {
                // Return empty info on error
            }
        }

        return {}; // Return empty info if parsing failed
    }

    std::string BackupManager::generateBackupFilename(const std::string& originalFileName) const
    {
        // Generate filename: YYYYMMDD_HHMMSS_sessionid_originalname.3mf
        auto now = std::chrono::system_clock::now();
        auto time_t_val = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t_val);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%d_%H%M%S")
            << "_" << m_currentSessionId
            << "_" << originalFileName
            << ".3mf";

        return oss.str();
    }
}
