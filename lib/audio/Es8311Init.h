#pragma once

// Initialize ES8311 DAC/speaker codec via Arduino Wire.
// Must be called after Wire.begin(). PA_PIN (GPIO46) is driven HIGH on success.
// Returns true on success.
bool initEs8311(int sampleRate = 16000);
