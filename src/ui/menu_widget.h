#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMetaType>
#include <QMenu>
#include <QAction>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QUuid>

QT_BEGIN_NAMESPACE
class Ui_MenuWidget;
QT_END_NAMESPACE

// Forward declarations
namespace playlist {
    struct CategoryInfo;
}

struct MenuPlaylistInfo {
    QString id;
    QString name;
    QString description;
    int songCount = 0;
};

class MenuWidget : public QWidget
{
    Q_OBJECT
public:
    struct MenuItem {
        QString name;
        QIcon icon;
        QString args;
    };
    
    enum ItemType {
        CategoryItem,
        PlaylistItem,
        ActionItem
    };
    
public:
    explicit MenuWidget(QWidget *parent = nullptr);
    ~MenuWidget();

    void addPlaylist(const MenuPlaylistInfo& playlist);
    void removePlaylist(const QString& playlistId);
    void updatePlaylist(const MenuPlaylistInfo& playlist);
    void selectItem(const QString& itemId);

signals:
    void menuItemSelected(const QString& itemId, const QString& itemText, const QString& args);
    void playlistClicked(const QString& playlistId, const QString& playlistName);
    void categoryClicked(const QString& categoryName);
    void categoryCreated(const QString& categoryUuid, const QString& categoryName);

protected:
    void showEvent(QShowEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void showContextMenu(const QPoint& position);
    void createNewCategory();
    void onAddCategoryButtonClicked();
    void onAddPlaylistButtonClicked();
    void onTreeItemEntered(QTreeWidgetItem* item, int column);
    void onTreeLeaveEvent();
    void onCategoriesLoaded(int categoryCount, int playlistCount);
    void onCategoryAdded(const playlist::CategoryInfo& category);
    void onDeleteItem();

private:
    void setupMenuItems();
    void setupContextMenu();
    void updateCategories();
    void updateAddCategoryButtonPosition();
    void updateAddPlaylistButtonPosition(QTreeWidgetItem* categoryItem);
    QTreeWidgetItem* createCategoryItem(const QString& name, const QString& itemId, const QUuid& uuid);
    QTreeWidgetItem* createPlaylistItem(const MenuPlaylistInfo& playlist, const QUuid& uuid);
    QTreeWidgetItem* createActionItem(const QString& name, const QString& args = "", const QIcon& icon = QIcon());
    QTreeWidgetItem* findPlaylistItem(const QString& playlistId);
    QString generateUniquePlaylistId();
    void addPlaylistToCategory(QTreeWidgetItem* categoryItem, const MenuPlaylistInfo& playlist);
    void createNewCategoryWithName(const QString& name);
    void deleteCategoryWithConfirmation(QTreeWidgetItem* categoryItem);
    void deletePlaylistWithConfirmation(QTreeWidgetItem* playlistItem);

private:
    Ui_MenuWidget *ui;
    QTreeWidgetItem* m_playlistCategory;
    QMenu* m_contextMenu;
    QAction* m_deleteAction;
    QTreeWidgetItem* m_contextMenuItem;
    QPushButton* m_addCategoryButton;
    QPushButton* m_addPlaylistButton;
    QTreeWidgetItem* m_hoveredCategoryItem;
};

Q_DECLARE_METATYPE(MenuWidget::MenuItem)
Q_DECLARE_METATYPE(MenuPlaylistInfo)