#pragma once

#include <core/state_manager/AbstractState.h>
#include <ui/gui/Gui.h>
#include <media/bluetooth/BluetoothAudio.h>

// 蓝牙 A2DP 播放状态：
//   onEnter: 启动 A2DP Sink 广播，屏幕显示 "BLUETOOTH" + 连接状态
//   onExit:  停止 A2DP，断开连接，恢复 I2S
//   KEY1: 切换到下一状态（MAIN_UI）
//   KEY2: 预留（暂无操作）
class BluetoothState : public AbstractState {
public:
    explicit BluetoothState(Gui& gui);

    void onEnter()                         override;
    void onExit()                          override;
    void onMessage(const AppMessage& msg)  override;
    void onKeyEvent(const KeyEvent& event) override;

private:
    Gui&           gui_;
    BluetoothAudio bt_;
    bool           btReady_ = false;

    // 记录上次显示的连接状态，避免重复刷屏
    bool lastConnected_ = false;

    void drawUI(const char* line1, const char* line2 = nullptr);
};
