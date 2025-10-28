#include "ui/main_window.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QTimer>
#include "manager/application_context.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    // Set application properties
    a.setApplicationName("BilibiliPlayer");
    a.setApplicationVersion("1.0.0");

    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "BilibiliPlayer_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    QTimer::singleShot(100, [&w]() {
        try {
            // Initialize with default workspace directory
            APP_CONTEXT.initialize();
        } catch (const std::exception& e) {
            qCritical() << "Initialization failed:" << e.what();
            QApplication::quit();
        }
    });
    
    w.show();

    // Handle cleanup on app quit
    QObject::connect(&a, &QApplication::aboutToQuit, []() {
        APP_CONTEXT.shutdown();
    });
    return a.exec();
}
