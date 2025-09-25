#pragma once
#include <filesystem>
#include <optional>
#include <vector>

namespace gladius::ui
{
    std::string readFileOrReturnEmpty(const std::filesystem::path & path);

    class CopyRightInfo
    {
      public:
        explicit CopyRightInfo(std::filesystem::path path);

        [[nodiscard]] const std::string & getName() const;

        [[nodiscard]] std::string getOrLoadUrl();

        [[nodiscard]] std::string getOrLoadCopyrightText();

      private:
        std::filesystem::path m_path;
        std::string m_name{m_path.filename().string()};
        std::optional<std::string> m_licenseText;
        std::optional<std::string> m_url;
    };

    class CopyRightInfoCache
    {
        using CopyRigths = std::vector<CopyRightInfo>;

      public:
        explicit CopyRightInfoCache(std::filesystem::path licenseDir);

        [[nodiscard]] CopyRigths::iterator begin();
        [[nodiscard]] CopyRigths::iterator end();

        [[nodiscard]] CopyRigths::const_iterator cbegin() const;
        [[nodiscard]] CopyRigths::const_iterator cend() const;

      private:
        std::vector<CopyRightInfo> m_licenses;
    };

    class AboutDialog
    {
      public:
        AboutDialog();
        void show();
        void hide();
        void render();

      private:
        bool m_visible = false;
        std::filesystem::path m_appDir;
        CopyRightInfoCache m_copyRightInfoCache;
        CopyRightInfo m_copyRight;
    };
}
