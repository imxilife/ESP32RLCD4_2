#pragma once

#include <core/state_manager/AbstractState.h>
#include <media/mp3_controller/Mp3Controller.h>
#include <ui/gui/Gui.h>

/**功能: 管理 MP3 播放页的歌单浏览和播放控制 */
class MusicPlayerState : public AbstractState {
public:
    explicit MusicPlayerState(Gui& gui);

    void onEnter()                         override;
    void onExit()                          override;
    void onMessage(const AppMessage& msg)  override;
    void onKeyEvent(const KeyEvent& event) override;
    void tick()                            override;

private:
    Gui& gui_;
    Mp3Controller& controller_;

    static constexpr int kVisibleRows = 6;
    int listTopIndex_ = 0;
    int selectedIndex_ = 0;
    int lastRenderedIndex_ = -2;
    Mp3Controller::State lastRenderedState_ = Mp3Controller::State::UNINITIALIZED;
    uint32_t lastRenderedProgressBucket_ = UINT32_MAX;
    uint32_t lastRenderedMarqueeBucket_ = UINT32_MAX;

    void ensureSelectionVisible();
    void syncSelectionToCurrent(const Mp3Controller::Snapshot& snap);
    bool hasVisibleOverflowLabels(const Mp3Controller::Snapshot& snap) const;
    void drawUI();
    void drawPlaylist(const Mp3Controller::Snapshot& snap);
    void drawNowPlaying(const Mp3Controller::Snapshot& snap);
};
