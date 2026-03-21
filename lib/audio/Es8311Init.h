#pragma once
#include <stdint.h>

// Initialize ES8311 as DAC-only (speaker output). ES8311's own ADC is disabled;
// mic input is handled by ES7210. DAC volume defaults to 0x00 (muted).
// Must be called after Wire.begin(). Returns true on success.
bool initEs8311(int sampleRate = 16000);

// Set ES8311 DAC digital volume via I2C.
// vol: 0x00 = -95.5 dB (muted), 0xBF ≈ 0 dB, 0xFF = +32 dB.
void es8311SetDacVolume(uint8_t vol);
