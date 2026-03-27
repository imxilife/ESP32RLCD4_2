#pragma once

#include <ui/gui/Gui.h>
#include <core/app_message/app_message.h>

// 处理 MSG_POMODORO_UPDATE 消息，负责所有番茄时钟界面的绘制。
// 由 main loop 在收到消息后调用，持有 Gui 引用。
void handlePomodoroUpdate(Gui& gui, const AppMessage& msg);
