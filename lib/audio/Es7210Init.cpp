#include "Es7210Init.h"
#include <Wire.h>
#include <Arduino.h>

// ── hardware constants ────────────────────────────────────────
#define ES7210_ADDR  0x40

// ── Wire helpers ─────────────────────────────────────────────
static void wr7(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ES7210_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static uint8_t rd7(uint8_t reg) {
    Wire.beginTransmission(ES7210_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)ES7210_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0x00;
}

// Read-modify-write: (reg & ~mask) | (mask & data)
static void upd7(uint8_t reg, uint8_t mask, uint8_t data) {
    uint8_t v = rd7(reg);
    wr7(reg, (v & ~mask) | (mask & data));
}

// ── ES7210 init ───────────────────────────────────────────────
// Sequence derived from Espressif esp_codec_dev es7210.c:
//   es7210_open  →  es7210_set_fs(I2S_NORMAL, 16-bit, slave)  →  es7210_start
// MIC selection: MIC1+MIC2 (non-TDM, REG12=0x00)
// Gain: 30dB set explicitly after start (gain index 10 = 30dB in GAIN_xDB enum)
bool initEs7210() {
    // ── es7210_open ──────────────────────────────────────────
    wr7(0x00, 0xFF);  // RESET_REG00: reset
    wr7(0x00, 0x41);
    wr7(0x01, 0x3F);  // CLOCK_OFF_REG01: all clocks off
    wr7(0x09, 0x30);  // TIME_CONTROL0_REG09: chip state cycle
    wr7(0x0A, 0x30);  // TIME_CONTROL1_REG0A: power-on state cycle
    wr7(0x23, 0x2A);  // ADC12_HPF2_REG23
    wr7(0x22, 0x0A);  // ADC12_HPF1_REG22
    wr7(0x20, 0x0A);  // ADC34_HPF2_REG20
    wr7(0x21, 0x2A);  // ADC34_HPF1_REG21

    // Slave mode: clear bit0 of MODE_CONFIG_REG08
    upd7(0x08, 0x01, 0x00);

    // Analog power and bias
    wr7(0x40, 0x43);  // ANALOG_REG40: VMID 5kΩ start
    wr7(0x41, 0x70);  // MIC12_BIAS_REG41: 2.87V
    wr7(0x42, 0x70);  // MIC34_BIAS_REG42: 2.87V
    wr7(0x07, 0x20);  // OSR_REG07
    wr7(0x02, 0xC1);  // MAINCLK_REG02: DLL bypass, clear state

    // ── mic_select(MIC1 + MIC2), not TDM ────────────────────
    // Disable all gain enables first
    for (int i = 0; i < 4; i++) upd7(0x43 + i, 0x10, 0x00);
    wr7(0x4B, 0xFF);  // MIC12_POWER_REG4B: power down
    wr7(0x4C, 0xFF);  // MIC34_POWER_REG4C: power down

    // Enable MIC1 clocks and power
    upd7(0x01, 0x0B, 0x00);  // CLOCK_OFF_REG01: enable MIC1/2 clocks
    wr7(0x4B, 0x00);          // MIC12_POWER_REG4B: power up MIC1+MIC2
    upd7(0x43, 0x10, 0x10);  // MIC1_GAIN_REG43: gain enable
    upd7(0x43, 0x0F, 0x00);  // MIC1 gain = 0dB initially (set to 30dB after start)

    // Enable MIC2
    upd7(0x44, 0x10, 0x10);  // MIC2_GAIN_REG44: gain enable
    upd7(0x44, 0x0F, 0x00);  // MIC2 gain = 0dB initially

    // 2 mics, not TDM
    wr7(0x12, 0x00);  // SDP_INTERFACE2_REG12: non-TDM

    // Save clock-off reg before starting (used by es7210_start)
    uint8_t offReg = rd7(0x01);

    // ── es7210_set_fs(16bit, I2S_NORMAL, slave) ─────────────
    // set_bits(16): REG11 bits[7:5] = 0x60
    wr7(0x11, (rd7(0x11) & 0x1F) | 0x60);

    // config_fmt(I2S_NORMAL): clear bits[1:0] of REG11
    wr7(0x11, rd7(0x11) & 0xFC);

    // config_sample: slave mode → no-op

    // ── es7210_start ─────────────────────────────────────────
    wr7(0x01, offReg);   // restore saved clock reg
    wr7(0x06, 0x00);     // POWER_DOWN_REG06: power on
    wr7(0x40, 0x43);
    wr7(0x47, 0x08);     // MIC1_POWER_REG47
    wr7(0x48, 0x08);     // MIC2_POWER_REG48
    wr7(0x49, 0x08);     // MIC3_POWER_REG49
    wr7(0x4A, 0x08);     // MIC4_POWER_REG4A

    // mic_select called again inside start (with codec->gain=0 in original)
    for (int i = 0; i < 4; i++) upd7(0x43 + i, 0x10, 0x00);
    wr7(0x4B, 0xFF);
    wr7(0x4C, 0xFF);
    upd7(0x01, 0x0B, 0x00);
    wr7(0x4B, 0x00);
    upd7(0x43, 0x10, 0x10);
    upd7(0x43, 0x0F, 0x00);  // gain=0 (overridden below)
    upd7(0x44, 0x10, 0x10);
    upd7(0x44, 0x0F, 0x00);  // gain=0 (overridden below)
    wr7(0x12, 0x00);
    wr7(0x40, 0x43);
    wr7(0x00, 0x71);     // RESET_REG00: start sequence
    wr7(0x00, 0x41);

    // Explicitly set 30dB gain for MIC1+MIC2 (GAIN_30DB = index 10 = 0x0A)
    upd7(0x43, 0x0F, 10);
    upd7(0x44, 0x0F, 10);

    // Verify I2C comms: REG00 should be 0x41 after reset/init
    uint8_t reg00 = rd7(0x00);
    Serial.printf("[ES7210] REG00 = 0x%02X (expect 0x41)\n", reg00);

    return true;
}
