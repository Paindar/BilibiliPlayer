#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class Ui_PlayerStatusBar;
QT_END_NAMESPACE

class PlayerStatusBar : public QWidget
{
    Q_OBJECT
public:
    explicit PlayerStatusBar(QWidget* parent = nullptr);
    virtual ~PlayerStatusBar();
public slots:
    void onNetworkDownload(QString url, QString savePath);
    // void onPlaybackStateChanged(int state);
    // void onPlaybackCompleted();
    // void currentSongChanged(const playlist::SongInfo& song, int index);
    // void positionChanged(qint64 positionMs);
    // void durationChanged(qint64 durationMs);
    // void onPlayModeChanged(playlist::PlayMode mode);
    // void onVolumeChanged(float volume);

private:
    Ui_PlayerStatusBar* ui;
    QString m_currentCoverPath;
};
