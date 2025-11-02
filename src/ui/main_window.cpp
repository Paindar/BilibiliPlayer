#include "main_window.h"
#include "./ui_main_window.h"
#include "custom_title_bar.h"
#include "menu_widget.h"
#include "navigator/i_navigable_page.h"
#include "navigator/navigator.h"
#include "page/search_page.h"
#include "page/playlist_page.h"
#include "page/settings_page.h"
#include "../manager/application_context.h"
#include "../audio/audio_player_controller.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_titleBar(nullptr)
    , m_menuWidget(nullptr)
    , m_navigator(nullptr)
{
    ui->setupUi(this);
    
    // Remove the Windows title bar to create a frameless window
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    
    setupCustomComponents();
    setupNavigator();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupCustomComponents()
{
    // Create and setup custom title bar
    m_titleBar = new CustomTitleBar(this);
    QVBoxLayout* titleBarLayout = new QVBoxLayout(ui->titleBarContainer);
    titleBarLayout->setContentsMargins(0, 0, 0, 0);
    titleBarLayout->addWidget(m_titleBar);
    
    // Connect title bar signals
    connect(m_titleBar, &CustomTitleBar::minimizeClicked, this, &MainWindow::onMinimizeClicked);
    connect(m_titleBar, &CustomTitleBar::maximizeClicked, this, &MainWindow::onMaximizeClicked);
    connect(m_titleBar, &CustomTitleBar::closeClicked, this, &MainWindow::onCloseClicked);
    connect(m_titleBar, &CustomTitleBar::searchRequested, this, &MainWindow::onSearchRequested);
    
    // Create and setup menu widget
    m_menuWidget = new MenuWidget(this);
    connect(m_menuWidget, &MenuWidget::menuItemSelected, this, &MainWindow::onMenuItemSelected);
    ui->menuContainerLayout->addWidget(m_menuWidget);
}

void MainWindow::setupNavigator()
{
    m_navigator = new Navigator(this);

    m_navigator->registerPageFactory(SearchPage::SEARCH_PAGE_TYPE, [this]() {
        return new SearchPage(this);
    });
    
    m_navigator->registerPageFactory(PlaylistPage::PLAYLIST_PAGE_TYPE, [this]() {
        auto* playlistPage = new PlaylistPage(this);
        
        // Connect playlist page signals to audio player controller
        auto* audioController = AUDIO_PLAYER_CONTROLLER;
        if (audioController) {
            connect(playlistPage, &PlaylistPage::songDoubleClicked, 
                    [audioController, playlistPage](const playlist::SongInfo& song) {
                // Get current playlist from the page and start playback
                auto currentPlaylistId = playlistPage->getCurrentPlaylistId();
                if (!currentPlaylistId.isNull()) {
                    audioController->playPlaylistFromSong(currentPlaylistId, song);
                }
            });
        }
        
        return playlistPage;
    });
    m_navigator->registerPageFactory(SettingsPage::SETTINGS_PAGE_TYPE, [this]() {
        return new SettingsPage(this);
    });

    connect(m_navigator, &Navigator::afterPageChange, this, &MainWindow::onPageChanged);
    m_navigator->navigateTo(PlaylistPage::PLAYLIST_PAGE_TYPE);
}

void MainWindow::onMinimizeClicked()
{
    showMinimized();
}

void MainWindow::onMaximizeClicked()
{
    if (isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
}

void MainWindow::onCloseClicked()
{
    close();
}

void MainWindow::onSearchRequested(const QString& searchText)
{
    // Navigate to search page and set search text
    m_menuWidget->selectItem(SearchPage::SEARCH_PAGE_TYPE); // Select the search menu item
    m_navigator->navigateTo(SearchPage::SEARCH_PAGE_TYPE, QString("query=%0").arg(searchText));
}

void MainWindow::onPageChanged(INavigablePage *page)
{
    // Update the content area
    QWidget* pageWidget = dynamic_cast<QWidget*>(page);
    if (pageWidget) {
        // Clean up current content (but DON'T delete - let Navigator handle that)
        QLayoutItem* item;
        while ((item = ui->contentLayout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->setParent(nullptr);  // Remove from layout, but don't delete
            }
            delete item;  // Only delete the layout item
        }
        ui->contentLayout->addWidget(pageWidget);
    }
    
    // Let Navigator delete oldPage after this method returns
    // (Navigator should delete oldPage safely)
}

void MainWindow::onMenuItemSelected(const QString& itemId, const QString& itemText, const QString& args)
{
    m_navigator->navigateTo(itemId, args);
}
