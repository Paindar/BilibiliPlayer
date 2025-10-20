#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class CustomTitleBar;
class MenuWidget;
class SearchPage;
class INavigablePage;
class Navigator;

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
    void onSearchRequested(const QString& searchText);
    void onPageChanged(INavigablePage* page);

private:
    void setupCustomComponents();
    void setupNavigator();

private:
    Ui::MainWindow *ui;
    CustomTitleBar *m_titleBar;
    MenuWidget *m_menuWidget;
    Navigator* m_navigator;
};
#endif // MAIN_WINDOW_H
