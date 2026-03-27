#include "MusicPlayerState.h"
#include <core/state_manager/StateManager.h>
#include <ui/gui/fonts/Font_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20.h>

MusicPlayerState::MusicPlayerState(Gui& gui)
    : gui_(gui) {}

void MusicPlayerState::onEnter() {
    // 懒初始化音频 codec（仅一次）
    if (!audioReady_) {
        audioReady_ = true;
        if (!audio_.begin(16000)) {
            Serial.println("[Music] Audio codec init failed");
        }
    }

    // 懒初始化 SD 卡
    if (!sdReady_) {
        sdReady_ = true;
        if (!sd_.begin()) {
            Serial.println("[Music] SD card init failed");
        }
    }

    // 懒初始化 MP3 播放器
    if (!mp3Ready_ && sd_.isMounted()) {
        mp3Ready_ = mp3_.begin();
    }

    // 扫描 MP3 文件
    playlistCount_ = 0;
    if (sd_.isMounted() && mp3Ready_) {
        playlistCount_ = mp3_.scanMp3Files("/music", playlist_, kMaxPlaylist);
        if (playlistCount_ == 0) {
            // 根目录也找一下
            playlistCount_ = mp3_.scanMp3Files("/", playlist_, kMaxPlaylist);
        }
    }

    if (playlistCount_ > 0) {
        playlistIndex_ = 0;
        drawUI("MUSIC", "KEY2=PLAY");
    } else {
        drawUI("REC PLAY", "KEY2=REC");
    }
}

void MusicPlayerState::onExit() {
    stopAll();
}

void MusicPlayerState::onMessage(const AppMessage& msg) {
    (void)msg;
}

void MusicPlayerState::onKeyEvent(const KeyEvent& event) {
    if (event.action != KeyAction::DOWN) return;

    if (event.id == KeyId::KEY1) {
        stopAll();
        requestTransition(StateId::CAROUSEL);
    } else if (event.id == KeyId::KEY2) {
        if (playlistCount_ > 0) {
            // MP3 模式
            if (mp3_.isPlaying()) {
                // 正在播放 → 下一曲
                mp3_.stop();
                playlistIndex_ = (playlistIndex_ + 1) % playlistCount_;
                playCurrentTrack();
            } else {
                playCurrentTrack();
            }
        } else {
            // 无 MP3 文件 → 录音回放模式
            if (!audio_.isRunning()) {
                drawUI("REC PLAY", "Recording...");
                audio_.startRecordPlay(3000);
            }
        }
    }
}

void MusicPlayerState::stopAll() {
    if (mp3_.isPlaying()) mp3_.stop();
    if (audio_.isRunning()) audio_.stop();
}

void MusicPlayerState::playCurrentTrack() {
    if (playlistIndex_ < 0 || playlistIndex_ >= playlistCount_) return;

    const String& path = playlist_[playlistIndex_];
    // 提取文件名用于显示
    int lastSlash = path.lastIndexOf('/');
    const char* filename = (lastSlash >= 0) ? path.c_str() + lastSlash + 1
                                            : path.c_str();

    char line2[64];
    snprintf(line2, sizeof(line2), "%d/%d %s",
             playlistIndex_ + 1, playlistCount_, filename);
    drawUI("PLAYING", line2);

    mp3_.play(path.c_str());
}

void MusicPlayerState::drawUI(const char* line1, const char* line2) {
    gui_.clear();
    gui_.setFont(&kFont_chinese_AlibabaPuHuiTi_3_75_SemiBold_20_20);

    static const int kFontW   = 20;
    static const int kScreenW = 400;
    static const int kScreenH = 300;

    int x1 = (kScreenW - (int)strlen(line1) * kFontW) / 2;
    int y1 = kScreenH / 2 - 30;
    if (x1 < 0) x1 = 0;
    gui_.drawText(x1, y1, line1, ColorBlack, ColorWhite);

    if (line2) {
        int x2 = (kScreenW - (int)strlen(line2) * kFontW) / 2;
        int y2 = y1 + 28;
        if (x2 < 0) x2 = 0;
        gui_.drawText(x2, y2, line2, ColorBlack, ColorWhite);
    }
}
