#include "search_page.h"
#include "ui_search_page.h"
#include <QUrlQuery>
#include <QMenu>
#include <QActionGroup>
#include <QAction>
#include <QKeyEvent>
#include <QListWidgetItem>
#include "../../network/network_manager.h"

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
    
    // Connect NetworkManager signals
    auto& networkManager = network::NetworkManager::instance();
    connect(&networkManager, &network::NetworkManager::searchCompleted,
            this, &SearchPage::onSearchCompleted);
    connect(&networkManager, &network::NetworkManager::searchFailed,
            this, &SearchPage::onSearchFailed);
    connect(&networkManager, &network::NetworkManager::searchProgress,
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
    
    // Start async search using NetworkManager
    auto& networkManager = network::NetworkManager::instance();
    
    if (m_currentScope == SearchScope::Bilibili) {
        // Use Bilibili search only
        networkManager.executeMultiSourceSearch(keyword, network::SupportInterface::Bilibili, 20);
    } else {
        // For "All" scope, search all available sources
        networkManager.executeMultiSourceSearch(keyword, network::SupportInterface::Bilibili, 20);
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

void SearchPage::onSearchProgress(const QString& keyword, const QList<SearchResult>& results)
{
    // Only process progress for current search
    if (keyword != m_currentSearchText) {
        return;
    }
    for (const auto& result : results) {
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(QString("%1\n上传者: %2 | 平台: %3 | 播放量: %4\n%5")
                      .arg(result.title)
                      .arg(result.uploader)
                      .arg(result.platform)
                      .arg(result.viewCount)
                      .arg(result.description));
        ui->resultsList->addItem(item);
    }
    
    // Update the first item with progress status
    if (ui->resultsList->count() > 0) {
        ui->resultsList->item(0)->setText(QString("搜索 \"%1\":").arg(keyword));
    }
}
