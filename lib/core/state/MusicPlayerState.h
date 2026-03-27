#pragma once

#include <core/state_manager/AbstractState.h>
#include <ui/gui/Gui.h>
#include <media/audio/AudioCodec.h>
#include <media/mp3_player/Mp3Player.h>
#include <device/sdcard/SDCard.h>

// 音乐播放器状态：
//   KEY2 短按 → MP3 播放（SD 卡 /music/ 目录），无 MP3 时退回录音回放
//   KEY2 长按 → 录音回放（先录后放，避免声学反馈）
//   KEY1 → 停止播放 → 切换到下一状态
class MusicPlayerState : public AbstractState {
public:
    explicit MusicPlayerState(Gui& gui);

    void onEnter()                         override;
    void onExit()                          override;
    void onMessage(const AppMessage& msg)  override;
    void onKeyEvent(const KeyEvent& event) override;

private:
    Gui&       gui_;
    AudioCodec audio_;
    Mp3Player  mp3_;
    SDCard     sd_;

    static constexpr int kMaxPlaylist = 32;
    String playlist_[kMaxPlaylist];
    int    playlistCount_ = 0;
    int    playlistIndex_ = 0;

    bool   sdReady_    = false;
    bool   mp3Ready_   = false;
    bool   audioReady_ = false;

    void drawUI(const char* line1, const char* line2 = nullptr);
    void stopAll();
    void playCurrentTrack();
};
