#pragma once

#include <core/app_message/app_message.h>

namespace VoiceAssistantService {
void begin();
VoiceAssistantPhase phase();
void setPhase(VoiceAssistantPhase phase);
const char* label(VoiceAssistantPhase phase);
}
