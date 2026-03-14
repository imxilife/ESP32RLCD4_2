#include "Es8311Init.h"
#include <Wire.h>
#include <Arduino.h>

// ── hardware constants ────────────────────────────────────────
#define ES8311_ADDR  0x18
#define ES8311_PA_PIN 46   // Power Amplifier enable, active HIGH

// ── Wire helpers ─────────────────────────────────────────────
static void wr(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static uint8_t rd(uint8_t reg) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)ES8311_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0x00;
}

// ── ES8311 init ───────────────────────────────────────────────
// Sequence derived from Espressif esp_codec_dev es8311.c:
//   es8311_open  →  es8311_set_fs(sampleRate, 16-bit, I2S_NORMAL)  →  es8311_start(BOTH)
//
// For sampleRate=16000, MCLK=256*16000=4,096,000 Hz the coeff table row is:
//   {4096000, 16000, pre_div=1, pre_multi=1, adc_div=1, dac_div=1, fs_mode=0,
//    lrck_h=0x00, lrck_l=0xFF, bclk_div=4, adc_osr=0x10, dac_osr=0x20}
bool initEs8311(int /*sampleRate*/) {
    // ── es8311_open ──────────────────────────────────────────
    // Noise immunity: first I2C write may fail; write REG44 twice
    wr(0x44, 0x08);
    wr(0x44, 0x08);

    // Default register values
    wr(0x01, 0x30);  // CLK_MANAGER_REG01: clock off
    wr(0x02, 0x00);  // CLK_MANAGER_REG02
    wr(0x03, 0x10);  // CLK_MANAGER_REG03
    wr(0x16, 0x24);  // ADC_REG16: MIC gain 12dB
    wr(0x04, 0x10);  // CLK_MANAGER_REG04
    wr(0x05, 0x00);  // CLK_MANAGER_REG05
    wr(0x0B, 0x00);  // SYSTEM_REG0B
    wr(0x0C, 0x00);  // SYSTEM_REG0C
    wr(0x10, 0x1F);  // SYSTEM_REG10
    wr(0x11, 0x7F);  // SYSTEM_REG11
    wr(0x00, 0x80);  // RESET_REG00: codec on

    // Slave mode: read REG00, clear bit6, write back
    uint8_t r00 = rd(0x00) & 0xBF;
    wr(0x00, r00);

    // use_mclk=true (bit7=0), no invert (bit6=0)
    wr(0x01, 0x3F);

    // No SCLK invert: clear bit5 of REG06
    wr(0x06, rd(0x06) & ~0x20u);

    // Additional init
    wr(0x13, 0x10);  // SYSTEM_REG13
    wr(0x1B, 0x0A);  // ADC_REG1B: HPF
    wr(0x1C, 0x6A);  // ADC_REG1C: HPF
    wr(0x44, 0x58);  // GPIO_REG44: internal ref (ADCL+DACR) enabled

    // ── es8311_set_fs(16000, 16-bit, I2S_NORMAL) ────────────
    // set_bits_per_sample(16): REG09 |= 0x0C, REG0A |= 0x0C
    wr(0x09, rd(0x09) | 0x0C);
    wr(0x0A, rd(0x0A) | 0x0C);

    // config_fmt(I2S_NORMAL): clear bits[1:0] of REG09, REG0A
    wr(0x09, rd(0x09) & 0xFC);
    wr(0x0A, rd(0x0A) & 0xFC);

    // config_sample for MCLK=4096000, rate=16000:
    //   REG02: (pre_div-1)<<5 | datmp<<3 = 0
    wr(0x02, rd(0x02) & 0x07);       // keep lower 3 bits, zero upper 5

    //   REG05: (adc_div-1)<<4 | (dac_div-1)<<0 = 0
    wr(0x05, 0x00);

    //   REG03: fs_mode<<6 | adc_osr = 0x10  (keep bit7)
    wr(0x03, (rd(0x03) & 0x80) | 0x10);

    //   REG04: dac_osr = 0x20  (keep bit7)
    wr(0x04, (rd(0x04) & 0x80) | 0x20);

    //   REG07: lrck_h = 0x00  (keep bits[7:6])
    wr(0x07, (rd(0x07) & 0xC0) | 0x00);

    //   REG08: lrck_l = 0xFF
    wr(0x08, 0xFF);

    //   REG06: bclk_div-1 = 3  (keep bits[7:5])
    wr(0x06, (rd(0x06) & 0xE0) | 0x03);

    // ── es8311_start (slave, use_mclk, BOTH mode) ───────────
    wr(0x00, 0x80);  // slave mode, codec on
    wr(0x01, 0x3F);  // use MCLK, no invert

    // Unmute DAC and ADC: clear bit6 of REG09/REG0A
    wr(0x09, rd(0x09) & 0xBF);
    wr(0x0A, rd(0x0A) & 0xBF);

    wr(0x17, 0xBF);  // ADC_REG17: ADC volume
    wr(0x0E, 0x02);  // SYSTEM_REG0E
    wr(0x12, 0x00);  // SYSTEM_REG12: DAC enabled
    wr(0x14, 0x1A);  // SYSTEM_REG14: analog PGA, no digital mic (bit6=0)
    wr(0x0D, 0x01);  // SYSTEM_REG0D: power up
    wr(0x15, 0x40);  // ADC_REG15
    wr(0x37, 0x08);  // DAC_REG37: ramp rate
    wr(0x45, 0x00);  // GP_REG45

    // DAC digital volume: 0xBF ≈ 0 dB  (range 0x00=-95.5dB … 0xFF=+32dB)
    // Must be written explicitly — after suspend/soft-reset it may be 0x00 (muted)
    wr(0x32, 0xBF);

    // Enable Power Amplifier
    pinMode(ES8311_PA_PIN, OUTPUT);
    digitalWrite(ES8311_PA_PIN, HIGH);

    // Verify I2C comms: chip-ID register 0xFD should read 0x83
    uint8_t chipId = rd(0xFD);
    Serial.printf("[ES8311] chip-ID REG_FD = 0x%02X (expect 0x83)\n", chipId);

    return true;
}
