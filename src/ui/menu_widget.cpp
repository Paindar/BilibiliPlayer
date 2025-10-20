#include "menu_widget.h"
#include "ui_menu_widget.h"
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

void MenuWidget::selectItem(int index)
{
    if (index >= 0 && index < ui->menuList->count()) {
        ui->menuList->setCurrentRow(index);
        QListWidgetItem* item = ui->menuList->item(index);
        if (item) {
            emit menuItemSelected(index, item->text());
        }
    }
}

void MenuWidget::onItemClicked(QListWidgetItem* item)
{
    if (item) {
        int index = ui->menuList->row(item);
        QString text = item->text();
        emit menuItemSelected(index, text);
    }
}