#pragma once

#include <QFile>
#include <QDir>
#include <QByteArray>
#include <QString>
#include <QCoreApplication>
#include <memory>

namespace testutils {

// Read a file that contains hex bytes (ASCII hex, optional whitespace/newlines)
// and write the decoded binary to `outPath`. Returns true on success.
inline bool writeHexAsset(const QString& hexFilePath, const QString& outPath) {
    QFile in(hexFilePath);
    if (!in.open(QFile::ReadOnly | QFile::Text)) return false;
    QByteArray hex = in.readAll();
    // Remove whitespace/newlines
    QByteArray filtered;
    for (auto c : hex) {
        if (!QByteArray(" \t\r\n").contains(c)) filtered.append(c);
    }
    QByteArray data = QByteArray::fromHex(filtered);
    QDir outDir = QFileInfo(outPath).dir();
    if (!outDir.exists()) outDir.mkpath(".");
    QFile out(outPath);
    if (!out.open(QFile::WriteOnly)) return false;
    qint64 written = out.write(data);
    out.close();
    return written == data.size();
}

// Ensure a QCoreApplication exists for tests that use Qt core features.
// Safe to call multiple times; only one application instance will be created.
inline void ensureQCoreApplication()
{
    if (QCoreApplication::instance()) return;
    int argc = 1;
    static char arg0[] = "test";
    static char* argv[] = { arg0, nullptr };
    static std::unique_ptr<QCoreApplication> s_app = std::make_unique<QCoreApplication>(argc, argv);
    (void)s_app;
}

} // namespace testutils
