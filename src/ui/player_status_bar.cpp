#include "player_status_bar.h"
#include "ui_player_status_bar.h"
#include <manager/application_context.h>
#include <audio/audio_player_controller.h>
#include <ui/util/elided_label.h>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QEvent>
#include <QTimer>
#include <QPropertyAnimation>
#include <log/log_manager.h>
#include <config/config_manager.h>

PlayerStatusBar::PlayerStatusBar(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui_PlayerStatusBar())
{
    ui->setupUi(this);
    
    // Configure ScrollingLabel properties
    ui->trackTitleLabel->setAutoScroll(true);
    ui->uploaderLabel->setAutoScroll(true);

    m_lastVolumeBeforeMute = 0.8f;
    
    auto* audioController = AUDIO_PLAYER_CONTROLLER;

    // Connect button signals
    if (ui->btnPrev) {
        connect(ui->btnPrev, &QPushButton::clicked, this, &PlayerStatusBar::onPreviousClicked);
    }
    if (ui->btnNext) {
        connect(ui->btnNext, &QPushButton::clicked, this, &PlayerStatusBar::onNextClicked);
    }
    if (ui->btnPlayPause) {
        connect(ui->btnPlayPause, &QPushButton::clicked, this, &PlayerStatusBar::onPlayPauseClicked);
    }
    if (ui->btnPlayMode) {
        connect(ui->btnPlayMode, &QPushButton::clicked, this, &PlayerStatusBar::onPlayModeClicked);
    }
    if (ui->volumeSlider) {
        connect(ui->volumeSlider, &QSlider::valueChanged, this, &PlayerStatusBar::onVolumeChanged);
    }
    if (ui->btnMute) {
        connect(ui->btnMute, &QPushButton::clicked, this, &PlayerStatusBar::onMuteClicked);
    }

    // Connect audio controller signals
    if (audioController) {
        connect(audioController, &audio::AudioPlayerController::currentSongChanged, 
                this, &PlayerStatusBar::onCurrentSongChanged);
        connect(audioController, &audio::AudioPlayerController::positionChanged, 
                this, &PlayerStatusBar::onPositionChanged);
        connect(audioController, &audio::AudioPlayerController::playbackStateChanged, 
                this, &PlayerStatusBar::onPlaybackStateChanged);
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
            if (!pix.isNull()) {
                ui->coverLabel->setPixmap(pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
    }
}

void PlayerStatusBar::onPreviousClicked()
{
    auto* audioController = AUDIO_PLAYER_CONTROLLER;
    if (audioController) {
        audioController->previous();
    }
}

void PlayerStatusBar::onNextClicked()
{
    auto* audioController = AUDIO_PLAYER_CONTROLLER;
    if (audioController) {
        audioController->next();
    }
}

void PlayerStatusBar::onPlayPauseClicked()
{
    auto* audioController = AUDIO_PLAYER_CONTROLLER;
    if (!audioController) return;
    
    if (audioController->isPlaying()) {
        audioController->pause();
    } else {
        audioController->play();
    }
}

void PlayerStatusBar::onPlayModeClicked()
{
    auto* audioController = AUDIO_PLAYER_CONTROLLER;
    if (!audioController) return;
    
    auto mode = audioController->getPlayMode();
    using namespace playlist;
    
    PlayMode next = PlayMode::PlaylistLoop;
    if (mode == PlayMode::PlaylistLoop) {
        next = PlayMode::SingleLoop;
    } else if (mode == PlayMode::SingleLoop) {
        next = PlayMode::Random;
    } else {
        next = PlayMode::PlaylistLoop;
    }
    
    audioController->setPlayMode(next);
}

void PlayerStatusBar::onVolumeChanged(int value)
{
    auto* audioController = AUDIO_PLAYER_CONTROLLER;
    if (!audioController) return;
    
    float volume = static_cast<float>(value) / 100.0f;
    audioController->setVolume(volume);
}

void PlayerStatusBar::onMuteClicked()
{
    auto* audioController = AUDIO_PLAYER_CONTROLLER;
    if (!audioController) return;
    
    float currentVolume = audioController->getVolume();
    if (currentVolume > 0.001f) {
        m_lastVolumeBeforeMute = currentVolume;
        audioController->setVolume(0.0f);
        ui->btnMute->setText("Unmute");
        if (ui->volumeSlider) {
            ui->volumeSlider->setValue(0);
        }
    } else {
        audioController->setVolume(m_lastVolumeBeforeMute);
        ui->btnMute->setText("Mute");
        if (ui->volumeSlider) {
            ui->volumeSlider->setValue(static_cast<int>(m_lastVolumeBeforeMute * 100.0f));
        }
    }
}

void PlayerStatusBar::onCurrentSongChanged(const playlist::SongInfo& song, int index)
{
    if (ui->trackTitleLabel) {
        ui->trackTitleLabel->setText(song.title);
    }
    if (ui->uploaderLabel) {
        ui->uploaderLabel->setText(song.uploader);
    }
    if (ui->progressSlider) {
        ui->progressSlider->setRange(0, song.duration);
    }
    
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
    LOG_INFO("Now playing: {} by {} (Index: {}), duration {}, cover {}", 
        song.title.toStdString(), song.uploader.toStdString(), index, 
        song.duration, song.coverName.toStdString());
}

void PlayerStatusBar::onPositionChanged(qint64 posMs)
{
    if (ui->progressSlider) {
        ui->progressSlider->setValue(posMs);
    }
}

void PlayerStatusBar::onPlaybackStateChanged(audio::PlaybackState state)
{
    if (!ui->btnPlayPause) return;
    
    if (state == audio::PlaybackState::Playing) {
        ui->btnPlayPause->setText("Pause");
    } else {
        ui->btnPlayPause->setText("Play");
    }
}