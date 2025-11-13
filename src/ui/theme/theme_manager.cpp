#include "theme_manager.h"
#include <log/log_manager.h>
#include <config/config_manager.h>
#include <json/json.h>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <fstream>
#include <sstream>

// ========== Theme Implementation ==========

bool Theme::isValid() const
{
    return !name.isEmpty() && 
           fontPrimaryColor.isValid() && 
           buttonNormalColor.isValid();
}

QMap<QString, QVariant> Theme::toMap() const
{
    QMap<QString, QVariant> map;
    map["name"] = name;
    map["description"] = description;
    map["backgroundImagePath"] = backgroundImagePath;
    map["backgroundMode"] = backgroundMode;
    map["backgroundColor"] = backgroundColor.name();
    map["buttonNormalColor"] = buttonNormalColor.name();
    map["buttonHoverColor"] = buttonHoverColor.name();
    map["buttonPressedColor"] = buttonPressedColor.name();
    map["buttonDisabledColor"] = buttonDisabledColor.name();
    map["buttonBorderColor"] = buttonBorderColor.name();
    map["fontPrimaryColor"] = fontPrimaryColor.name();
    map["fontSecondaryColor"] = fontSecondaryColor.name();
    map["fontDisabledColor"] = fontDisabledColor.name();
    map["fontLinkColor"] = fontLinkColor.name();
    map["borderColor"] = borderColor.name();
    map["accentColor"] = accentColor.name();
    map["selectionColor"] = selectionColor.name();
    map["windowBackgroundColor"] = windowBackgroundColor.name();
    map["panelBackgroundColor"] = panelBackgroundColor.name();
    return map;
}

Theme Theme::fromMap(const QMap<QString, QVariant>& map)
{
    Theme theme;
    theme.name = map.value("name", "Unnamed").toString();
    theme.description = map.value("description", "").toString();
    theme.backgroundImagePath = map.value("backgroundImagePath", "").toString();
    theme.backgroundMode = map.value("backgroundMode", "none").toString();
    theme.backgroundColor = QColor(map.value("backgroundColor", "#ffffff").toString());
    theme.buttonNormalColor = QColor(map.value("buttonNormalColor", "#e0e0e0").toString());
    theme.buttonHoverColor = QColor(map.value("buttonHoverColor", "#d0d0d0").toString());
    theme.buttonPressedColor = QColor(map.value("buttonPressedColor", "#c0c0c0").toString());
    theme.buttonDisabledColor = QColor(map.value("buttonDisabledColor", "#f0f0f0").toString());
    theme.buttonBorderColor = QColor(map.value("buttonBorderColor", "#a0a0a0").toString());
    theme.fontPrimaryColor = QColor(map.value("fontPrimaryColor", "#000000").toString());
    theme.fontSecondaryColor = QColor(map.value("fontSecondaryColor", "#606060").toString());
    theme.fontDisabledColor = QColor(map.value("fontDisabledColor", "#a0a0a0").toString());
    theme.fontLinkColor = QColor(map.value("fontLinkColor", "#0066cc").toString());
    theme.borderColor = QColor(map.value("borderColor", "#cccccc").toString());
    theme.accentColor = QColor(map.value("accentColor", "#409eff").toString());
    theme.selectionColor = QColor(map.value("selectionColor", "#409eff").toString());
    theme.windowBackgroundColor = QColor(map.value("windowBackgroundColor", "#ffffff").toString());
    theme.panelBackgroundColor = QColor(map.value("panelBackgroundColor", "#f5f5f5").toString());
    return theme;
}

// ========== ThemeManager Implementation ==========

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
    , m_themeDirectory("resource/themes")
{
    initializeBuiltInThemes();
    m_currentTheme = getLightTheme();
}

ThemeManager::~ThemeManager()
{
}

void ThemeManager::initializeBuiltInThemes()
{
    m_builtInThemes["Light"] = getLightTheme();
    m_builtInThemes["Dark"] = getDarkTheme();
    m_builtInThemes["Custom"] = getCustomTheme();
}

Theme ThemeManager::getLightTheme()
{
    Theme theme;
    theme.name = "Light";
    theme.description = "Clean light theme with blue accents";
    theme.backgroundImagePath = "";
    theme.backgroundMode = "none";
    theme.backgroundColor = QColor("#ffffff");
    
    theme.buttonNormalColor = QColor("#e0e0e0");
    theme.buttonHoverColor = QColor("#d0d0d0");
    theme.buttonPressedColor = QColor("#c0c0c0");
    theme.buttonDisabledColor = QColor("#f0f0f0");
    theme.buttonBorderColor = QColor("#a0a0a0");
    
    theme.fontPrimaryColor = QColor("#000000");
    theme.fontSecondaryColor = QColor("#606060");
    theme.fontDisabledColor = QColor("#a0a0a0");
    theme.fontLinkColor = QColor("#0066cc");
    
    theme.borderColor = QColor("#cccccc");
    theme.accentColor = QColor("#409eff");
    theme.selectionColor = QColor("#409eff");
    
    theme.windowBackgroundColor = QColor("#ffffff");
    theme.panelBackgroundColor = QColor("#f5f5f5");
    
    return theme;
}

Theme ThemeManager::getDarkTheme()
{
    Theme theme;
    theme.name = "Dark";
    theme.description = "Dark theme with reduced eye strain";
    theme.backgroundImagePath = "";
    theme.backgroundMode = "none";
    theme.backgroundColor = QColor("#2b2b2b");
    
    theme.buttonNormalColor = QColor("#3c3c3c");
    theme.buttonHoverColor = QColor("#4c4c4c");
    theme.buttonPressedColor = QColor("#5c5c5c");
    theme.buttonDisabledColor = QColor("#2b2b2b");
    theme.buttonBorderColor = QColor("#555555");
    
    theme.fontPrimaryColor = QColor("#e0e0e0");
    theme.fontSecondaryColor = QColor("#a0a0a0");
    theme.fontDisabledColor = QColor("#606060");
    theme.fontLinkColor = QColor("#4a9eff");
    
    theme.borderColor = QColor("#555555");
    theme.accentColor = QColor("#409eff");
    theme.selectionColor = QColor("#409eff");
    
    theme.windowBackgroundColor = QColor("#2b2b2b");
    theme.panelBackgroundColor = QColor("#353535");
    
    return theme;
}

Theme ThemeManager::getCustomTheme()
{
    Theme theme = getLightTheme();
    theme.name = "Custom";
    theme.description = "User-customized theme";
    return theme;
}

bool ThemeManager::loadTheme(const QString& themeName)
{
    // Check built-in themes first
    if (m_builtInThemes.contains(themeName)) {
        m_currentTheme = m_builtInThemes[themeName];
        applyCurrentTheme();
        emit themeChanged(themeName);
        LOG_INFO("Loaded built-in theme: {}", themeName.toStdString());
        return true;
    }
    
    // Try loading from file
    QString filePath = m_themeDirectory + "/" + themeName + ".json";
    return loadThemeFromFile(filePath);
}

bool ThemeManager::loadThemeFromFile(const QString& filePath)
{
    std::ifstream file(filePath.toStdString());
    if (!file.is_open()) {
        LOG_ERROR("Failed to open theme file: {}", filePath.toStdString());
        return false;
    }
    
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, file, &root, &errs)) {
        LOG_ERROR("Failed to parse theme JSON: {}", errs);
        file.close();
        return false;
    }
    file.close();
    
    if (!root.isObject()) {
        LOG_ERROR("Invalid theme file format: {}", filePath.toStdString());
        return false;
    }
    
    // Convert JSON to QMap
    QMap<QString, QVariant> map;
    for (const auto& key : root.getMemberNames()) {
        const Json::Value& value = root[key];
        if (value.isString()) {
            map[QString::fromStdString(key)] = QString::fromStdString(value.asString());
        } else if (value.isInt()) {
            map[QString::fromStdString(key)] = value.asInt();
        } else if (value.isBool()) {
            map[QString::fromStdString(key)] = value.asBool();
        }
    }
    
    Theme theme = Theme::fromMap(map);
    if (!theme.isValid()) {
        LOG_ERROR("Invalid theme data in file: {}", filePath.toStdString());
        return false;
    }
    
    m_currentTheme = theme;
    applyCurrentTheme();
    emit themeChanged(theme.name);
    LOG_INFO("Loaded theme from file: {}", filePath.toStdString());
    return true;
}

bool ThemeManager::saveThemeToFile(const QString& filePath, const Theme& theme)
{
    if (!theme.isValid()) {
        LOG_ERROR("Cannot save invalid theme");
        return false;
    }
    
    // Ensure directory exists
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Convert theme to JSON
    QMap<QString, QVariant> map = theme.toMap();
    Json::Value root(Json::objectValue);
    
    for (auto it = map.begin(); it != map.end(); ++it) {
        QString key = it.key();
        QVariant value = it.value();
        
        if (value.metaType().id() == QMetaType::QString) {
            root[key.toStdString()] = value.toString().toStdString();
        } else if (value.metaType().id() == QMetaType::Int) {
            root[key.toStdString()] = value.toInt();
        } else if (value.metaType().id() == QMetaType::Bool) {
            root[key.toStdString()] = value.toBool();
        }
    }
    
    // Write to file
    std::ofstream file(filePath.toStdString());
    if (!file.is_open()) {
        LOG_ERROR("Failed to save theme file: {}", filePath.toStdString());
        return false;
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &file);
    file.close();
    
    LOG_INFO("Saved theme to file: {}", filePath.toStdString());
    return true;
}

void ThemeManager::applyTheme(const Theme& theme)
{
    if (!theme.isValid()) {
        LOG_WARN("Attempted to apply invalid theme");
        return;
    }
    
    m_currentTheme = theme;
    applyCurrentTheme();
}

void ThemeManager::applyCurrentTheme()
{
    QString styleSheet = generateStyleSheet(m_currentTheme);
    
    // Apply stylesheet to QApplication (not QCoreApplication)
    if (auto* app = qobject_cast<QApplication*>(QApplication::instance())) {
        app->setStyleSheet(styleSheet);
        emit themeApplied(m_currentTheme);
        LOG_INFO("Applied theme: {}", m_currentTheme.name.toStdString());
    } else {
        LOG_WARN("Cannot apply theme: QApplication instance not available");
    }
}

QString ThemeManager::generateStyleSheet(const Theme& theme) const
{
    QString qss;
    
    // Global widget styling
    qss += QString("QWidget { "
                   "background-color: %1; "
                   "color: %2; "
                   "}"
                   ).arg(theme.windowBackgroundColor.name(), theme.fontPrimaryColor.name());
    
    // Background image if specified
    if (!theme.backgroundImagePath.isEmpty() && theme.backgroundMode != "none") {
        qss += backgroundImageToStyleSheet(theme.backgroundImagePath, theme.backgroundMode);
    }
    
    // Button styling
    qss += QString("QPushButton { "
                   "background-color: %1; "
                   "color: %2; "
                   "border: 1px solid %3; "
                   "border-radius: 4px; "
                   "padding: 5px 15px; "
                   "} "
                   "QPushButton:hover { "
                   "background-color: %4; "
                   "} "
                   "QPushButton:pressed { "
                   "background-color: %5; "
                   "} "
                   "QPushButton:disabled { "
                   "background-color: %6; "
                   "color: %7; "
                   "}"
                   ).arg(theme.buttonNormalColor.name(),
                         theme.fontPrimaryColor.name(),
                         theme.buttonBorderColor.name(),
                         theme.buttonHoverColor.name(),
                         theme.buttonPressedColor.name(),
                         theme.buttonDisabledColor.name(),
                         theme.fontDisabledColor.name());
    
    // Panel/GroupBox styling
    qss += QString("QGroupBox, QFrame { "
                   "background-color: %1; "
                   "border: 1px solid %2; "
                   "border-radius: 4px; "
                   "}"
                   ).arg(theme.panelBackgroundColor.name(), theme.borderColor.name());
    
    // LineEdit/TextEdit styling
    qss += QString("QLineEdit, QTextEdit, QPlainTextEdit { "
                   "background-color: %1; "
                   "color: %2; "
                   "border: 1px solid %3; "
                   "border-radius: 2px; "
                   "padding: 3px; "
                   "selection-background-color: %4; "
                   "}"
                   ).arg(theme.windowBackgroundColor.name(),
                         theme.fontPrimaryColor.name(),
                         theme.borderColor.name(),
                         theme.selectionColor.name());
    
    // ComboBox styling
    qss += QString("QComboBox { "
                   "background-color: %1; "
                   "color: %2; "
                   "border: 1px solid %3; "
                   "border-radius: 2px; "
                   "padding: 3px; "
                   "}"
                   ).arg(theme.buttonNormalColor.name(),
                         theme.fontPrimaryColor.name(),
                         theme.borderColor.name());
    
    // TreeWidget/ListView styling
    qss += QString("QTreeWidget, QListWidget, QTableWidget { "
                   "background-color: %1; "
                   "color: %2; "
                   "border: 1px solid %3; "
                   "selection-background-color: %4; "
                   "}"
                   ).arg(theme.windowBackgroundColor.name(),
                         theme.fontPrimaryColor.name(),
                         theme.borderColor.name(),
                         theme.selectionColor.name());
    
    // Label styling
    qss += QString("QLabel { "
                   "color: %1; "
                   "background: transparent; "
                   "}"
                   ).arg(theme.fontPrimaryColor.name());
    
    return qss;
}

QString ThemeManager::backgroundImageToStyleSheet(const QString& imagePath, const QString& mode) const
{
    QString qss;
    QString imageUrl = "url(" + imagePath + ")";
    
    if (mode == "tiled") {
        qss = QString("QMainWindow { background-image: %1; background-repeat: repeat; }").arg(imageUrl);
    } else if (mode == "stretched") {
        qss = QString("QMainWindow { background-image: %1; background-position: center; background-size: cover; }").arg(imageUrl);
    } else if (mode == "centered") {
        qss = QString("QMainWindow { background-image: %1; background-position: center; background-repeat: no-repeat; }").arg(imageUrl);
    }
    
    return qss;
}

QStringList ThemeManager::getAvailableThemes() const
{
    QStringList themes = getBuiltInThemeNames();
    
    // Scan theme directory for custom themes
    QDir dir(m_themeDirectory);
    if (dir.exists()) {
        QStringList filters;
        filters << "*.json";
        QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
        for (const QFileInfo& fileInfo : files) {
            QString themeName = fileInfo.baseName();
            if (!themes.contains(themeName)) {
                themes.append(themeName);
            }
        }
    }
    
    return themes;
}

QStringList ThemeManager::getBuiltInThemeNames() const
{
    return m_builtInThemes.keys();
}

void ThemeManager::setThemeDirectory(const QString& directory)
{
    m_themeDirectory = directory;
    LOG_INFO("Theme directory set to: {}", directory.toStdString());
}
