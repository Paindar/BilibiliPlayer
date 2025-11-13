#pragma once

#include <QWidget>

namespace audio {
    enum class PlaybackState;
}

namespace playlist {
    struct SongInfo;
    enum class PlayMode;
}

QT_BEGIN_NAMESPACE
class Ui_PlayerStatusBar;
class QPropertyAnimation;
QT_END_NAMESPACE

class ScrollingLabel;

class PlayerStatusBar : public QWidget
{
    Q_OBJECT
public:
    explicit PlayerStatusBar(QWidget* parent = nullptr);
    virtual ~PlayerStatusBar();
public slots:
    void onNetworkDownload(QString url, QString savePath);
    void onPreviousClicked();
    void onNextClicked();
    void onPlayPauseClicked();
    void onPlayModeClicked();
    void onVolumeChanged(int value);
    void onMuteClicked();
    void onCurrentSongChanged(const playlist::SongInfo& song, int index);
    void onPositionChanged(qint64 posMs);
    void onPlaybackStateChanged(audio::PlaybackState state);

private:
    Ui_PlayerStatusBar* ui;
    QString m_currentCoverPath;
    float m_lastVolumeBeforeMute;
};
