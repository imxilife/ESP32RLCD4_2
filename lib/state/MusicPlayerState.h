#pragma once

#include <AbstractState.h>
#include <Gui.h>
#include <AudioCodec.h>

// 音乐播放器状态：麦克风实时回声（MIC → Speaker）
// AudioCodec 作为值成员内化，不再依赖 main.cpp 的全局对象。
// begin(16000) 在首次进入时懒初始化。
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

    void drawStatusLine(const char* text);
};
