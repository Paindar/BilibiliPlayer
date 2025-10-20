#pragma once

#include <QString>

class INavigablePage {
public:
    explicit INavigablePage(const QString& pageType) : m_pageType(pageType) {}
    virtual ~INavigablePage() = default;
    QString pageType() const { return m_pageType; }
    // called to get current navigation state
    virtual QString getNavigationState() const = 0;
    // called when page is being restored
    virtual void restoreFromState(const QString& state) = 0;
private:
    QString m_pageType;
};
