#include "search_page.h"
#include "ui_search_page.h"
#include <QUrlQuery>

SearchPage::SearchPage(QWidget *parent)
    : QWidget(parent)
    , INavigablePage(SEARCH_PAGE_TYPE)
    , ui(new Ui_SearchPage)
{
    ui->setupUi(this);
}

SearchPage::~SearchPage()
{
    delete ui;
}

void SearchPage::setSearchText(const QString& searchText)
{
    m_currentSearchText = searchText;
    ui->placeholderLabel->setText(tr("Searching for \"%1\"...").arg(searchText));   
}

QString SearchPage::getNavigationState() const
{
    QString strUriEncoded = m_currentSearchText.toUtf8().toPercentEncoding();
    return QString("search?query=%1").arg(strUriEncoded);
}

void SearchPage::restoreFromState(const QString& state)
{
    QUrlQuery queryPart(state.section('?', 1));
    setSearchText(queryPart.queryItemValue("query"));
    return;
}
