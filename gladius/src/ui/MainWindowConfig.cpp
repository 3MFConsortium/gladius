// Implementation of save and load render settings

#include "MainWindow.h"

namespace gladius::ui
{    void MainWindow::saveRenderSettings()
    {
        if (!m_configManager)
            return;
            
        auto& renderSettings = m_core->getResourceContext()->getRenderingSettings();
        
        // Save rendering settings
        m_configManager->setValue("rendering", "quality", renderSettings.quality);
        
        bool enableSdfRendering = m_core->getPreviewRenderProgram()->isSdfVisualizationEnabled();
        m_configManager->setValue("rendering", "showDistanceField", enableSdfRendering);
        
        float sliceHeight = m_core->getSliceHeight();
        m_configManager->setValue("rendering", "sliceHeight", sliceHeight);
        
        // Save UI scale
        m_configManager->setValue("ui", "scale", m_mainView.getUiScale());
        
        // Save the configuration
        m_configManager->save();
    }
    
    void MainWindow::loadRenderSettings()
    {        if (!m_configManager || !m_core)
            return;
            
        auto& renderSettings = m_core->getResourceContext()->getRenderingSettings();
        
        // Load rendering settings with defaults
        renderSettings.quality = m_configManager->getValue("rendering", "quality", 1.0f);
        
        bool enableSdfRendering = m_configManager->getValue("rendering", "showDistanceField", false);
        m_core->getPreviewRenderProgram()->setSdfVisualizationEnabled(enableSdfRendering);
        
        float sliceHeight = m_configManager->getValue("rendering", "sliceHeight", 0.0f);
        m_core->setSliceHeight(sliceHeight);
        
        // Load UI scale
        float savedUiScale = m_configManager->getValue("ui", "scale", 1.0f);
        m_mainView.setUiScale(savedUiScale);
    }
}
