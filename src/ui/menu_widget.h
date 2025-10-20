#pragma once

#include <QWidget>
#include <QListWidget>

QT_BEGIN_NAMESPACE
class Ui_MenuWidget;
QT_END_NAMESPACE

class MenuWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MenuWidget(QWidget *parent = nullptr);
    ~MenuWidget();

    void selectItem(int index);

signals:
    void menuItemSelected(int index, const QString& itemText);

private slots:
    void onItemClicked(QListWidgetItem* item);

private:
    Ui_MenuWidget *ui;
};