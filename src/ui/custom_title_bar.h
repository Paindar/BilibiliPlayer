#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QMouseEvent>

QT_BEGIN_NAMESPACE
class Ui_CustomTitleBar;
QT_END_NAMESPACE

class CustomTitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit CustomTitleBar(QWidget *parent = nullptr);
    ~CustomTitleBar();

    void setTitle(const QString& title);

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();
    void searchRequested(const QString& searchText);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onMinimizeClicked();
    void onMaximizeClicked();
    void onCloseClicked();
    void onSearchSubmitted();

private:
    Ui_CustomTitleBar *ui;
    QPoint m_startPos;
    bool m_dragging;
};