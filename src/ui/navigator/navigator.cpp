// src/ui/controllers/navigator.cpp
#include "navigator.h"
#include <QUrlQuery>
#include <QWidget>

Navigator::Navigator(QObject* parent, uint maxHistorySize) : QObject(parent), m_maxHistorySize(maxHistorySize) {}

void Navigator::registerPageFactory(const QString& pageType, 
                                   std::function<INavigablePage*()> factory) {
    m_pageFactories[pageType] = factory;
}

void Navigator::navigateTo(const QString& pageType, const QString& params) {
    // Save current page state if exists
    if (m_currentPage) {
        m_currentEntry.fullState = m_currentPage->getNavigationState();
        m_backStack.push(m_currentEntry);
    }
    
    // Clear forward stack (new navigation path)
    m_forwardStack.clear();
    
    // Create new navigation entry
    NavigationEntry newEntry;
    newEntry.pageType = pageType;
    newEntry.fullState = params.isEmpty() ? pageType : pageType + "?" + params;
    
    setCurrentPage(newEntry);
    emit navigationStateChanged(currentNavigationState());
}

void Navigator::navigateBack() {
    if (m_backStack.isEmpty()) return;
    
    // Save current state to forward stack
    if (m_currentPage) {
        m_currentEntry.fullState = m_currentPage->getNavigationState();
        m_forwardStack.push(m_currentEntry);
    }
    
    // Restore previous page
    NavigationEntry previousEntry = m_backStack.pop();
    setCurrentPage(previousEntry);
    emit navigationStateChanged(currentNavigationState());
}

void Navigator::setCurrentPage(const NavigationEntry& entry) {
    INavigablePage* oldPage = m_currentPage;
    
    // Create new page instance
    QString pageType = entry.fullState.split('?')[0];
    m_currentPage = createPage(pageType);
    
    if (m_currentPage) {
        // Restore page state
        m_currentPage->restoreFromState(entry.fullState);
        m_currentEntry = entry;
        
        // Emit signals to update UI
        emit beforePageChange(oldPage);
        emit afterPageChange(m_currentPage);
    }
    
    // Clean up old page AFTER all signal processing is complete
    // Use deleteLater to ensure it's deleted safely after event loop
    if (oldPage) {
        QWidget* oldWidget = dynamic_cast<QWidget*>(oldPage);
        if (oldWidget) {
            oldWidget->deleteLater();  // Safe delayed deletion
        } else {
            delete oldPage;  // Non-widget pages can be deleted immediately
        }
    }
}

INavigablePage* Navigator::createPage(const QString& pageType) {
    auto it = m_pageFactories.find(pageType);
    if (it != m_pageFactories.end()) {
        return it.value()();
    }
    return nullptr;
}

QString Navigator::currentNavigationState() const {
    return m_currentPage ? m_currentPage->getNavigationState() : QString();
}

// signals:
//     void beforePageChange(INavigablePage* currentPage);  // For cleanup
//     void afterPageChange(INavigablePage* newPage);       // For display