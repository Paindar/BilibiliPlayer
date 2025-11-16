#include "ui/main_window.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QTimer>
#include <QImageReader>
#include <manager/application_context.h>
#include <exception>
#include <csignal>
#include <iostream>
#include <log/log_manager.h>
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

int main(int argc, char *argv[])
{
    // Install terminate handler to capture uncaught exceptions and make them
    // visible in logs / debugger (calls abort() so debugger will stop).
    std::set_terminate([]() {
        try {
            if (std::current_exception()) {
                try { std::rethrow_exception(std::current_exception()); }
                catch (const std::exception &e) {
                    std::cerr << "terminate due to exception: " << e.what() << std::endl;
                }
                catch (...) {
                    std::cerr << "terminate due to unknown non-std exception" << std::endl;
                }
            } else {
                std::cerr << "terminate called without an active exception" << std::endl;
            }
        } catch (...) {}
#ifdef _WIN32
        // Print stack trace on Windows before aborting
        SymInitialize(GetCurrentProcess(), NULL, TRUE);
        void* stack[64];
        WORD frames = CaptureStackBackTrace(0, 64, stack, NULL);
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
        symbol->MaxNameLen = 255;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        std::cerr << "Stack trace (most recent call first):\n";
        for (WORD i = 0; i < frames; ++i) {
            DWORD64 address = (DWORD64)(stack[i]);
            if (SymFromAddr(GetCurrentProcess(), address, 0, symbol)) {
                std::cerr << frames - i - 1 << ": " << symbol->Name << " - 0x" << std::hex << symbol->Address << std::dec << "\n";
            } else {
                std::cerr << frames - i - 1 << ": " << "0x" << std::hex << address << std::dec << "\n";
            }
        }
        free(symbol);
        SymCleanup(GetCurrentProcess());
#endif
        std::abort();
    });

    // Basic signal handlers to convert signals into abort() so debugger can catch
    std::signal(SIGSEGV, [](int){ std::cerr << "fatal: SIGSEGV" << std::endl; std::abort(); });
    std::signal(SIGABRT, [](int){ std::cerr << "fatal: SIGABRT" << std::endl; std::abort(); });
    std::signal(SIGFPE,  [](int){ std::cerr << "fatal: SIGFPE" << std::endl;  std::abort(); });
    std::signal(SIGILL,  [](int){ std::cerr << "fatal: SIGILL" << std::endl;  std::abort(); });
    QApplication a(argc, argv);

    QTranslator translator;
    // Set application properties
    a.setApplicationName("BilibiliPlayer");
    a.setApplicationVersion("1.0.0");

    try {
        // Initialize with default workspace directory. Keep this fast; defer
        // non-critical UI init (translator load, image format logging) until
        // after the event loop starts to improve cold-start time.
        APP_CONTEXT.initialize();
    } catch (const std::exception& e) {
        qCritical() << "Initialization failed:" << e.what();
        QApplication::quit();
    }
    MainWindow w;
    
    w.show();

    // Defer loading translators and logging image formats to the event loop
    // so startup isn't blocked by optional initialization. This is non-invasive
    // — translator variable remains in-scope for the lifetime of the app.
    QTimer::singleShot(0, [&a, &translator]() {
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &locale : uiLanguages) {
            const QString baseName = "BilibiliPlayer_" + QLocale(locale).name();
            if (translator.load(":/i18n/" + baseName)) {
                a.installTranslator(&translator);
                break;
            }
        }

        // Log supported image formats (non-critical diagnostic)
        QList<QByteArray> formats = QImageReader::supportedImageFormats();
        QString formatList;
        for (const QByteArray& fmt : formats) {
            formatList += QString::fromUtf8(fmt) + " ";
        }
        LOG_INFO("Qt supported image formats: {}", formatList.trimmed().toStdString());
    });

    // No special harness flags here; keep startup behavior unchanged.

    // Handle cleanup on app quit (wrap in try/catch to ensure errors are logged)
    QObject::connect(&a, &QApplication::aboutToQuit, []() {
        try {
            APP_CONTEXT.shutdown();
        } catch (const std::exception& e) {
            std::cerr << "Exception during APP_CONTEXT.shutdown(): " << e.what() << std::endl;
            std::abort();
        } catch (...) {
            std::cerr << "Unknown exception during APP_CONTEXT.shutdown()" << std::endl;
            std::abort();
        }
    });

    int execRet = a.exec();
    std::cerr << "Application exiting with code: " << execRet << std::endl;
    return execRet;
}
