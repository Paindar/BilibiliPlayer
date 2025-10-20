#pragma once

#include <QWidget>
#include "../navigator/i_navigable_page.h"

QT_BEGIN_NAMESPACE
class Ui_SearchPage;
QT_END_NAMESPACE


class SearchPage : public QWidget, public INavigablePage
{
    Q_OBJECT
public:
    static constexpr const char* SEARCH_PAGE_TYPE = "SearchPage";
public:
    explicit SearchPage(QWidget *parent = nullptr);
    ~SearchPage();

    void setSearchText(const QString& searchText);

    // INavigablePage interface
    QString getNavigationState() const override;
    void restoreFromState(const QString& state) override;

private:
    Ui_SearchPage *ui;
    QString m_currentSearchText;
};