#include <media/audio/I2sDriver.h>
#include "driver/i2s.h"

#define AUDIO_I2S_PORT  I2S_NUM_0

bool initI2s(int sampleRate) {
    // Philips I2S, master, TX+RX, 32-bit stereo
    // BCLK = 2 * 32 * sampleRate; MCLK = 256 * sampleRate → MCLK/BCLK = 4
    // Matches ES8311 coeff table bclk_div=4 for mclk=4096000, rate=16000
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
        .sample_rate          = (uint32_t)sampleRate,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = 0,
        .dma_buf_count        = 3,
        .dma_buf_len          = 64,   // 64 frames * 2ch * 4B = 512 B ≈ 4ms @16kHz
        .use_apll             = true,   // exact MCLK = 256*sampleRate via APLL
        .tx_desc_auto_clear   = true, // output zeros when TX starved
        .fixed_mclk           = 0,
        .mclk_multiple        = I2S_MCLK_MULTIPLE_256,
        .bits_per_chan         = I2S_BITS_PER_CHAN_32BIT,
    };

    if (i2s_driver_install(AUDIO_I2S_PORT, &cfg, 0, nullptr) != ESP_OK)
        return false;

    i2s_pin_config_t pins = {
        .mck_io_num   = 16,
        .bck_io_num   = 9,
        .ws_io_num    = 45,
        .data_out_num = 8,
        .data_in_num  = 10,
    };

    if (i2s_set_pin(AUDIO_I2S_PORT, &pins) != ESP_OK) {
        i2s_driver_uninstall(AUDIO_I2S_PORT);
        return false;
    }

    return true;
}

void deinitI2s() {
    i2s_driver_uninstall(AUDIO_I2S_PORT);
}
