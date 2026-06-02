#include "VoiceAssistantService.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

extern QueueHandle_t g_msgQueue;

namespace {
VoiceAssistantPhase g_phase = VoiceAssistantPhase::IDLE;
}

namespace VoiceAssistantService {
void begin() {
    // 只建立状态出口；录音、识别、处理和播报链路后续通过 setPhase() 接入。
    setPhase(g_phase);
}

VoiceAssistantPhase phase() {
    return g_phase;
}

void setPhase(VoiceAssistantPhase phase) {
    g_phase = phase;
    if (g_msgQueue == nullptr) return;

    AppMessage msg = {};
    msg.type = MSG_VOICE_ASSISTANT_UPDATE;
    msg.voiceAssistant.phase = g_phase;
    xQueueSend(g_msgQueue, &msg, 0);
}

const char* label(VoiceAssistantPhase phase) {
    switch (phase) {
    case VoiceAssistantPhase::IDLE: return "IDLE";
    case VoiceAssistantPhase::LISTENING: return "Listening...";
    case VoiceAssistantPhase::PROCESSING: return "Processing...";
    }
    return "IDLE";
}
}
