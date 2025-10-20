#include "menuwidget.h"
#include "ui_menuwidget.h"
#include <QListWidgetItem>

MenuWidget::MenuWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui_MenuWidget)
{
    ui->setupUi(this);
    
    // Connect list widget signals
    connect(ui->menuList, &QListWidget::itemClicked, this, &MenuWidget::onItemClicked);
    
    // Set first item as selected by default
    if (ui->menuList->count() > 0) {
        ui->menuList->setCurrentRow(0);
    }
}

MenuWidget::~MenuWidget()
{
    delete ui;
}

void MenuWidget::onItemClicked(QListWidgetItem* item)
{
    if (item) {
        int index = ui->menuList->row(item);
        QString text = item->text();
        emit menuItemSelected(index, text);
    }
}