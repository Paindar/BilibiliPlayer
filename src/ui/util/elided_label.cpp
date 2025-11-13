#include "elided_label.h"
#include <QPainter>
#include <QResizeEvent>
#include <QEnterEvent>
#include <QTimer>
#include <QPropertyAnimation>
#include <log/log_manager.h>

ScrollingLabel::ScrollingLabel(QWidget* parent)
    : QLabel(parent)
    , m_scrollOffset(0)
    , m_isTextElided(false)
    , m_scrollingEnabled(true)
    , m_autoScroll(false)
    , m_scrollAnimation(nullptr)
    , m_pauseTimer(nullptr)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setTextFormat(Qt::PlainText);
}

void ScrollingLabel::setText(const QString& text) {
    m_fullText = text;
    setToolTip(text);
    updateText();
}

QString ScrollingLabel::fullText() const {
    return m_fullText;
}

void ScrollingLabel::setScrollingEnabled(bool enabled) {
    m_scrollingEnabled = enabled;
    if (!enabled) {
        stopScrolling();
    } else {
        updateText();
    }
}

bool ScrollingLabel::isScrollingEnabled() const {
    return m_scrollingEnabled;
}

void ScrollingLabel::setAutoScroll(bool autoScroll) {
    m_autoScroll = autoScroll;
    if (m_autoScroll && m_scrollingEnabled && m_isTextElided) {
        startScrolling();
    } else if (!m_autoScroll) {
        stopScrolling();
    }
}

bool ScrollingLabel::isAutoScroll() const {
    return m_autoScroll;
}

void ScrollingLabel::resizeEvent(QResizeEvent* event) {
    QLabel::resizeEvent(event);
    updateText();
}

void ScrollingLabel::enterEvent(QEnterEvent* event) {
    QLabel::enterEvent(event);
    if (!m_autoScroll && m_scrollingEnabled && m_isTextElided) {
        startScrolling();
    }
}

void ScrollingLabel::leaveEvent(QEvent* event) {
    QLabel::leaveEvent(event);
    if (!m_autoScroll) {
        stopScrolling();
    }
}

void ScrollingLabel::paintEvent(QPaintEvent* event) {
    // If we have full text but QLabel text is empty, we're in scrolling mode
    bool inScrollingMode = !m_fullText.isEmpty() && text().isEmpty();
    
    if (!inScrollingMode || !m_scrollingEnabled) {
        QLabel::paintEvent(event);
        return;
    }

    QPainter painter(this);
    painter.setFont(font());
    
    QFontMetrics metrics(font());
    int textWidth = metrics.horizontalAdvance(m_fullText);
    
    LOG_DEBUG("ScrollingLabel::paintEvent: scrollOffset={}, textWidth={}, labelWidth={}, font={}",
              m_scrollOffset, textWidth, width(), font().toString().toStdString());
    
    // If textWidth is 0, font metrics failed - fall back to showing text normally
    if (textWidth == 0) {
        QLabel::setText(m_fullText);
        QLabel::paintEvent(event);
        return;
    }
    
    // Draw scrolling text
    int x = -m_scrollOffset;
    painter.drawText(x, 0, textWidth, height(), Qt::AlignLeft | Qt::AlignVCenter, m_fullText);
    
    // Draw duplicate for seamless loop
    if (m_scrollOffset > 0) {
        painter.drawText(x + textWidth + 20, 0, textWidth, height(), Qt::AlignLeft | Qt::AlignVCenter, m_fullText);
    }
}

void ScrollingLabel::setScrollOffset(int offset) {
    m_scrollOffset = offset;
    update();
}

int ScrollingLabel::scrollOffset() const {
    return m_scrollOffset;
}

void ScrollingLabel::updateText() {
    // Don't process if widget isn't visible or has no size yet
    if (width() <= 0 || height() <= 0) {
        return;
    }
    
    QFontMetrics metrics(font());
    int textWidth = metrics.horizontalAdvance(m_fullText);
    m_isTextElided = textWidth > width();
    
    if (!m_scrollingEnabled || !m_isTextElided) {
        // Show elided text
        QString elided = metrics.elidedText(m_fullText, Qt::ElideRight, width());
        QLabel::setText(elided);
        stopScrolling();
    } else {
        // Clear label text, we'll draw in paintEvent
        QLabel::setText("");
        if (m_autoScroll) {
            startScrolling();
        }
    }
}

void ScrollingLabel::startScrolling() {
    stopScrolling();

    QFontMetrics metrics(font());
    int textWidth = metrics.horizontalAdvance(m_fullText);
    
    // Check if scrolling is actually needed
    if (!m_scrollingEnabled || textWidth <= width()) {
        return;
    }
    
    // Pause before starting
    m_pauseTimer = new QTimer(this);
    m_pauseTimer->setSingleShot(true);
    connect(m_pauseTimer, &QTimer::timeout, this, [this, textWidth]() {
        m_scrollAnimation = new QPropertyAnimation(this, "scrollOffset", this);
        m_scrollAnimation->setDuration((textWidth + 20) * 15); // ~15ms per pixel
        m_scrollAnimation->setStartValue(0);
        m_scrollAnimation->setEndValue(textWidth + 20);
        m_scrollAnimation->setEasingCurve(QEasingCurve::Linear);
        m_scrollAnimation->setLoopCount(-1); // Infinite loop
        m_scrollAnimation->start();
    });
    m_pauseTimer->start(500); // 500ms pause before scrolling
}

void ScrollingLabel::stopScrolling() {
    if (m_scrollAnimation) {
        m_scrollAnimation->stop();
        m_scrollAnimation->deleteLater();
        m_scrollAnimation = nullptr;
    }
    if (m_pauseTimer) {
        m_pauseTimer->stop();
        m_pauseTimer->deleteLater();
        m_pauseTimer = nullptr;
    }
    m_scrollOffset = 0;
    update();
}
