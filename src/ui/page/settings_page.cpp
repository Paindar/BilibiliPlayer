#include "settings_page.h"
#include "ui_settings_page.h"
#include "../../config/config_manager.h"
#include "../../manager/application_context.h"
#include "../../log/log_manager.h"
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QColorDialog>
#include <QStandardPaths>
#include <QDir>

SettingsPage::SettingsPage(QWidget* parent)
    : QWidget(parent)
    , INavigablePage(SettingsPage::SETTINGS_PAGE_TYPE)
    , ui(new Ui::SettingsPage)
    , m_configManager(nullptr)
    , m_settingsModified(false)
    , m_translator(new QTranslator(this))
{
    ui->setupUi(this);
    m_configManager = ApplicationContext::instance().configManager();
    setupUI();
    connectSignals();
    loadSettings();
}

SettingsPage::~SettingsPage()
{
    delete ui;
}

void SettingsPage::onPageActivated()
{
    LOG_DEBUG("Settings page activated");
    loadSettings(); // Refresh settings when page is shown
}

void SettingsPage::onPageDeactivated()
{
    LOG_DEBUG("Settings page deactivated");
    if (m_settingsModified) {
        auto result = QMessageBox::question(this, 
                                          tr("Unsaved Changes"),
                                          tr("You have unsaved changes. Do you want to save them?"),
                                          QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        
        if (result == QMessageBox::Save) {
            saveSettings();
        } else if (result == QMessageBox::Cancel) {
            // TODO: Prevent navigation if user cancels
            return;
        }
    }
}

void SettingsPage::setupUI()
{
    // Initial accent color (will be loaded from config later)
    m_selectedAccentColor = QColor(64, 158, 255); // Default blue
    updateAccentColorButton();
}

void SettingsPage::connectSignals()
{
    // General settings
    connect(ui->themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onSettingsChanged);
    connect(ui->languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPage::onSettingsChanged);
    connect(ui->accentColorButton, &QPushButton::clicked,
            this, &SettingsPage::selectAccentColor);
    
    // Network settings
    connect(ui->timeoutSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsPage::onSettingsChanged);
    connect(ui->proxyUrlEdit, &QLineEdit::textChanged,
            this, &SettingsPage::onSettingsChanged);
    
    // Path settings
    connect(ui->playlistDirButton, &QPushButton::clicked,
            this, &SettingsPage::selectPlaylistDirectory);
    connect(ui->themeDirButton, &QPushButton::clicked,
            this, &SettingsPage::selectThemeDirectory);
    connect(ui->audioCacheDirButton, &QPushButton::clicked,
            this, &SettingsPage::selectAudioCacheDirectory);
    connect(ui->coverCacheDirButton, &QPushButton::clicked,
            this, &SettingsPage::selectCoverCacheDirectory);
    connect(ui->platformDirButton, &QPushButton::clicked,
            this, &SettingsPage::selectPlatformDirectory);
    
    // Playlist settings
    connect(ui->autoSaveCheckBox, &QCheckBox::toggled,
            this, &SettingsPage::onSettingsChanged);
    connect(ui->autoSaveIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsPage::onSettingsChanged);
    
    // Advanced settings
    connect(ui->exportButton, &QPushButton::clicked,
            this, &SettingsPage::exportSettings);
    connect(ui->importButton, &QPushButton::clicked,
            this, &SettingsPage::importSettings);
    
    // Action buttons
    connect(ui->applyButton, &QPushButton::clicked,
            this, &SettingsPage::applySettings);
    connect(ui->resetButton, &QPushButton::clicked,
            this, &SettingsPage::resetSettings);
    connect(ui->okButton, &QPushButton::clicked,
            this, [this]() { applySettings(); });
    connect(ui->cancelButton, &QPushButton::clicked,
            this, [this]() { loadSettings(); });
    
    // Config manager signals
    if (m_configManager) {
        connect(m_configManager, &ConfigManager::themeChanged,
                this, &SettingsPage::onThemeChanged);
        connect(m_configManager, &ConfigManager::languageChanged,
                this, &SettingsPage::onLanguageChanged);
        connect(m_configManager, &ConfigManager::accentColorChanged,
                this, &SettingsPage::onAccentColorChanged);
    }
}

void SettingsPage::loadSettings()
{
    if (!m_configManager) {
        LOG_ERROR("ConfigManager not available");
        return;
    }
    
    LOG_DEBUG("Loading settings into UI");
    
    // Block signals to prevent triggering change events
    blockSignals(true);
    
    // General settings
    QString theme = m_configManager->getTheme();
    if (theme == "light") ui->themeCombo->setCurrentIndex(0);
    else if (theme == "dark") ui->themeCombo->setCurrentIndex(1);
    else ui->themeCombo->setCurrentIndex(2); // system
    
    QString language = m_configManager->getLanguage();
    if (language == "en_US" || language == "en") ui->languageCombo->setCurrentIndex(0);
    else if (language == "zh_CN") ui->languageCombo->setCurrentIndex(1);
    else ui->languageCombo->setCurrentIndex(0); // Default to English
    
    m_selectedAccentColor = m_configManager->getAccentColor();
    updateAccentColorButton();
    
    // Network settings
    ui->timeoutSpinBox->setValue(m_configManager->getNetworkTimeout());
    ui->proxyUrlEdit->setText(m_configManager->getProxyUrl());
    
    // Path settings
    updateDirectoryLabels();
    
    // Playlist settings
    ui->autoSaveCheckBox->setChecked(m_configManager->getAutoSaveEnabled());
    ui->autoSaveIntervalSpinBox->setValue(m_configManager->getAutoSaveInterval());
    
    blockSignals(false);
    m_settingsModified = false;
}

void SettingsPage::saveSettings()
{
    if (!m_configManager) {
        LOG_ERROR("ConfigManager not available");
        return;
    }
    
    LOG_DEBUG("Saving settings from UI");
    
    // General settings
    QStringList themes = {"light", "dark", "system"};
    m_configManager->setTheme(themes[ui->themeCombo->currentIndex()]);
    
    QStringList languages = {"en_US", "zh_CN"};
    QString selectedLanguage = languages[ui->languageCombo->currentIndex()];
    m_configManager->setLanguage(selectedLanguage);
    
    // Apply language change immediately without restart
    applyLanguage(selectedLanguage);
    
    m_configManager->setAccentColor(m_selectedAccentColor);
    
    // Network settings
    m_configManager->setNetworkTimeout(ui->timeoutSpinBox->value());
    m_configManager->setProxyUrl(ui->proxyUrlEdit->text());
    
    // Playlist settings
    m_configManager->setAutoSaveEnabled(ui->autoSaveCheckBox->isChecked());
    m_configManager->setAutoSaveInterval(ui->autoSaveIntervalSpinBox->value());
    
    // Save to file
    m_configManager->saveToFile();
    
    m_settingsModified = false;
    LOG_INFO("Settings saved successfully");
}

void SettingsPage::resetToDefaults()
{
    auto result = QMessageBox::question(this,
                                      tr("Reset Settings"),
                                      tr("Are you sure you want to reset all settings to defaults? This cannot be undone."),
                                      QMessageBox::Yes | QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        // This would require adding a resetToDefaults method to ConfigManager
        LOG_WARN("Reset to defaults not yet implemented in ConfigManager");
        QMessageBox::information(this, tr("Not Implemented"), 
                               tr("Reset to defaults functionality will be implemented in a future version."));
    }
}

void SettingsPage::onSettingsChanged()
{
    m_settingsModified = true;
}

void SettingsPage::onThemeChanged()
{
    loadSettings(); // Reload to reflect external changes
}

void SettingsPage::onLanguageChanged()
{
    loadSettings(); // Reload to reflect external changes
}

void SettingsPage::onAccentColorChanged()
{
    loadSettings(); // Reload to reflect external changes
}

void SettingsPage::selectPlaylistDirectory()
{
    QString currentDir = m_configManager->getAbsolutePath(m_configManager->getPlaylistDirectory());
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Playlist Directory"), currentDir);
    
    if (!dir.isEmpty()) {
        // Convert to relative path
        QString relativePath = QDir(m_configManager->getWorkspaceDirectory()).relativeFilePath(dir);
        if (m_configManager->validateRelativePath(relativePath)) {
            m_configManager->setPlaylistDirectory(relativePath);
            updateDirectoryLabels();
            onSettingsChanged();
        } else {
            QMessageBox::warning(this, tr("Invalid Directory"), 
                               tr("The selected directory is outside the workspace or invalid."));
        }
    }
}

void SettingsPage::selectThemeDirectory()
{
    QString currentDir = m_configManager->getAbsolutePath(m_configManager->getThemeDirectory());
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Theme Directory"), currentDir);
    
    if (!dir.isEmpty()) {
        QString relativePath = QDir(m_configManager->getWorkspaceDirectory()).relativeFilePath(dir);
        if (m_configManager->validateRelativePath(relativePath)) {
            m_configManager->setThemeDirectory(relativePath);
            updateDirectoryLabels();
            onSettingsChanged();
        } else {
            QMessageBox::warning(this, tr("Invalid Directory"), 
                               tr("The selected directory is outside the workspace or invalid."));
        }
    }
}

void SettingsPage::selectAudioCacheDirectory()
{
    QString currentDir = m_configManager->getAbsolutePath(m_configManager->getAudioCacheDirectory());
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Audio Cache Directory"), currentDir);
    
    if (!dir.isEmpty()) {
        QString relativePath = QDir(m_configManager->getWorkspaceDirectory()).relativeFilePath(dir);
        if (m_configManager->validateRelativePath(relativePath)) {
            m_configManager->setAudioCacheDirectory(relativePath);
            updateDirectoryLabels();
            onSettingsChanged();
        } else {
            QMessageBox::warning(this, tr("Invalid Directory"), 
                               tr("The selected directory is outside the workspace or invalid."));
        }
    }
}

void SettingsPage::selectCoverCacheDirectory()
{
    QString currentDir = m_configManager->getAbsolutePath(m_configManager->getCoverCacheDirectory());
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Cover Cache Directory"), currentDir);
    
    if (!dir.isEmpty()) {
        QString relativePath = QDir(m_configManager->getWorkspaceDirectory()).relativeFilePath(dir);
        if (m_configManager->validateRelativePath(relativePath)) {
            m_configManager->setCoverCacheDirectory(relativePath);
            updateDirectoryLabels();
            onSettingsChanged();
        } else {
            QMessageBox::warning(this, tr("Invalid Directory"), 
                               tr("The selected directory is outside the workspace or invalid."));
        }
    }
}

void SettingsPage::selectPlatformDirectory()
{
    QString currentDir = m_configManager->getAbsolutePath(m_configManager->getPlatformDirectory());
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Platform Directory"), currentDir);
    
    if (!dir.isEmpty()) {
        QString relativePath = QDir(m_configManager->getWorkspaceDirectory()).relativeFilePath(dir);
        if (m_configManager->validateRelativePath(relativePath)) {
            m_configManager->setPlatformDirectory(relativePath);
            updateDirectoryLabels();
            onSettingsChanged();
        } else {
            QMessageBox::warning(this, tr("Invalid Directory"), 
                               tr("The selected directory is outside the workspace or invalid."));
        }
    }
}

void SettingsPage::selectAccentColor()
{
    QColor color = QColorDialog::getColor(m_selectedAccentColor, this, tr("Select Accent Color"));
    if (color.isValid()) {
        m_selectedAccentColor = color;
        updateAccentColorButton();
        onSettingsChanged();
    }
}

void SettingsPage::applySettings()
{
    saveSettings();
}

void SettingsPage::resetSettings()
{
    resetToDefaults();
}

void SettingsPage::exportSettings()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                  tr("Export Settings"),
                                                  QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/bilibili_settings.json",
                                                  tr("JSON Files (*.json)"));
    
    if (!fileName.isEmpty()) {
        // TODO: Implement settings export functionality
        LOG_WARN("Settings export not yet implemented");
        QMessageBox::information(this, tr("Not Implemented"), 
                               tr("Settings export functionality will be implemented in a future version."));
    }
}

void SettingsPage::importSettings()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                  tr("Import Settings"),
                                                  QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                  tr("JSON Files (*.json)"));
    
    if (!fileName.isEmpty()) {
        // TODO: Implement settings import functionality
        LOG_WARN("Settings import not yet implemented");
        QMessageBox::information(this, tr("Not Implemented"), 
                               tr("Settings import functionality will be implemented in a future version."));
    }
}

void SettingsPage::updateAccentColorButton()
{
    QString colorStyle = QString("QPushButton { background-color: %1; border: 2px solid #ccc; border-radius: 4px; }")
                        .arg(m_selectedAccentColor.name());
    ui->accentColorButton->setStyleSheet(colorStyle);
    ui->accentColorButton->setText(m_selectedAccentColor.name().toUpper());
}

void SettingsPage::updateDirectoryLabels()
{
    if (m_configManager) {
        ui->playlistDirEdit->setText(m_configManager->getPlaylistDirectory());
        ui->themeDirEdit->setText(m_configManager->getThemeDirectory());
        ui->audioCacheDirEdit->setText(m_configManager->getAudioCacheDirectory());
        ui->coverCacheDirEdit->setText(m_configManager->getCoverCacheDirectory());
        ui->platformDirEdit->setText(m_configManager->getPlatformDirectory());
    }
}

QString SettingsPage::getNavigationState() const
{
    // Return current settings state as JSON or similar
    // For now, just return empty - could be extended later for navigation state preservation
    return QString();
}

void SettingsPage::restoreFromState(const QString& state)
{
    // Restore settings page state from navigation state
    // For now, just reload settings - could be extended later
    Q_UNUSED(state)
    loadSettings();
}

void SettingsPage::applyLanguage(const QString& languageCode)
{
    LOG_INFO("Applying language change to: {}", languageCode.toStdString());
    
    // Remove previous translator if any
    if (m_translator) {
        QApplication::removeTranslator(m_translator);
    }
    
    // Map language code to Qt translation file names
    QString translationFile;
    if (languageCode == "en_US") {
        translationFile = "BilibiliPlayer_en_US";
    } else if (languageCode == "zh_CN") {
        translationFile = "BilibiliPlayer_zh_CN";
    } else {
        translationFile = "BilibiliPlayer_en_US"; // Default to English
    }
    
    // Load translation file from resource/lang/ directory
    QString translationPath = QApplication::applicationDirPath() + "/resource/lang/";
    
    if (m_translator->load(translationFile, translationPath)) {
        QApplication::installTranslator(m_translator);
        LOG_INFO("Successfully loaded translation file: {}", translationFile.toStdString());
        
        // Retranslate the current UI
        ui->retranslateUi(this);
        
        // Emit signal to notify other components
        if (m_configManager) {
            emit m_configManager->languageChanged(languageCode);
        }
    } else {
        LOG_WARN("Failed to load translation file: {} from {}", 
                 translationFile.toStdString(), translationPath.toStdString());
        
        // If loading fails but we're switching to English, that's okay (it's the source language)
        if (languageCode == "en_US") {
            LOG_INFO("English is the source language, no translation file needed");
            // Still retranslate to ensure English strings are shown
            ui->retranslateUi(this);
        }
    }
}