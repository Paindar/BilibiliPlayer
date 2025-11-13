#include "search_page.h"
#include "ui_search_page.h"
#include <QUrlQuery>
#include <QMenu>
#include <QActionGroup>
#include <QAction>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QPixmap>
#include <QFile>
#include <QTimer>
#include <log/log_manager.h>
#include <audio/audio_player_controller.h>
#include <network/network_manager.h>
#include <playlist/playlist_manager.h>
#include <manager/application_context.h>
#include <util/md5.h>
#include <config/config_manager.h>

// ==================== SearchResultItemWidget Implementation ====================

SearchResultItemWidget::SearchResultItemWidget(const network::SearchResult& result, QWidget* parent)
    : QWidget(parent)
    , m_result(result)
{
    // Create main horizontal layout
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(12);
    
    // Create cover image label (left section)
    m_coverLabel = new QLabel(this);
    m_coverLabel->setFixedSize(64, 64);
    m_coverLabel->setScaledContents(false);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setStyleSheet("QLabel { background-color: #3a3a3a; border: 1px solid #555555; }");
    m_coverLabel->setText("加载中...");
    mainLayout->addWidget(m_coverLabel);
    
    // Create info layout (right section)
    QVBoxLayout* infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(4);
    
    // Title label
    m_titleLabel = new ScrollingLabel(this);
    m_titleLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #ffffff; }");
    m_titleLabel->setText(result.title);
    infoLayout->addWidget(m_titleLabel);
    
    // Uploader and Platform label (combined)
    m_uploaderLabel = new ScrollingLabel(this);
    m_uploaderLabel->setStyleSheet("QLabel { font-size: 12px; color: #aaaaaa; }");
    QString platformName;
    switch (result.platform) {
        case network::SupportInterface::Bilibili:
            platformName = "Bilibili";
            break;
        default:
            platformName = "未知";
            break;
    }
    QString uploaderText = QString("上传者: %1 | 平台: %2").arg(result.uploader).arg(platformName);
    m_uploaderLabel->setText(uploaderText);
    infoLayout->addWidget(m_uploaderLabel);
    
    // Description label
    m_descriptionLabel = new QLabel(this);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setStyleSheet("QLabel { font-size: 11px; color: #888888; }");
    m_descriptionLabel->setText(result.description);
    m_descriptionLabel->setMaximumHeight(40);
    infoLayout->addWidget(m_descriptionLabel);
    
    infoLayout->addStretch();
    
    mainLayout->addLayout(infoLayout, 1);
    
    setLayout(mainLayout);
    
    // Set minimum height for the widget
    setMinimumHeight(80);
}

void SearchResultItemWidget::setCoverImage(const QPixmap& pixmap)
{
    if (!pixmap.isNull()) {
        m_coverLabel->setPixmap(pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_coverLabel->setText("无封面");
    }
}

void SearchResultItemWidget::updateResult(const network::SearchResult& result)
{
    m_result = result;
    m_titleLabel->setText(result.title);
    
    QString platformName;
    switch (result.platform) {
        case network::SupportInterface::Bilibili:
            platformName = "Bilibili";
            break;
        default:
            platformName = "未知";
            break;
    }
    m_uploaderLabel->setText(QString("上传者: %1 | 平台: %2").arg(result.uploader).arg(platformName));
    m_descriptionLabel->setText(result.description);
}

// ==================== SearchPage Implementation ====================


SearchPage::SearchPage(QWidget *parent)
    : QWidget(parent)
    , INavigablePage(SEARCH_PAGE_TYPE)
    , ui(new Ui_SearchPage)
    , m_currentScope(SearchScope::All)
    , m_scopeMenu(nullptr)
    , m_scopeActionGroup(nullptr)
{
    ui->setupUi(this);
    setupScopeMenu();
    
    // Connect signals
    connect(ui->searchButton, &QPushButton::clicked, this, &SearchPage::onSearchButtonClicked);
    connect(ui->searchInput, &QLineEdit::returnPressed, this, &SearchPage::onSearchInputReturnPressed);
    connect(ui->scopeButton, &QPushButton::clicked, this, &SearchPage::onScopeButtonClicked);
    connect(ui->resultsList, &QListWidget::itemDoubleClicked, this, &SearchPage::onResultItemDoubleClicked);
    
    // Connect NetworkManager signals
    connect(NETWORK_MANAGER, &network::NetworkManager::searchCompleted,
            this, &SearchPage::onSearchCompleted);
    connect(NETWORK_MANAGER, &network::NetworkManager::searchFailed,
            this, &SearchPage::onSearchFailed);
    connect(NETWORK_MANAGER, &network::NetworkManager::searchProgress,
            this, &SearchPage::onSearchProgress);
    
    // Show empty state initially
    showEmptyState();
}

SearchPage::~SearchPage()
{
    delete ui;
}

void SearchPage::setSearchText(const QString& searchText)
{
    m_currentSearchText = searchText;
    ui->searchInput->setText(searchText);
    
    if (!searchText.isEmpty()) {
        performSearch();
    }
}

QString SearchPage::getNavigationState() const
{
    QUrlQuery query;
    query.addQueryItem("query", m_currentSearchText);
    query.addQueryItem("scope", QString::number(static_cast<int>(m_currentScope)));
    return QString("%1?%2").arg(SEARCH_PAGE_TYPE).arg(query.toString());
}

void SearchPage::restoreFromState(const QString& state)
{
    QUrl url(state);
    QUrlQuery query(url);
    
    QString searchText = query.queryItemValue("query");
    int scope = query.queryItemValue("scope").toInt();
    
    m_currentScope = static_cast<SearchScope>(scope);
    setSearchText(searchText);
}

void SearchPage::onSearchButtonClicked()
{
    performSearch();
}

void SearchPage::onSearchInputReturnPressed()
{
    performSearch();
}

void SearchPage::onScopeButtonClicked()
{
    if (m_scopeMenu) {
        // Show menu below the button
        QPoint buttonPos = ui->scopeButton->mapToGlobal(QPoint(0, ui->scopeButton->height()));
        m_scopeMenu->exec(buttonPos);
    }
}

void SearchPage::onScopeSelectionChanged()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action) {
        m_currentScope = static_cast<SearchScope>(action->data().toInt());
        
        // Update button tooltip
        QString scopeName = (m_currentScope == SearchScope::All) ? "全部" : "Bilibili";
        ui->scopeButton->setToolTip(QString("搜索范围: %1").arg(scopeName));
    }
}

void SearchPage::performSearch()
{
    QString keyword = ui->searchInput->text().trimmed();
    m_currentSearchText = keyword;
    
    if (keyword.isEmpty()) {
        showEmptyState();
        return;
    }
    
    // Emit search signal for any listeners
    emit searchRequested(keyword, m_currentScope);
    
    // Show loading state
    showResults();
    ui->resultsList->clear();
    // ui->resultsList->addItem(QString("正在搜索: \"%1\"...").arg(keyword));
    
    // Disable search button to prevent multiple requests
    ui->searchButton->setEnabled(false);
    ui->searchInput->setEnabled(false);
    
    
    if (m_currentScope == SearchScope::Bilibili) {
        // Use Bilibili search only
        NETWORK_MANAGER->executeMultiSourceSearch(keyword, network::SupportInterface::Bilibili, 20);
    } else {
        // For "All" scope, search all available sources
        NETWORK_MANAGER->executeMultiSourceSearch(keyword, network::SupportInterface::All, 20);
    }
}

void SearchPage::showEmptyState()
{
    ui->contentStack->setCurrentIndex(0); // Empty page
}

void SearchPage::showResults()
{
    ui->contentStack->setCurrentIndex(1); // Results page
}

void SearchPage::setupScopeMenu()
{
    m_scopeMenu = new QMenu(this);
    m_scopeActionGroup = new QActionGroup(this);
    
    // Create "All" option
    QAction* allAction = new QAction("全部", this);
    allAction->setCheckable(true);
    allAction->setChecked(true);
    allAction->setData(static_cast<int>(SearchScope::All));
    
    // Create "Bilibili" option
    QAction* bilibiliAction = new QAction("Bilibili", this);
    bilibiliAction->setCheckable(true);
    bilibiliAction->setData(static_cast<int>(SearchScope::Bilibili));
    
    // Add to action group for mutual exclusion
    m_scopeActionGroup->addAction(allAction);
    m_scopeActionGroup->addAction(bilibiliAction);
    
    // Add to menu
    m_scopeMenu->addAction(allAction);
    m_scopeMenu->addAction(bilibiliAction);
    
    // Connect signals
    connect(allAction, &QAction::triggered, this, &SearchPage::onScopeSelectionChanged);
    connect(bilibiliAction, &QAction::triggered, this, &SearchPage::onScopeSelectionChanged);
    
    // Style the menu
    m_scopeMenu->setStyleSheet(R"(
        QMenu {
            background-color: #505050;
            border: 1px solid #666666;
            color: #ffffff;
            padding: 4px;
        }
        QMenu::item {
            padding: 8px 20px;
            margin: 2px;
        }
        QMenu::item:selected {
            background-color: #3498db;
        }
        QMenu::item:checked {
            background-color: #2c3e50;
        }
    )");
}

QString SearchPage::convertPlatformEnumToString(network::SupportInterface platform) const
{
    switch (platform) {
    case network::SupportInterface::Bilibili:
        return "Bilibili";
    default:
        return "未知";
    }
}

void SearchPage::onSearchCompleted(const QString& keyword)
{
    
    // Only process results for current search
    if (keyword != m_currentSearchText) {
        return;
    }
    // Re-enable UI
    ui->searchButton->setEnabled(true);
    ui->searchInput->setEnabled(true);
    
}

void SearchPage::onSearchFailed(const QString& keyword, const QString& errorMessage)
{
    // Re-enable UI
    ui->searchButton->setEnabled(true);
    ui->searchInput->setEnabled(true);
    
    // Only process errors for current search
    if (keyword != m_currentSearchText) {
        return;
    }
    
    // Show error message
    showResults();
    ui->resultsList->clear();
    ui->resultsList->addItem(QString("搜索失败: %1").arg(errorMessage));
    ui->resultsList->addItem("请检查网络连接或稍后重试");
}

void SearchPage::onSearchProgress(const QString& keyword, const QList<network::SearchResult>& results)
{
    // Only process progress for current search
    if (keyword != m_currentSearchText) {
        return;
    }
    for (const auto& result : results) {
        // Create custom widget for this result
        SearchResultItemWidget* resultWidget = new SearchResultItemWidget(result, ui->resultsList);
        
        // Download cover image asynchronously if available
        if (!result.coverUrl.isEmpty()) {
            std::string coverName = util::md5Hash(result.coverUrl.toStdString()) + ".jpg";
            std::string coverPath = CONFIG_MANAGER->getCoverCacheDirectory().toStdString() + "/" + coverName;
            QString coverPathQt = QString::fromStdString(coverPath);
            
            // Check if cover already exists
            if (QFile::exists(coverPathQt)) {
                QPixmap pix(coverPathQt);
                resultWidget->setCoverImage(pix);
            } else {
                // Download cover asynchronously
                auto future = NETWORK_MANAGER->downloadAsync(network::SupportInterface::Bilibili,
                                                             result.coverUrl,
                                                             coverPathQt);
                
                // Use a lambda to check when download completes and update the widget
                // Note: This is a simple polling approach; a QFutureWatcher would be more Qt-idiomatic
                QTimer::singleShot(100, this, [resultWidget, coverPathQt]() {
                    if (QFile::exists(coverPathQt)) {
                        QPixmap pix(coverPathQt);
                        if (!pix.isNull()) {
                            resultWidget->setCoverImage(pix);
                        }
                    }
                });
            }
        }
        
        // Create list item and set the custom widget
        QListWidgetItem* item = new QListWidgetItem(ui->resultsList);
        item->setData(Qt::UserRole, QVariant::fromValue(result));
        item->setSizeHint(resultWidget->sizeHint());
        
        ui->resultsList->addItem(item);
        ui->resultsList->setItemWidget(item, resultWidget);
    }
}

void SearchPage::onResultItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) {
        return;
    }

    network::SearchResult result = item->data(Qt::UserRole).value<network::SearchResult>();
    auto currentPlaylistUuid = PLAYLIST_MANAGER->getCurrentPlaylist();
    if (currentPlaylistUuid.isNull()) {
        LOG_WARN("No current playlist selected");
        return;
    }
    auto playlistOpt = PLAYLIST_MANAGER->getPlaylist(currentPlaylistUuid);
    if (!playlistOpt.has_value()) {
        LOG_WARN("No playlist found for current playlist UUID");
        return;
    }
    playlist::SongInfo song{
        .title = result.title,
        .uploader = result.uploader,
        .platform = static_cast<int>(result.platform),
        .duration = result.duration,
        .coverName = result.coverImg,  // Use coverImg from search result
        .args = result.interfaceData
    };
    bool success = PLAYLIST_MANAGER->addSongToPlaylist(song, currentPlaylistUuid);
    if (!success) {
        LOG_ERROR("Failed to add song to playlist");
    }
    
    // Download cover image if it doesn't exist in tmp/cover
    QString coverPath = CONFIG_MANAGER->getCoverCacheDirectory() + "/" + result.coverImg;
    if (!QFile::exists(coverPath)) {
        LOG_INFO("Downloading cover image: {} -> {}", result.coverUrl.toStdString(), coverPath.toStdString());
        NETWORK_MANAGER->downloadAsync(network::SupportInterface::Bilibili,
                                           result.coverUrl,
                                           coverPath);
    }
    
    auto playlist = playlistOpt.value();
    if (playlist.coverUri.isEmpty()) {
        playlist.coverUri = coverPath;
        PLAYLIST_MANAGER->updatePlaylist(playlist, QUuid());
    }
    
    if (AUDIO_PLAYER_CONTROLLER) {
        if (!AUDIO_PLAYER_CONTROLLER->isPlaying()) {
            AUDIO_PLAYER_CONTROLLER->playPlaylistFromSong(currentPlaylistUuid, song);
        }
    }
}
