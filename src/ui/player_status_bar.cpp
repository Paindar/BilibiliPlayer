#include "player_status_bar.h"
#include "ui_player_status_bar.h"
#include <manager/application_context.h>
#include <audio/audio_player_controller.h>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <log/log_manager.h>
#include <config/config_manager.h>

PlayerStatusBar::PlayerStatusBar(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui_PlayerStatusBar())
{
    ui->setupUi(this);

    auto* audioController = AUDIO_PLAYER_CONTROLLER;

    if (ui->btnPrev) {
        connect(ui->btnPrev, &QPushButton::clicked, this, [audioController]() {
            if (audioController) audioController->previous();
        });
    }
    if (ui->btnNext) {
        connect(ui->btnNext, &QPushButton::clicked, this, [audioController]() {
            if (audioController) audioController->next();
        });
    }
    if (ui->btnPlayPause) {
        connect(ui->btnPlayPause, &QPushButton::clicked, this, [this, audioController]() {
            if (!audioController) return;
            if (audioController->isPlaying()) audioController->pause(); else audioController->play();
        });
    }
    if (ui->btnPlayMode) {
        connect(ui->btnPlayMode, &QPushButton::clicked, this, [audioController]() {
            if (!audioController) return;
            auto mode = audioController->getPlayMode();
            using namespace playlist;
            playlist::PlayMode next = playlist::PlayMode::PlaylistLoop;
            if (mode == PlayMode::PlaylistLoop) next = PlayMode::SingleLoop;
            else if (mode == PlayMode::SingleLoop) next = PlayMode::Random;
            else next = PlayMode::PlaylistLoop;
            audioController->setPlayMode(next);
        });
    }

    static float lastVolumeBeforeMute = 0.8f;
    if (ui->volumeSlider) {
        connect(ui->volumeSlider, &QSlider::valueChanged, this, [audioController](int value) {
            if (!audioController) return;
            float v = static_cast<float>(value) / 100.0f;
            audioController->setVolume(v);
        });
    }
    if (ui->btnMute) {
        connect(ui->btnMute, &QPushButton::clicked, this, [this, audioController]() {
            if (!audioController) return;
            float cur = audioController->getVolume();
            if (cur > 0.001f) {
                lastVolumeBeforeMute = cur;
                audioController->setVolume(0.0f);
                ui->btnMute->setText("Unmute");
                if (ui->volumeSlider) ui->volumeSlider->setValue(0);
            } else {
                audioController->setVolume(lastVolumeBeforeMute);
                ui->btnMute->setText("Mute");
                if (ui->volumeSlider) ui->volumeSlider->setValue(static_cast<int>(lastVolumeBeforeMute * 100.0f));
            }
        });
    }

    if (audioController) {
        connect(audioController, &audio::AudioPlayerController::currentSongChanged, this, [this](const playlist::SongInfo& song, int index){
            if (ui->trackTitleLabel) ui->trackTitleLabel->setText(song.title);
            if (ui->uploaderLabel) ui->uploaderLabel->setText(song.uploader);
            if (ui->progressSlider) ui->progressSlider->setRange(0, song.duration);
            if (ui->coverLabel) {
                if (!song.coverName.isEmpty()) {
                    QString dirPath = CONFIG_MANAGER->getCoverCacheDirectory();
                    QString coverPath = dirPath + "/" + song.coverName;
                    
                    QPixmap pix(coverPath);
                    if (!pix.isNull()) {
                        QPixmap scaled = pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        ui->coverLabel->setPixmap(scaled);
                    } else {
                        LOG_WARN("Failed to load cover image: {}", coverPath.toStdString());
                        ui->coverLabel->setText("No Cover");
                    }
                } else {
                    ui->coverLabel->setText("No Cover");
                }
            }
            m_currentCoverPath = song.filepath;
            LOG_INFO(" Now playing: {} by {} (Index: {}), duration {}, cover {}", 
                song.title.toStdString(), song.uploader.toStdString(), index, 
                song.duration, song.coverName.toStdString());
        });

        connect(audioController, &audio::AudioPlayerController::positionChanged, this, [this](qint64 posMs){
            if (ui->progressSlider) ui->progressSlider->setValue(posMs);
        });
        connect(audioController, &audio::AudioPlayerController::playbackStateChanged, this, [this](audio::PlaybackState state){
            if (!ui->btnPlayPause) return;
            if (state == audio::PlaybackState::Playing) 
                ui->btnPlayPause->setText("Pause"); 
            else ui->btnPlayPause->setText("Play");
        });
        
    }
}

PlayerStatusBar::~PlayerStatusBar() 
{
    delete ui;
}


void PlayerStatusBar::onNetworkDownload(QString url, QString savePath)
{
    if (savePath == m_currentCoverPath) {
        if (ui->coverLabel) {
            QPixmap pix(savePath);
            if (!pix.isNull()) ui->coverLabel->setPixmap(pix.scaled(64,64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

// void PlayStatusBar::onPlaybackStateChanged(int state) {
//     if (!ui->btnPlayPause) return;
//     if (state == audio::PlaybackState::Playing) 
//         ui->btnPlayPause->setText("Pause"); 
//     else ui->btnPlayPause->setText("Play");
// }

// void PlayerStatusBar::onPlaybackCompleted() {
//     // Handle playback completed event
// }

// void PlayerStatusBar::currentSongChanged(const playlist::SongInfo& song, int index) {
//     if (ui->trackTitleLabel) ui->trackTitleLabel->setText(song.title);
//     if (ui->uploaderLabel) ui->uploaderLabel->setText(song.uploader);
//     if (ui->coverLabel) {
//         if (!song.filepath.isEmpty()) {
//             QPixmap pix(song.filepath);
//             if (!pix.isNull()) ui->coverLabel->setPixmap(pix.scaled(64,64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
//         }
//     }
// }

// void PlayerStatusBar::positionChanged(qint64 positionMs) {
//     if (ui->progressSlider) ui->progressSlider->setValue(static_cast<int>(positionMs / 1000));
// }

// void PlayerStatusBar::durationChanged(qint64 durationMs) {
//     if (ui->progressSlider) ui->progressSlider->setRange(0, static_cast<int>(durationMs / 1000));
// }