#include "searching_page.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>

SearchingPage::SearchingPage(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

SearchingPage::~SearchingPage() = default;

void SearchingPage::setupUI()
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);
    
    // Status label
    m_statusLabel = new QLabel(tr("Searching..."), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("font-size: 14px; color: #888888;");
    layout->addWidget(m_statusLabel);
    
    // Progress bar (indeterminate by default)
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setValue(0);
    m_progressBar->setMinimumHeight(30);
    layout->addWidget(m_progressBar);
    
    // Add stretch to center content
    layout->addStretch();
    
    setLayout(layout);
    
    // Light styling
    setStyleSheet(
        "SearchingPage { background-color: #fafafa; }"
        "QProgressBar { border: 1px solid #cccccc; border-radius: 5px; }"
        "QProgressBar::chunk { background-color: #3498db; }"
    );
}

void SearchingPage::setStatusText(const QString& text)
{
    if (m_statusLabel) {
        m_statusLabel->setText(text);
    }
}

void SearchingPage::setProgress(int value)
{
    if (m_progressBar) {
        m_progressBar->setValue(value);
    }
}
