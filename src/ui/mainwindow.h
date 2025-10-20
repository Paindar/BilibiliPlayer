#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class CustomTitleBar;
class MenuWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onMinimizeClicked();
    void onMaximizeClicked();
    void onCloseClicked();
    void onMenuItemSelected(int index, const QString& itemText);

private:
    void setupCustomComponents();
    void updateContentArea(int index, const QString& itemText);

private:
    Ui::MainWindow *ui;
    CustomTitleBar *m_titleBar;
    MenuWidget *m_menuWidget;
};
#endif // MAINWINDOW_H
