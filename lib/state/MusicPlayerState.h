#pragma once

#include <AbstractState.h>
#include <Gui.h>
#include <AudioCodec.h>

// 音乐播放器状态：录音回放（先录后放，避免声学反馈）
// KEY2 触发录音（3秒）→ 自动播放回放
// KEY1 切换到下一状态
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

    void drawUI(const char* status);
};
