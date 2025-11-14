#pragma once

#include <QWidget>
#include <QColor>
#include <QString>
#include <QTranslator>
#include <navigator/i_navigable_page.h>

class ConfigManager;
class QComboBox;
class QPushButton;
class QLineEdit;
class QSpinBox;
class QCheckBox;

QT_BEGIN_NAMESPACE
namespace Ui {
class SettingsPage;
}
QT_END_NAMESPACE

/**
 * Settings Page - provides UI for configuring all application settings
 * Groups settings logically like common applications (General, Network, Paths, etc.)
 */
class SettingsPage : public QWidget, public INavigablePage
{
    Q_OBJECT
    
public:
    static constexpr const char* SETTINGS_PAGE_TYPE = "SettingsPage";

    explicit SettingsPage(QWidget* parent = nullptr);
    ~SettingsPage();
    
    // INavigablePage interface
    QString getNavigationState() const override;
    void restoreFromState(const QString& state) override;
    
    // Page lifecycle
    void onPageActivated();
    void onPageDeactivated();
    
    // Settings management
    void loadSettings();
    void saveSettings();
    void resetToDefaults();
    
public slots:
    void onSettingsChanged();
    void onThemeChanged();
    void onLanguageChanged();
    void onAccentColorChanged();
    
private slots:
    void selectPlaylistDirectory();
    void selectThemeDirectory();
    void selectAudioCacheDirectory();
    void selectCoverCacheDirectory();
    void selectPlatformDirectory();
    void selectAccentColor();
    void applySettings();
    void resetSettings();
    void exportSettings();
    void importSettings();
    
private:
    void setupUI();
    void connectSignals();
    void updateAccentColorButton();
    void updateDirectoryLabels();
    void applyLanguage(const QString& languageCode);
    
    Ui::SettingsPage* ui;
    ConfigManager* m_configManager;
    QColor m_selectedAccentColor;
    bool m_settingsModified;
    QTranslator* m_translator;
};