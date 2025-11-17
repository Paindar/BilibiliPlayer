#include <catch2/catch_test_macros.hpp>
#include <QCoreApplication>
#include "test_utils.h"
#include <QTranslator>
#include <QString>

class TestTranslator : public QTranslator {
public:
    TestTranslator(const QString &locale) : locale_(locale) {}
    QString translate(const char *context, const char *sourceText, const char * /*disambiguation*/ , int /*n*/ ) const override {
        QString ctx = QString::fromUtf8(context);
        QString key = QString::fromUtf8(sourceText);
        if (locale_ == "en") {
            if (key == "greeting") return QString::fromUtf8("Hello");
            if (key == "farewell") return QString::fromUtf8("Goodbye");
        } else if (locale_ == "zh") {
            if (key == "greeting") return QString::fromUtf8("你好");
            if (key == "farewell") return QString::fromUtf8("再见");
        }
        return QString::fromUtf8(sourceText);
    }
private:
    QString locale_;
};

TEST_CASE("QTranslator-based simple translations", "[i18n]") {
    testutils::ensureQCoreApplication();

    TestTranslator en("en");
    TestTranslator zh("zh");

    // Install English translator
    QCoreApplication::installTranslator(&en);
    REQUIRE(QString(QCoreApplication::translate("App", "greeting")) == QString("Hello"));

    // Install Chinese translator (takes precedence)
    QCoreApplication::installTranslator(&zh);
    REQUIRE(QString(QCoreApplication::translate("App", "greeting")) == QString::fromUtf8("你好"));

    // Uninstall translators by letting them go out of scope (no explicit uninstall needed here)
}
