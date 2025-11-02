// Test to verify filepath generation improvements
#include <iostream>
#include <QString>
#include <QCryptographicHash>

QString generateStreamingFilepath_old(const QString& title, const QString& uploader, int platform, const QString& args)
{
    // Old problematic version
    QString platformStr = QString::number(platform);
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(args.toUtf8());
    QString encodedArgs = hash.result().toHex().left(8);
    
    return QString("[%1] %2 - %3 - %4.audio")
                  .arg(platformStr)
                  .arg(encodedArgs)
                  .arg(title)
                  .arg(uploader);
}

QString generateStreamingFilepath_new(const QString& title, const QString& uploader, int platform, const QString& args)
{
    // New improved version
    QString platformStr = QString::number(platform);
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(args.toUtf8());
    hash.addData(title.toUtf8());
    hash.addData(uploader.toUtf8());
    QString fileHash = hash.result().toHex();
    
    return QString("%1_%2.audio").arg(platformStr).arg(fileHash);
}

int main() {
    // Test with problematic strings
    QString longTitle = "【MAD】Really Really Really Really Really Really Really Really Really Really Really Really Long Title With Special Characters <>/\\|?*:\"";
    QString longUploader = "超级长的用户名包含各种特殊字符和表情😊🎵🎶♪♫♬🎼🎤🎧🎹🥁🎸🎺🎷📻🔊🔉";
    int platform = 1;
    QString args = "some_streaming_args_data";
    
    std::cout << "=== Filepath Generation Test ===" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Input Data:" << std::endl;
    std::cout << "Title: " << longTitle.toStdString() << std::endl;
    std::cout << "Uploader: " << longUploader.toStdString() << std::endl;
    std::cout << "Platform: " << platform << std::endl;
    std::cout << "Args: " << args.toStdString() << std::endl;
    std::cout << std::endl;
    
    QString oldPath = generateStreamingFilepath_old(longTitle, longUploader, platform, args);
    QString newPath = generateStreamingFilepath_new(longTitle, longUploader, platform, args);
    
    std::cout << "Old Method (Problematic):" << std::endl;
    std::cout << "Length: " << oldPath.length() << " characters" << std::endl;
    std::cout << "Path: " << oldPath.toStdString() << std::endl;
    std::cout << std::endl;
    
    std::cout << "New Method (Improved):" << std::endl;
    std::cout << "Length: " << newPath.length() << " characters" << std::endl;
    std::cout << "Path: " << newPath.toStdString() << std::endl;
    std::cout << std::endl;
    
    // Test uniqueness
    QString differentArgs = "different_streaming_args";
    QString newPath2 = generateStreamingFilepath_new(longTitle, longUploader, platform, differentArgs);
    
    std::cout << "Uniqueness Test:" << std::endl;
    std::cout << "Same song, different args: " << newPath2.toStdString() << std::endl;
    std::cout << "Are they different? " << (newPath != newPath2 ? "YES" : "NO") << std::endl;
    
    return 0;
}