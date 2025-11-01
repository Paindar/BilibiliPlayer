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
    setupContextMenu();
    
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
    
    // Connect to PlaylistManager signals
    if (PLAYLIST_MANAGER) {
        connect(PLAYLIST_MANAGER, &PlaylistManager::categoriesLoaded, this, &MenuWidget::onCategoriesLoaded);
        connect(PLAYLIST_MANAGER, &PlaylistManager::categoryAdded, this, &MenuWidget::onCategoryAdded);
        
        // Check if categories are already loaded (PlaylistManager initialized before UI)
        // Use QTimer::singleShot to ensure this runs after constructor completes
        QTimer::singleShot(0, this, [this]() {
            if (PLAYLIST_MANAGER) {
                updateCategories();
            }
        });
    }
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
    
    QUuid playlistUuid = QUuid::createUuid(); // Generate UUID for this playlist
    QTreeWidgetItem* playlistItem = createPlaylistItem(playlist, playlistUuid);
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
        // UUID remains unchanged at Qt::UserRole + 1
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
    QUuid playlistCategoryUuid = QUuid::createUuid(); // Special UUID for the main playlists category
    m_playlistCategory = createCategoryItem("🎵Playlists", PlaylistPage::PLAYLIST_PAGE_TYPE, playlistCategoryUuid);
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

QTreeWidgetItem* MenuWidget::createCategoryItem(const QString& name, const QString& itemId, const QUuid& uuid)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, name);
    item->setData(0, Qt::UserRole, itemId);
    item->setData(0, Qt::UserRole + 1, uuid);
    item->setData(0, Qt::UserRole + 2, static_cast<int>(CategoryItem));
    
    // Style category items
    // QFont font = item->font(0);
    // font.setBold(true);
    // item->setFont(0, font);
    
    return item;
}

QTreeWidgetItem* MenuWidget::createPlaylistItem(const MenuPlaylistInfo& playlist, const QUuid& uuid)
{
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, QString("%1 (%2 songs)").arg(playlist.name).arg(playlist.songCount));
    item->setToolTip(0, playlist.description);
    item->setData(0, Qt::UserRole, playlist.id);
    item->setData(0, Qt::UserRole + 1, uuid);
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
            QString playlistId = item->data(0, Qt::UserRole).toString();
            QUuid playlistUuid = item->data(0, Qt::UserRole + 1).toUuid();
            QString playlistName = item->text(0);
            // Extract name from the "Name (X songs)" format
            int parenIndex = playlistName.lastIndexOf(" (");
            if (parenIndex > 0) {
                playlistName = playlistName.left(parenIndex);
            }
            
            // Navigate to playlist page with playlist UUID as argument
            QString args = QString("playlistId=%1").arg(playlistUuid.toString(QUuid::WithoutBraces));
            emit menuItemSelected(PlaylistPage::PLAYLIST_PAGE_TYPE, playlistName, args);
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
    ui->menuTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->menuTree, &QTreeWidget::customContextMenuRequested, 
            this, &MenuWidget::showContextMenu);
    
    // Create context menu
    m_contextMenu = new QMenu(this);
    
    // Delete action - text will be updated dynamically based on item type
    m_deleteAction = m_contextMenu->addAction("Delete");
    connect(m_deleteAction, &QAction::triggered, this, &MenuWidget::onDeleteItem);
}

void MenuWidget::updateCategories()
{
    if (!PLAYLIST_MANAGER || !m_playlistCategory) {
        LOG_WARN("Cannot update categories: manager or playlist category is null");
        return;
    }
    
    // Clear existing child categories (but keep any existing playlists)
    QList<QTreeWidgetItem*> toRemove;
    for (int i = 0; i < m_playlistCategory->childCount(); ++i) {
        QTreeWidgetItem* child = m_playlistCategory->child(i);
        ItemType itemType = static_cast<ItemType>(child->data(0, Qt::UserRole + 2).toInt());
        if (itemType == CategoryItem) {
            toRemove.append(child);
        }
    }
    
    for (QTreeWidgetItem* item : toRemove) {
        m_playlistCategory->removeChild(item);
        delete item;
    }
    
    // Load all categories from the manager
    int categoryCount = 0, playlistCount = 0;
    QList<playlist::CategoryInfo> categories = PLAYLIST_MANAGER->iterateCategories([](const playlist::CategoryInfo&) { return true; });
    for (const auto& categoryInfo : categories) {
        QString categoryId = QString("category_%1").arg(categoryInfo.uuid.toString(QUuid::WithoutBraces));
        
        // Create category tree widget item
        QTreeWidgetItem* categoryItem = createCategoryItem(categoryInfo.name, categoryId, categoryInfo.uuid);
        categoryItem->setIcon(0, QIcon(":/icons/folder.png"));
        
        // Add as a child of the Playlists category
        m_playlistCategory->addChild(categoryItem);
        
        // Add playlists to this category
        auto playlists = PLAYLIST_MANAGER->iteratePlaylistsInCategory(categoryInfo.uuid, 
            [](const playlist::PlaylistInfo&) { return true; });
        for (const auto& playlistInfo : playlists) {
            MenuPlaylistInfo menuPlaylist;
            menuPlaylist.id = playlistInfo.uuid.toString(QUuid::WithoutBraces);
            menuPlaylist.name = playlistInfo.name;
            menuPlaylist.description = playlistInfo.description;
            // Get song count from the separate songs storage
            auto songs = PLAYLIST_MANAGER->iterateSongsInPlaylist(playlistInfo.uuid,
                [](const playlist::SongInfo&) { return true; });
            menuPlaylist.songCount = songs.size();
            
            QTreeWidgetItem* playlistItem = createPlaylistItem(menuPlaylist, playlistInfo.uuid);
            categoryItem->addChild(playlistItem);
            playlistCount++;
        }
        categoryCount++;
    }
    
    // Update button position since text might have changed
    updateAddCategoryButtonPosition();

    LOG_INFO("Updated menu with {} categories and {} playlists", categoryCount, playlistCount);
}

void MenuWidget::showContextMenu(const QPoint& position)
{
    QTreeWidgetItem* item = ui->menuTree->itemAt(position);
    if (!item) return;
    
    m_contextMenuItem = item;
    ItemType itemType = static_cast<ItemType>(item->data(0, Qt::UserRole + 2).toInt());
    
    // Don't show context menu for the main Playlists category or Action items
    if (itemType == ActionItem || item == m_playlistCategory) {
        return;
    }
    
    // Update delete action text based on item type and name
    QString itemName = item->text(0);
    if (itemType == CategoryItem) {
        // Extract clean category name
        m_deleteAction->setText(QString("Delete Category \"%1\"").arg(itemName));
    } else if (itemType == PlaylistItem) {
        // Extract playlist name from "Name (X songs)" format
        int parenIndex = itemName.lastIndexOf(" (");
        if (parenIndex > 0) {
            itemName = itemName.left(parenIndex);
        }
        m_deleteAction->setText(QString("Delete Playlist \"%1\"").arg(itemName));
    }
    
    // Show context menu for categories and playlists
    if (itemType == CategoryItem || itemType == PlaylistItem) {
        m_contextMenu->exec(ui->menuTree->mapToGlobal(position));
    }
}

void MenuWidget::createNewCategory()
{
    // Redirect to the add button functionality
    onAddCategoryButtonClicked();
}

void MenuWidget::onDeleteItem()
{
    if (!m_contextMenuItem) {
        LOG_WARN("No context menu item selected for deletion");
        return;
    }
    
    ItemType itemType = static_cast<ItemType>(m_contextMenuItem->data(0, Qt::UserRole + 2).toInt());
    QString itemName = m_contextMenuItem->text(0);
    
    if (itemType == CategoryItem) {
        deleteCategoryWithConfirmation(m_contextMenuItem);
    } else if (itemType == PlaylistItem) {
        deletePlaylistWithConfirmation(m_contextMenuItem);
    }
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

    QUuid categoryUuid = categoryItem->data(0, Qt::UserRole + 1).toUuid();
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
    QTreeWidgetItem* playlistItem = createPlaylistItem(playlist, playlistInfo.uuid);
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
    
    QString categoryId = QString("category_%1").arg(categoryInfo.uuid.toString(QUuid::WithoutBraces));
    QTreeWidgetItem* categoryItem = createCategoryItem(name, categoryId, categoryInfo.uuid);
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

void MenuWidget::onCategoriesLoaded(int categoryCount, int playlistCount)
{
    LOG_DEBUG("Categories loaded signal received: %d categories, %d playlists", categoryCount, playlistCount);
    
    updateCategories();
}

void MenuWidget::onCategoryAdded(const playlist::CategoryInfo& category)
{
    if (!m_playlistCategory) {
        LOG_WARN("Cannot add category to menu: playlist category is null");
        return;
    }
    
    QString categoryId = QString("category_%1").arg(category.uuid.toString(QUuid::WithoutBraces));
    
    // Create category tree widget item
    QTreeWidgetItem* categoryItem = createCategoryItem(category.name, categoryId, category.uuid);
    categoryItem->setIcon(0, QIcon(":/icons/folder.png"));
    
    // Add as a child of the Playlists category
    m_playlistCategory->addChild(categoryItem);
    
    // Add any existing playlists to this category
    auto playlists = PLAYLIST_MANAGER->iteratePlaylistsInCategory(category.uuid, 
        [](const playlist::PlaylistInfo&) { return true; });
    for (const auto& playlistInfo : playlists) {
        MenuPlaylistInfo menuPlaylist;
        menuPlaylist.id = playlistInfo.uuid.toString(QUuid::WithoutBraces);
        menuPlaylist.name = playlistInfo.name;
        menuPlaylist.description = playlistInfo.description;
        // Get song count from the separate songs storage
        auto songs = PLAYLIST_MANAGER->iterateSongsInPlaylist(playlistInfo.uuid,
            [](const playlist::SongInfo&) { return true; });
        menuPlaylist.songCount = songs.size();
        
        QTreeWidgetItem* playlistItem = createPlaylistItem(menuPlaylist, playlistInfo.uuid);
        categoryItem->addChild(playlistItem);
    }
    
    LOG_DEBUG("Added category to menu: %s", category.name.toUtf8().constData());
}

void MenuWidget::deleteCategoryWithConfirmation(QTreeWidgetItem* categoryItem)
{
    if (!categoryItem || !PLAYLIST_MANAGER) {
        LOG_WARN("Cannot delete category: invalid item or manager not available");
        return;
    }
    
    QString categoryName = categoryItem->text(0);
    QUuid categoryUuid = categoryItem->data(0, Qt::UserRole + 1).toUuid();
    
    // Count playlists in this category
    int playlistCount = categoryItem->childCount();
    
    // Create confirmation message
    QString confirmMessage;
    if (playlistCount > 0) {
        confirmMessage = QString("Are you sure you want to delete category \"%1\"?\n\n"
                                "This will also delete %2 playlist(s) contained in this category.\n"
                                "This action cannot be undone.")
                                .arg(categoryName)
                                .arg(playlistCount);
    } else {
        confirmMessage = QString("Are you sure you want to delete category \"%1\"?\n\n"
                                "This action cannot be undone.")
                                .arg(categoryName);
    }
    
    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Delete Category",
        confirmMessage,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        // Delete from manager first
        if (PLAYLIST_MANAGER->removeCategory(categoryUuid)) {
            // Remove from UI
            if (categoryItem->parent()) {
                categoryItem->parent()->removeChild(categoryItem);
            } else {
                int index = ui->menuTree->indexOfTopLevelItem(categoryItem);
                if (index >= 0) {
                    ui->menuTree->takeTopLevelItem(index);
                }
            }
            delete categoryItem;
            
            LOG_INFO("Deleted category: %s with %d playlists", categoryName.toUtf8().constData(), playlistCount);
        } else {
            QMessageBox::warning(this, "Delete Failed", 
                               QString("Failed to delete category \"%1\" from storage.").arg(categoryName));
            LOG_ERROR("Failed to delete category from manager: %s", categoryName.toUtf8().constData());
        }
    }
}

void MenuWidget::deletePlaylistWithConfirmation(QTreeWidgetItem* playlistItem)
{
    if (!playlistItem || !PLAYLIST_MANAGER) {
        LOG_WARN("Cannot delete playlist: invalid item or manager not available");
        return;
    }
    
    QString playlistName = playlistItem->text(0);
    // Extract clean playlist name from "Name (X songs)" format
    int parenIndex = playlistName.lastIndexOf(" (");
    if (parenIndex > 0) {
        playlistName = playlistName.left(parenIndex);
    }
    
    QUuid playlistUuid = playlistItem->data(0, Qt::UserRole + 1).toUuid();
    
    // Create confirmation message
    QString confirmMessage = QString("Are you sure you want to delete playlist \"%1\"?\n\n"
                                   "This action cannot be undone.")
                                   .arg(playlistName);
    
    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Delete Playlist",
        confirmMessage,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        // Get parent category UUID
        QUuid categoryUuid;
        if (playlistItem->parent()) {
            categoryUuid = playlistItem->parent()->data(0, Qt::UserRole + 1).toUuid();
        }
        
        // Delete from manager first
        if (PLAYLIST_MANAGER->removePlaylist(playlistUuid, categoryUuid)) {
            // Remove from UI
            if (playlistItem->parent()) {
                playlistItem->parent()->removeChild(playlistItem);
            }
            delete playlistItem;
            
            LOG_INFO("Deleted playlist: %s", playlistName.toUtf8().constData());
        } else {
            QMessageBox::warning(this, "Delete Failed", 
                               QString("Failed to delete playlist \"%1\" from storage.").arg(playlistName));
            LOG_ERROR("Failed to delete playlist from manager: %s", playlistName.toUtf8().constData());
        }
    }
}

