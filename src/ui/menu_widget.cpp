#include "menu_widget.h"
#include "ui_menu_widget.h"
#include <QTreeWidgetItem>
#include <QVariant>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QLineEdit>
#include <QApplication>
#include <QKeyEvent>
#include <QEvent>
#include <QUuid>
#include <QMessageBox>
#include <QRegularExpression>
#include <QInputDialog>
#include <QPushButton>
#include <QHBoxLayout>
#include <QRect>
#include <QTimer>
#include <QFontMetrics>
#include <QShowEvent>
#include "page/search_page.h"
#include "page/playlist_page.h"
#include "page/settings_page.h"
#include <log/log_manager.h>
#include <manager/application_context.h>
#include <playlist/playlist.h>
#include <playlist/playlist_manager.h>

MenuWidget::MenuWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui_MenuWidget)
    , m_playlistCategory(nullptr)
    , m_contextMenu(nullptr)
    , m_contextMenuItem(nullptr)
    , m_addCategoryButton(nullptr)
    , m_addPlaylistButton(nullptr)
    , m_hoveredCategoryItem(nullptr)
{
    ui->setupUi(this);
    
    // Connect tree widget signals
    connect(ui->menuTree, &QTreeWidget::itemClicked, this, &MenuWidget::onItemClicked);
    
    // Setup context menu
    // setupContextMenu();
    
    setupMenuItems();
    
    // Expand important categories by default
    if (m_playlistCategory) {
        m_playlistCategory->setExpanded(true);
    }
    
    // Connect resize events to update button position
    connect(ui->menuTree, &QTreeWidget::itemExpanded, this, &MenuWidget::updateAddCategoryButtonPosition);
    connect(ui->menuTree, &QTreeWidget::itemCollapsed, this, &MenuWidget::updateAddCategoryButtonPosition);
    
    // Connect hover events for playlist button
    connect(ui->menuTree, &QTreeWidget::itemEntered, this, &MenuWidget::onTreeItemEntered);
    ui->menuTree->setMouseTracking(true);
}

MenuWidget::~MenuWidget()
{
    delete ui;
}

void MenuWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // Update button position when widget becomes visible
    updateAddCategoryButtonPosition();
}

void MenuWidget::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    // Hide playlist button when mouse leaves the widget
    onTreeLeaveEvent();
}



void MenuWidget::selectItem(const QString& itemId)
{
    // Find and select item by ID
    QTreeWidgetItemIterator it(ui->menuTree);
    while (*it) {
        QTreeWidgetItem* item = *it;
        if (item->data(0, Qt::UserRole).toString() == itemId) {
            ui->menuTree->setCurrentItem(item);
            onItemClicked(item, 0);
            break;
        }
        ++it;
    }
}

void MenuWidget::addPlaylist(const MenuPlaylistInfo& playlist)
{
    if (!m_playlistCategory) {
        qWarning() << "Playlist category not found";
        return;
    }
    
    // Check if playlist already exists
    if (findPlaylistItem(playlist.id)) {
        updatePlaylist(playlist);
        return;
    }
    
    QTreeWidgetItem* playlistItem = createPlaylistItem(playlist);
    m_playlistCategory->addChild(playlistItem);
    
    // Update playlist count in category
    int playlistCount = m_playlistCategory->childCount();
    m_playlistCategory->setText(0, QString("Playlists (%1)").arg(playlistCount));
    
    // Update button position since text changed
    updateAddCategoryButtonPosition();
}

void MenuWidget::removePlaylist(const QString& playlistId)
{
    QTreeWidgetItem* item = findPlaylistItem(playlistId);
    if (item && m_playlistCategory) {
        m_playlistCategory->removeChild(item);
        delete item;
        
        // Update playlist count
        int playlistCount = m_playlistCategory->childCount();
        m_playlistCategory->setText(0, QString("Playlists (%1)").arg(playlistCount));
        
        // Update button position since text changed
        updateAddCategoryButtonPosition();
    }
}

void MenuWidget::updatePlaylist(const MenuPlaylistInfo& playlist)
{
    QTreeWidgetItem* item = findPlaylistItem(playlist.id);
    if (item) {
        item->setText(0, QString("%1 (%2 songs)").arg(playlist.name).arg(playlist.songCount));
        item->setToolTip(0, playlist.description);
        item->setData(0, Qt::UserRole + 1, QVariant::fromValue(playlist));
    }
}

void MenuWidget::setupMenuItems()
{
    ui->menuTree->clear();
    
    // Create main navigation items
    QTreeWidgetItem* homeItem = createActionItem("🏠Home", "home", QIcon(":/icons/home.png"));
    ui->menuTree->addTopLevelItem(homeItem);

    QTreeWidgetItem* searchItem = createActionItem("🔍Search", SearchPage::SEARCH_PAGE_TYPE, QIcon(":/icons/search.png"));
    ui->menuTree->addTopLevelItem(searchItem);
    
    // Create Playlists category
    m_playlistCategory = createCategoryItem("🎵Playlists", PlaylistPage::PLAYLIST_PAGE_TYPE);
    m_playlistCategory->setIcon(0, QIcon(":/icons/playlist.png"));
    ui->menuTree->addTopLevelItem(m_playlistCategory);

    // Create the add button
    m_addCategoryButton = new QPushButton("+", this);
    m_addCategoryButton->setFixedSize(20, 20);
    m_addCategoryButton->setToolTip("Add new category");
    m_addCategoryButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 10px;"
        "    font-weight: bold;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #45a049;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #3d8b40;"
        "}"
    );
    
    // Position the button - we'll position it after the tree is set up
    connect(m_addCategoryButton, &QPushButton::clicked, this, &MenuWidget::onAddCategoryButtonClicked);
    updateAddCategoryButtonPosition();
    
    // Create the add playlist button (initially hidden)
    m_addPlaylistButton = new QPushButton("+", this);
    m_addPlaylistButton->setFixedSize(20, 20);
    m_addPlaylistButton->setToolTip("Add new playlist");
    m_addPlaylistButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #2196F3;"  // Blue color to distinguish from category button
        "    color: white;"
        "    border: none;"
        "    border-radius: 10px;"
        "    font-weight: bold;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #1976D2;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #1565C0;"
        "}"
    );
    
    // Initially hidden
    m_addPlaylistButton->hide();
    
    // Connect click event
    connect(m_addPlaylistButton, &QPushButton::clicked, this, &MenuWidget::onAddPlaylistButtonClicked);

    // Position the button relative to the tree widget
    // We'll position it when the playlists category is created
    
    // Create Settings item
    QTreeWidgetItem* settingsItem = createActionItem("⚙️Settings", SettingsPage::SETTINGS_PAGE_TYPE, QIcon(":/icons/settings.png"));
    ui->menuTree->addTopLevelItem(settingsItem);
    
    // Select first item by default
    if (ui->menuTree->topLevelItemCount() > 0) {
        ui->menuTree->setCurrentItem(ui->menuTree->topLevelItem(0));
    }
}

QTreeWidgetItem* MenuWidget::createCategoryItem(const QString& name, const QString& args)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, name);
    item->setData(0, Qt::UserRole, args);
    item->setData(0, Qt::UserRole + 2, static_cast<int>(CategoryItem));
    
    // Style category items
    // QFont font = item->font(0);
    // font.setBold(true);
    // item->setFont(0, font);
    
    return item;
}

QTreeWidgetItem* MenuWidget::createPlaylistItem(const MenuPlaylistInfo& playlist)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, QString("%1 (%2 songs)").arg(playlist.name).arg(playlist.songCount));
    item->setToolTip(0, playlist.description);
    item->setData(0, Qt::UserRole, playlist.id);
    item->setData(0, Qt::UserRole + 1, QVariant::fromValue(playlist));
    item->setData(0, Qt::UserRole + 2, static_cast<int>(PlaylistItem));
    item->setIcon(0, QIcon(":/icons/playlist_item.png"));
    
    return item;
}

QTreeWidgetItem* MenuWidget::createActionItem(const QString& name, const QString& args, const QIcon& icon)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, name);
    item->setData(0, Qt::UserRole, args);
    item->setData(0, Qt::UserRole + 2, static_cast<int>(ActionItem));
    item->setIcon(0, icon);
    
    return item;
}

QTreeWidgetItem* MenuWidget::findPlaylistItem(const QString& playlistId)
{
    if (!m_playlistCategory) return nullptr;
    
    for (int i = 0; i < m_playlistCategory->childCount(); ++i) {
        QTreeWidgetItem* child = m_playlistCategory->child(i);
        if (child->data(0, Qt::UserRole).toString() == playlistId) {
            return child;
        }
    }
    return nullptr;
}

void MenuWidget::onItemClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    
    if (!item) return;
    
    ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole + 2).toInt());
    QString itemId = item->data(0, Qt::UserRole).toString();
    QString itemText = item->text(0);
    
    switch (itemType) {
        case CategoryItem:
            emit categoryClicked(itemText);
            // Toggle expansion for category items
            item->setExpanded(!item->isExpanded());
            break;
            
        case PlaylistItem: {
            MenuPlaylistInfo playlist = item->data(0, Qt::UserRole + 1).value<MenuPlaylistInfo>();
            emit playlistClicked(playlist.id, playlist.name);
            break;
        }
        
        case ActionItem:
            emit menuItemSelected(itemId, itemText, itemId);
            break;
    }
}

void MenuWidget::setupContextMenu()
{
    // Enable context menu for tree widget
    // ui->menuTree->setContextMenuPolicy(Qt::CustomContextMenu);
    // connect(ui->menuTree, &QTreeWidget::customContextMenuRequested, 
    //         this, &MenuWidget::showContextMenu);
    
    // // Create context menu
    // m_contextMenu = new QMenu(this);
    
    // m_newPlaylistAction = m_contextMenu->addAction("New Category");
    // connect(m_newPlaylistAction, &QAction::triggered, this, &MenuWidget::createNewCategory);
}

void MenuWidget::showContextMenu(const QPoint& position)
{
    QTreeWidgetItem* item = ui->menuTree->itemAt(position);
    if (!item) return;
    
    m_contextMenuItem = item;
    ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole + 2).toInt());
    
    // Show context menu for any category item (including Playlists and new categories)
    if (itemType == CategoryItem) {
        m_contextMenu->exec(ui->menuTree->mapToGlobal(position));
    }
}

void MenuWidget::createNewCategory()
{
    // Redirect to the add button functionality
    onAddCategoryButtonClicked();
}

QString MenuWidget::generateUniquePlaylistId()
{
    return QString("playlist_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

void MenuWidget::addPlaylistToCategory(QTreeWidgetItem* categoryItem, const MenuPlaylistInfo& playlist)
{
    if (!categoryItem) return;

    if (!PLAYLIST_MANAGER) {
        LOG_WARN("Playlist manager is not available.");
        return;
    }

    QUuid categoryUuid = QUuid::fromString(categoryItem->data(0, Qt::UserRole).toString());
    playlist::PlaylistInfo playlistInfo {
        .name = playlist.name,
        .creator = "",
        .description = playlist.description,
        .coverUrl = "",
        .uuid = QUuid::createUuid()
    };
    if (!PLAYLIST_MANAGER->addPlaylist(playlistInfo, categoryUuid)) {
        LOG_WARN("Failed to add playlist to manager: %s", playlist.name.toUtf8().constData());
        return;
    }
    // Create playlist item and add to category
    QTreeWidgetItem* playlistItem = createPlaylistItem(playlist);
    categoryItem->addChild(playlistItem);

    // Update button position if this is the playlists category (text changed)
    if (categoryItem == m_playlistCategory) {
        updateAddCategoryButtonPosition();
    }
}

// Everytime the ui size changes, update the add category button position
void MenuWidget::updateAddCategoryButtonPosition()
{
    if (!m_addCategoryButton || !m_playlistCategory) return;
    
    // Use QTimer::singleShot to position after the widget is fully rendered
    QTimer::singleShot(0, this, [this]() {
        if (!m_addCategoryButton || !m_playlistCategory) return;
        
        QRect itemRect = ui->menuTree->visualItemRect(m_playlistCategory);
        if (itemRect.isValid()) {
            // Position button on the far right of the tree widget
            int treeWidth = ui->menuTree->width();
            int buttonX = treeWidth - 30; // 30px from right edge (20px button + 10px margin)
            int buttonY = itemRect.top() + (itemRect.height() - 20) / 2; // Center vertically
            
            m_addCategoryButton->move(buttonX, buttonY);
            m_addCategoryButton->show();
        }
    });
}

void MenuWidget::updateAddPlaylistButtonPosition(QTreeWidgetItem* categoryItem)
{
    if (!m_addPlaylistButton || !categoryItem) return;
    
    // Use QTimer::singleShot to position after the widget is fully rendered
    QTimer::singleShot(0, this, [this, categoryItem]() {
        if (!m_addPlaylistButton || !categoryItem) return;
        
        QRect itemRect = ui->menuTree->visualItemRect(categoryItem);
        if (itemRect.isValid()) {
            // Position button on the far right of the tree widget
            int treeWidth = ui->menuTree->width();
            int buttonX = treeWidth - 30; // 30px from right edge
            int buttonY = itemRect.top() + (itemRect.height() - 20) / 2; // Center vertically
            
            m_addPlaylistButton->move(buttonX, buttonY);
            m_addPlaylistButton->show();
        }
    });
}

void MenuWidget::onAddCategoryButtonClicked()
{
    bool ok;
    QString categoryName = QInputDialog::getText(
        this,
        "New Category",
        "Enter category name:",
        QLineEdit::Normal,
        "",
        &ok
    );
    
    if (ok && !categoryName.trimmed().isEmpty()) {
        createNewCategoryWithName(categoryName.trimmed());
    }
}

void MenuWidget::createNewCategoryWithName(const QString& name)
{
    if (!m_playlistCategory) return;
    
    if (PLAYLIST_MANAGER == nullptr) {
        LOG_ERROR("Playlist manager is not available.");
        return;
    }

    playlist::CategoryInfo categoryInfo {
        .name = name,
        .uuid = QUuid::createUuid()
    };
    if (!PLAYLIST_MANAGER->addCategory(categoryInfo)) {
        LOG_ERROR("Failed to add category to manager: %s", name.toUtf8().constData());
        return;
    }
    
    QTreeWidgetItem* categoryItem = createCategoryItem(name, categoryInfo.uuid.toString(QUuid::WithoutBraces));
    categoryItem->setIcon(0, QIcon(":/icons/folder.png"));
    
    // Add as a child of the Playlists category
    m_playlistCategory->addChild(categoryItem);
    
    // Emit signal for external handling
    emit categoryCreated(categoryInfo.uuid.toString(QUuid::WithoutBraces), name);

    LOG_DEBUG("Created new category: %s as subitem of Playlists", name.toUtf8().constData());
}

void MenuWidget::onTreeItemEntered(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    
    if (!item) return;
    
    ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole + 2).toInt());
    
    // Show playlist button only for category items that are NOT the main Playlists category
    // (i.e., show for sub-categories under Playlists, but not for the main Playlists item itself)
    if (itemType == CategoryItem && item != m_playlistCategory) {
        m_hoveredCategoryItem = item;
        updateAddPlaylistButtonPosition(item);
    } else {
        // Hide playlist button when not hovering over a sub-category
        if (m_addPlaylistButton) {
            m_addPlaylistButton->hide();
        }
        m_hoveredCategoryItem = nullptr;
    }
}

void MenuWidget::onTreeLeaveEvent()
{
    // Hide playlist button when mouse leaves the tree
    if (m_addPlaylistButton) {
        m_addPlaylistButton->hide();
    }
    m_hoveredCategoryItem = nullptr;
}

void MenuWidget::onAddPlaylistButtonClicked()
{
    if (!m_hoveredCategoryItem) return;
    auto category_ = m_hoveredCategoryItem;
    bool ok;
    QString playlistName = QInputDialog::getText(
        this,
        "New Playlist",
        "Enter playlist name:",
        QLineEdit::Normal,
        "",
        &ok
    );
    
    if (ok && !playlistName.trimmed().isEmpty()) {
        // Create a new playlist in the hovered category
        MenuPlaylistInfo newPlaylist;
        newPlaylist.id = generateUniquePlaylistId();
        newPlaylist.name = playlistName.trimmed();
        newPlaylist.description = "";
        newPlaylist.songCount = 0;

        addPlaylistToCategory(category_, newPlaylist);

        LOG_DEBUG("Created new playlist: %s in category: %s",
                  playlistName.toUtf8().constData(),
                  category_->text(0).toUtf8().constData());
    }
}

