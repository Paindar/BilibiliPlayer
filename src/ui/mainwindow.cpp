#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "customtitlebar.h"
#include "menuwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_menuWidget(nullptr)
{
    ui->setupUi(this);
    
    // Remove the Windows title bar to create a frameless window
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    
    setupCustomComponents();
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
    
    // Create and setup menu widget
    m_menuWidget = new MenuWidget(this);
    ui->menuContainerLayout->addWidget(m_menuWidget);

    // Connect menu signals
    connect(m_menuWidget, &MenuWidget::menuItemSelected, this, &MainWindow::onMenuItemSelected);
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

void MainWindow::onMenuItemSelected(int index, const QString& itemText)
{
    updateContentArea(index, itemText);
}

void MainWindow::updateContentArea(int index, const QString& itemText)
{
    // Clear current content
    QLayoutItem* item;
    while ((item = ui->contentLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    
    // Create content based on selected menu item
    QLabel* titleLabel = new QLabel(QString("当前页面: %1").arg(itemText), this);
    titleLabel->setStyleSheet("QLabel { color: #ffffff; font-size: 20px; font-weight: bold; }");
    titleLabel->setAlignment(Qt::AlignCenter);
    
    QLabel* contentLabel = new QLabel(QString("这是 %1 的内容区域").arg(itemText), this);
    contentLabel->setStyleSheet("QLabel { color: #cccccc; font-size: 14px; }");
    contentLabel->setAlignment(Qt::AlignCenter);
    
    ui->contentLayout->addWidget(titleLabel);
    ui->contentLayout->addStretch();
    ui->contentLayout->addWidget(contentLabel);
    ui->contentLayout->addStretch();
}
