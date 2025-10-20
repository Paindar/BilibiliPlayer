#pragma once

#include <QObject>
#include <QStack>
#include <QHash>
#include <functional>
#include "i_navigable_page.h"

class Navigator : public QObject {
    Q_OBJECT
    
public:
    explicit Navigator(QObject* parent = nullptr, uint maxHistorySize = 50);
    
    // Page registration
    void registerPageFactory(const QString& pageType, 
                           std::function<INavigablePage*()> factory);
    
    // Navigation methods
    void navigateTo(const QString& pageType, const QString& params = "");
    void navigateBack();
    void navigateForward();
    
    // State management
    QString currentNavigationState() const;
    void restoreNavigationState(const QString& state);
    
    // History management
    QStringList getNavigationHistory() const;
    void clearHistory();
    
    INavigablePage* currentPage() const { return m_currentPage; }
    
signals:
    void beforePageChange(INavigablePage* page);
    void afterPageChange(INavigablePage* newPage);
    void navigationStateChanged(const QString& state);
    
private:
    struct NavigationEntry {
        QString pageType;
        QString fullState;  // "pageType?param1=value1&param2=value2"
    };
    
    QStack<NavigationEntry> m_backStack;
    QStack<NavigationEntry> m_forwardStack;
    NavigationEntry m_currentEntry;
    uint m_maxHistorySize;
    
    QHash<QString, std::function<INavigablePage*()>> m_pageFactories;
    INavigablePage* m_currentPage = nullptr;
    
    INavigablePage* createPage(const QString& pageType);
    void setCurrentPage(const NavigationEntry& entry);
};