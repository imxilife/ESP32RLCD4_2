#pragma once

// Install I2S legacy driver (IDF 4.x) for duplex echo.
// GPIO: MCLK=16, BCLK=9, WS=45, DOUT=8 (TX→ES8311), DIN=10 (RX←ES7210)
// Mode: Philips I2S, 32-bit stereo, MCLK = 256 * sampleRate
// Returns true on success.
bool initI2s(int sampleRate);
void deinitI2s();
