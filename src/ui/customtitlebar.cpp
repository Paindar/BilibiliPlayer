#include "customtitlebar.h"
#include "ui_customtitlebar.h"
#include <QApplication>

CustomTitleBar::CustomTitleBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui_CustomTitleBar)
    , m_dragging(false)
{
    ui->setupUi(this);
    
    // Connect button signals
    connect(ui->minimizeButton, &QPushButton::clicked, this, &CustomTitleBar::onMinimizeClicked);
    connect(ui->maximizeButton, &QPushButton::clicked, this, &CustomTitleBar::onMaximizeClicked);
    connect(ui->closeButton, &QPushButton::clicked, this, &CustomTitleBar::onCloseClicked);
}

CustomTitleBar::~CustomTitleBar()
{
    delete ui;
}

void CustomTitleBar::setTitle(const QString& title)
{
    ui->titleLabel->setText(title);
}

void CustomTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_startPos = event->globalPosition().toPoint();
        m_dragging = true;
    }
    QWidget::mousePressEvent(event);
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        QWidget* parentWindow = window();
        if (parentWindow) {
            QPoint delta = event->globalPosition().toPoint() - m_startPos;
            parentWindow->move(parentWindow->pos() + delta);
            m_startPos = event->globalPosition().toPoint();
        }
    }
    QWidget::mouseMoveEvent(event);
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit maximizeClicked();
    }
    QWidget::mouseDoubleClickEvent(event);
}

void CustomTitleBar::onMinimizeClicked()
{
    emit minimizeClicked();
}

void CustomTitleBar::onMaximizeClicked()
{
    emit maximizeClicked();
}

void CustomTitleBar::onCloseClicked()
{
    emit closeClicked();
}