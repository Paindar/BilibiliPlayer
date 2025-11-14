#pragma once

#include <QWidget>
#include <QString>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <navigator/i_navigable_page.h>
#include <network/platform/i_platform.h>
#include <ui/util/elided_label.h>

QT_BEGIN_NAMESPACE
class Ui_SearchPage;
class QListWidgetItem;
QT_END_NAMESPACE

// Custom widget for search result item display
class SearchResultItemWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit SearchResultItemWidget(const network::SearchResult& result, QWidget* parent = nullptr);
    ~SearchResultItemWidget() = default;
    
    void setCoverImage(const QPixmap& pixmap);
    void updateResult(const network::SearchResult& result);
    
private:
    QLabel* m_coverLabel;
    ScrollingLabel* m_titleLabel;
    ScrollingLabel* m_uploaderLabel;
    QLabel* m_platformLabel;
    QLabel* m_descriptionLabel;
    network::SearchResult m_result;
};


class SearchPage : public QWidget, public INavigablePage
{
    Q_OBJECT
    
public:
    static constexpr const char* SEARCH_PAGE_TYPE = "SearchPage";
    
    enum SearchScope {
        All = 0,
        Bilibili = 1
    };
    
public:
    explicit SearchPage(QWidget *parent = nullptr);
    ~SearchPage();

    void setSearchText(const QString& searchText);

    // INavigablePage interface
    QString getNavigationState() const override;
    void restoreFromState(const QString& state) override;

signals:
    void searchRequested(const QString& keyword, SearchScope scope);

private slots:
    void onSearchButtonClicked();
    void onSearchInputReturnPressed();
    void onScopeButtonClicked();
    void onScopeSelectionChanged();
    
    // NetworkManager slots
    void onSearchCompleted(const QString& keyword);
    void onSearchFailed(const QString& keyword, const QString& errorMessage);
    void onSearchProgress(const QString& keyword, const QList<network::SearchResult>& results);
    
    // UI interaction slots
    void onResultItemDoubleClicked(QListWidgetItem* item);

private:
    void performSearch();
    void showEmptyState();
    void showResults();
    void setupScopeMenu();
    QString convertPlatformEnumToString(network::PlatformType platform) const;
    
private:
    Ui_SearchPage *ui;
    QString m_currentSearchText;
    SearchScope m_currentScope;
    class QMenu* m_scopeMenu;
    class QActionGroup* m_scopeActionGroup;
};