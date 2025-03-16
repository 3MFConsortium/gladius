#include "AboutDialog.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "../FileSystemUtils.h"

#include "imgui.h"
#include <fmt/format.h>

#include "Widgets.h"
#include "version.h"
#include "wordwarp.h"

namespace gladius::ui
{

    std::string readFileOrReturnEmpty(std::filesystem::path const & path)
    {
        try
        {
            const std::ifstream file{path};
            if (!file)
            {
                return {};
            }

            std::stringstream sstream;
            sstream << file.rdbuf();
            return warpTextAfter(sstream.str(), 200);
        }
        catch (std::exception const & e)
        {
            std::cerr << "Loading failed: " << e.what() << "\n";
            return {};
        }
    }

    CopyRightInfo::CopyRightInfo(std::filesystem::path path)
        : m_path(std::move(path))
    {
    }

    std::string const & CopyRightInfo::getName() const
    {
        return m_name;
    }

    std::string CopyRightInfo::getOrLoadUrl()
    {
        if (m_url.has_value())
        {
            return m_url.value();
        }

        if (std::filesystem::exists(m_path / "url"))
        {
            m_url = readFileOrReturnEmpty(m_path / "url");
        }

        return m_url.value();
    }

    std::string CopyRightInfo::getOrLoadCopyrightText()
    {
        if (m_licenseText.has_value())
        {
            return m_licenseText.value_or("No url file found");
        }

        if (std::filesystem::exists(m_path / "LICENSE"))
        {
            m_licenseText = readFileOrReturnEmpty(m_path / "LICENSE");
        }

        return m_licenseText.value_or("No LICENSE file found");
    }

    CopyRightInfoCache::CopyRightInfoCache(std::filesystem::path licenseDir)
    {
        for (auto const & dir : std::filesystem::directory_iterator(licenseDir))
        {
            m_licenses.emplace_back(dir);
        }
    }

    CopyRightInfoCache::CopyRigths::const_iterator CopyRightInfoCache::cbegin() const
    {
        return m_licenses.cbegin();
    }

    CopyRightInfoCache::CopyRigths::iterator CopyRightInfoCache::end()
    {
        return m_licenses.end();
    }

    CopyRightInfoCache::CopyRigths::iterator CopyRightInfoCache::begin()
    {
        return m_licenses.begin();
    }

    CopyRightInfoCache::CopyRigths::const_iterator CopyRightInfoCache::cend() const
    {
        return m_licenses.cend();
    }

    AboutDialog::AboutDialog()
        : m_copyRightInfoCache(getAppDir() / "licenses")
        , m_copyRight(getAppDir() / "copyright")
    {
    }

    void AboutDialog::show()
    {
        m_visible = true;
    }

    void AboutDialog::hide()
    {
        m_visible = false;
    }

    void AboutDialog::render()
    {
        if (!m_visible)
        {
            return;
        }

        ImGui::Begin("About Gladius", &m_visible);

        ImGui::Spacing();
        ImGui::TextUnformatted(
          fmt::format(
            "Gladius {}.{}.{} is a viewer and editor for 3mf files using the volumetric extension.",
            Version::major,
            Version::minor,
            Version::revision)
            .c_str());

        ImGui::Spacing();

        hyperlink("https://github.com/3MFConsortium/gladius",
                  "https://github.com/3MFConsortium/gladius");
        ImGui::Spacing();

        ImGui::TextUnformatted("Copyright 3MF Consortium");

        if (ImGui::CollapsingHeader("Gladius is licensed under BSD 2-Clause License"))
        {
            ImGui::Indent();
            ImGui::TextUnformatted(m_copyRight.getOrLoadCopyrightText().c_str());
        }

        if (ImGui::CollapsingHeader("3rd-party libs and acknowledgments"))
        {
            ImGui::Indent();
            for (auto & cpInfo : m_copyRightInfoCache)
            {
                if (ImGui::CollapsingHeader(cpInfo.getName().c_str()))
                {
                    hyperlink(fmt::format("website: {}", cpInfo.getOrLoadUrl()),
                              cpInfo.getOrLoadUrl());
                    ImGui::Separator();
                    ImGui::TextUnformatted(cpInfo.getOrLoadCopyrightText().c_str());
                }
            }
        }
        ImGui::End();
    }
}
