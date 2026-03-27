#pragma once

// Initialize ES7210 microphone ADC codec via Arduino Wire.
// Configures MIC1+MIC2, 16-bit, I2S Normal format, slave mode, 30dB gain.
// Must be called after Wire.begin().
// Returns true on success.
bool initEs7210();

// Dump key ES7210 register values to Serial for diagnostic verification.
void dumpEs7210Regs();
