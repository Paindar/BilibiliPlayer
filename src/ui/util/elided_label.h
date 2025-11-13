#pragma once

#include <QLabel>

class QResizeEvent;
class QEnterEvent;
class QPaintEvent;
class QPropertyAnimation;
class QTimer;

class ScrollingLabel : public QLabel {
    Q_OBJECT
    Q_PROPERTY(int scrollOffset READ scrollOffset WRITE setScrollOffset)

public:
    explicit ScrollingLabel(QWidget* parent = nullptr);

    void setText(const QString& text);
    QString fullText() const;
    
    void setScrollingEnabled(bool enabled);
    bool isScrollingEnabled() const;
    
    void setAutoScroll(bool autoScroll);
    bool isAutoScroll() const;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void setScrollOffset(int offset);

private:
    int scrollOffset() const;
    void updateText();
    void startScrolling();
    void stopScrolling();

    QString m_fullText;
    int m_scrollOffset;
    bool m_isTextElided;
    bool m_scrollingEnabled;
    bool m_autoScroll;
    QPropertyAnimation* m_scrollAnimation;
    QTimer* m_pauseTimer;
};
