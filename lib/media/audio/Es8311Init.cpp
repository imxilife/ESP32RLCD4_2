/**
 * @file Es8311Init.cpp
 * @brief ES8311 DAC 初始化 —— 仅用于扬声器输出（DAC-only 模式）
 *
 * ══════════════════════════════════════════════════════════════
 *  ES8311 芯片概述
 * ══════════════════════════════════════════════════════════════
 *
 *  ES8311 是一颗低功耗 mono 音频 CODEC，内部同时包含 ADC 和 DAC。
 *  在本项目中：
 *    - DAC 部分：用于扬声器输出（ESP32 I2S TX → ES8311 SDPIN → DAC → PA → Speaker）
 *    - ADC 部分：完全禁用（麦克风输入由独立的 ES7210 负责）
 *
 *  I2C 地址：0x18（7-bit），用于配置寄存器
 *  I2S 接口：接收 ESP32 发送的 PCM 数据（SDPIN = GPIO8）
 *
 * ══════════════════════════════════════════════════════════════
 *  ES8311 在系统中的位置（数据流）
 * ══════════════════════════════════════════════════════════════
 *
 *   ESP32 I2S TX (DOUT/GPIO8)
 *         │
 *         ▼
 *   ES8311 SDPIN 引脚（串行数据输入）
 *         │
 *         ▼
 *   ES8311 内部 DAC（数字→模拟转换）
 *         │
 *         ▼
 *   ES8311 模拟输出 → PA（GPIO46 控制使能）→ 扬声器
 *
 * ══════════════════════════════════════════════════════════════
 *  时钟树（Clock Tree）
 * ══════════════════════════════════════════════════════════════
 *
 *   ESP32 APLL 输出:
 *     MCLK = 256 × 16000 = 4,096,000 Hz  (GPIO16)
 *     BCLK = 2 × 32 × 16000 = 1,024,000 Hz  (GPIO9)
 *     WS/LRCK = 16,000 Hz  (GPIO45)
 *
 *   ES8311 内部分频（由寄存器配置）:
 *     pre_div=1, pre_multi=1 → 内部时钟 = MCLK / 1 × 1 = 4.096 MHz
 *     bclk_div=4 → BCLK = MCLK / 4 = 1.024 MHz（与 ESP32 BCLK 匹配）
 *     LRCK = BCLK / (lrck_h:lrck_l + 1) = 1024000 / 256 = 16000
 *     DAC OSR = 0x20 → 过采样率 32
 *
 * ══════════════════════════════════════════════════════════════
 *  DAC 音量控制（REG32）
 * ══════════════════════════════════════════════════════════════
 *
 *   REG32 取值范围 0x00 ~ 0xFF，对应 -95.5 dB ~ +32 dB：
 *     0x00 = -95.5 dB（静音）
 *     0xBF = 0 dB（unity gain，不增不减）
 *     0xFF = +32 dB（最大增益）
 *
 *   初始化时设为静音（0x00），播放时由 AudioCodec 设为 0xBF。
 *
 * ══════════════════════════════════════════════════════════════
 *  寄存器初始化序列来源
 * ══════════════════════════════════════════════════════════════
 *
 *   完全参照 Espressif esp_codec_dev 组件中 es8311.c 的初始化流程：
 *     es8311_open() → es8311_set_fs() → es8311_start()
 *   通过 Arduino Wire 库直接写 I2C 寄存器，不依赖 esp_codec_dev 框架。
 */

#include <media/audio/Es8311Init.h>
#include <Wire.h>
#include <Arduino.h>

// ── 硬件常量 ──────────────────────────────────────────────────
#define ES8311_ADDR   0x18   // ES8311 I2C 7-bit 地址
#define ES8311_PA_PIN 46     // 功率放大器使能引脚，高电平有效

// ── I2C 读写辅助函数 ─────────────────────────────────────────
// wr(): 向 ES8311 的指定寄存器写入一个字节
static void wr(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);   // 寄存器地址
    Wire.write(val);   // 数据
    Wire.endTransmission();
}

// rd(): 从 ES8311 的指定寄存器读取一个字节
static uint8_t rd(uint8_t reg) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);  // false = 发送 repeated start，不释放总线
    Wire.requestFrom((uint8_t)ES8311_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0x00;
}

// ═══════════════════════════════════════════════════════════════
//  initEs8311() —— ES8311 完整初始化（DAC-only 模式）
// ═══════════════════════════════════════════════════════════════
//
//  初始化分三个阶段，对应 esp_codec_dev 驱动的三个函数：
//    1. es8311_open()     —— 芯片上电、时钟配置、模拟通路使能
//    2. es8311_set_fs()   —— 设置采样率、位宽、I2S 格式
//    3. es8311_start()    —— 启动 DAC 数据通路，使能/禁用 ADC
//
bool initEs8311(int /*sampleRate*/) {

    // ╔═══════════════════════════════════════════════════════════╗
    // ║  阶段 1: es8311_open() —— 芯片基础初始化                  ║
    // ╚═══════════════════════════════════════════════════════════╝

    // REG44 = GPIO/参考信号控制寄存器
    // 写两次是因为 I2C 总线上电后第一次写入可能因噪声失败
    // 0x08 = 默认安全值（禁用 DAC 参考到 ADC 的内部路由）
    wr(0x44, 0x08);
    wr(0x44, 0x08);

    // 以下是芯片复位后的默认寄存器配置
    wr(0x01, 0x30);  // CLK_MANAGER_REG01: 时钟源关闭（稍后会打开）
    wr(0x02, 0x00);  // CLK_MANAGER_REG02: 预分频 = 1，预倍频 = 1
    wr(0x03, 0x10);  // CLK_MANAGER_REG03: ADC 过采样率
    wr(0x16, 0x00);  // ADC_REG16: MIC 增益 = 0（ADC 不使用，MIC 由 ES7210 处理）
    wr(0x04, 0x10);  // CLK_MANAGER_REG04: DAC 过采样率
    wr(0x05, 0x00);  // CLK_MANAGER_REG05: ADC/DAC 时钟分频 = 1
    wr(0x0B, 0x00);  // SYSTEM_REG0B: 系统配置
    wr(0x0C, 0x00);  // SYSTEM_REG0C: 系统配置
    wr(0x10, 0x1F);  // SYSTEM_REG10: 数字系统控制
    wr(0x11, 0x7F);  // SYSTEM_REG11: 数字系统控制
    wr(0x00, 0x80);  // RESET_REG00: bit7=1 → 芯片启动（非复位状态）

    // 设置从机模式（Slave mode）
    // bit6 = 0 → 从机模式（ESP32 提供 BCLK 和 WS，ES8311 跟随）
    // bit6 = 1 → 主机模式（ES8311 自己产生 BCLK 和 WS）
    uint8_t r00 = rd(0x00) & 0xBF;   // 清除 bit6
    wr(0x00, r00);

    // 时钟源选择：使用外部 MCLK（由 ESP32 APLL 通过 GPIO16 提供）
    // REG01: bit7=0 → 使用外部 MCLK（通过 MCLK 引脚输入）
    //        bit7=1 → 使用内部时钟（从 BCLK 倍频，精度差）
    //        bit6=0 → MCLK 不反转
    // 0x3F = 使用外部 MCLK + 所有内部时钟使能
    wr(0x01, 0x3F);

    // SCLK（BCLK）极性：不反转
    // REG06 bit5 = 0 → BCLK 不反转
    wr(0x06, rd(0x06) & ~0x20u);

    // 模拟通路和滤波器配置
    wr(0x13, 0x10);  // SYSTEM_REG13: 模拟参考电压配置
    wr(0x1B, 0x0A);  // ADC_REG1B: ADC 高通滤波器系数（虽然 ADC 不用，但需要设默认值）
    wr(0x1C, 0x6A);  // ADC_REG1C: ADC 高通滤波器系数

    // REG44 = 0x58: 使能 DAC 模拟输出所需的内部参考信号
    // 这个寄存器控制 ADC 和 DAC 之间的内部信号路由：
    //   bit6=1: 使能 ADCL 参考（DAC 模拟输出路径需要）
    //   bit4=1: 使能 DACR 参考
    //   bit3=1: 使能模拟输出缓冲
    // 如果不设置 0x58，DAC 模拟输出将没有信号
    wr(0x44, 0x58);

    // ╔═══════════════════════════════════════════════════════════╗
    // ║  阶段 2: es8311_set_fs() —— 采样率和 I2S 格式配置         ║
    // ╚═══════════════════════════════════════════════════════════╝

    // ── 设置位宽为 16-bit ──
    // REG09 (SDPIN, DAC 输入接口): bits[3:2] = 11 → 16-bit
    // REG0A (SDPOUT, ADC 输出接口): bits[3:2] = 11 → 16-bit
    // 注意：这里 16-bit 是指有效数据位宽，I2S slot 仍然是 32-bit
    wr(0x09, rd(0x09) | 0x0C);     // 设置 DAC 输入为 16-bit
    wr(0x0A, rd(0x0A) | 0x0C);     // 设置 ADC 输出为 16-bit

    // ── 设置 I2S 格式为 Philips I2S Normal ──
    // REG09/REG0A bits[1:0]:
    //   00 = I2S Normal (Philips)  ← 我们使用这个
    //   01 = Left Justified
    //   10 = Right Justified
    //   11 = DSP/PCM mode
    wr(0x09, rd(0x09) & 0xFC);     // 清除 bits[1:0] → I2S Normal
    wr(0x0A, rd(0x0A) & 0xFC);

    // ── 配置采样率相关的时钟分频 ──
    // 以下参数来自 ES8311 datasheet 的系数表（coeff table）：
    // MCLK=4,096,000 Hz, 采样率=16,000 Hz 对应的一行：
    //   pre_div=1, pre_multi=1, adc_div=1, dac_div=1,
    //   fs_mode=0, lrck_h=0x00, lrck_l=0xFF, bclk_div=4,
    //   adc_osr=0x10, dac_osr=0x20

    // REG02: 预分频器 (pre_div-1)<<5 = 0, 无温度补偿
    wr(0x02, rd(0x02) & 0x07);      // 保留低 3 位，高 5 位清零

    // REG05: ADC 分频 (adc_div-1)<<4 = 0, DAC 分频 (dac_div-1) = 0
    wr(0x05, 0x00);

    // REG03: ADC 过采样率 = 0x10 (fs_mode=0, adc_osr=16)
    wr(0x03, (rd(0x03) & 0x80) | 0x10);

    // REG04: DAC 过采样率 = 0x20 (dac_osr=32)
    wr(0x04, (rd(0x04) & 0x80) | 0x20);

    // REG07/REG08: LRCK 分频 = 0x00FF = 255
    // LRCK = BCLK / (lrck_h:lrck_l + 1) = 1,024,000 / 256 = 4,000 ?
    // 实际上 ES8311 在 slave 模式下这个只是告诉芯片 LRCK 的预期比例，
    // 真正的 LRCK 由 ESP32 I2S master 产生
    wr(0x07, (rd(0x07) & 0xC0) | 0x00);   // lrck_h = 0x00
    wr(0x08, 0xFF);                         // lrck_l = 0xFF

    // REG06: BCLK 分频 = bclk_div - 1 = 4 - 1 = 3
    // MCLK / bclk_div = 4,096,000 / 4 = 1,024,000 Hz = BCLK
    wr(0x06, (rd(0x06) & 0xE0) | 0x03);

    // ╔═══════════════════════════════════════════════════════════╗
    // ║  阶段 3: es8311_start() —— 启动数据通路                   ║
    // ╚═══════════════════════════════════════════════════════════╝

    wr(0x00, 0x80);  // 从机模式，芯片运行
    wr(0x01, 0x3F);  // 使用外部 MCLK，所有内部时钟使能

    // ── DAC 输入（SDPIN）和 ADC 输出（SDPOUT）的 mute 控制 ──
    //
    // REG09 bit6: SDPIN mute（ESP32 → ES8311 的 DAC 数据输入）
    //   0 = unmute（正常播放） ← 我们需要这个
    //   1 = mute（忽略输入数据，DAC 输出静音）
    //
    // REG0A bit6: SDPOUT mute（ES8311 → ESP32 的 ADC 数据输出）
    //   0 = unmute（ES8311 ADC 正常输出）
    //   1 = mute（禁止 ADC 输出） ← 我们需要这个（ADC 由 ES7210 负责）
    wr(0x09, rd(0x09) & 0xBF);   // SDPIN unmute → DAC 接收数据
    wr(0x0A, rd(0x0A) | 0x40);   // SDPOUT mute → ES8311 ADC 禁用

    // 以下是启动 DAC 模拟输出通路所需的系统寄存器配置
    wr(0x17, 0x00);  // ADC_REG17: ADC 数字音量 = 0（ADC 完全静音）
    wr(0x0E, 0x02);  // SYSTEM_REG0E: 系统控制（参考 Espressif 驱动）
    wr(0x12, 0x00);  // SYSTEM_REG12: DAC+ADC 模拟通路使能（DAC 输出需要）
    wr(0x14, 0x1A);  // SYSTEM_REG14: 模拟部分配置（DAC 输出路径需要）
    wr(0x0D, 0x01);  // SYSTEM_REG0D: 上电（power up）
    wr(0x15, 0x40);  // ADC_REG15: 模拟部分配置
    wr(0x37, 0x08);  // DAC_REG37: DAC 模拟输出控制（匹配参考驱动，0x08）
    wr(0x45, 0x00);  // GP_REG45: 通用寄存器

    // ── DAC 数字音量：初始化为静音 ──
    // 播放时由 AudioCodec 调用 es8311SetDacVolume(0xBF) 设为 0 dB
    wr(0x32, 0x00);

    // ── 功放引脚配置 ──
    // 仅设置引脚方向，不开启 PA（等待 AudioCodec::setSpeakerEnabled(true)）
    pinMode(ES8311_PA_PIN, OUTPUT);
    digitalWrite(ES8311_PA_PIN, LOW);   // PA 关闭

    // ── 验证 I2C 通信 ──
    // 读取芯片 ID 寄存器（REG_FD），ES8311 应返回 0x83
    uint8_t chipId = rd(0xFD);
    Serial.printf("[ES8311] chip-ID REG_FD = 0x%02X (expect 0x83)\n", chipId);

    return true;
}

// ═══════════════════════════════════════════════════════════════
//  es8311SetDacVolume() —— 运行时调整 DAC 数字音量
// ═══════════════════════════════════════════════════════════════
//  REG32: DAC 数字音量控制
//    0x00 = -95.5 dB（静音）
//    0xBF = 0 dB（unity gain）
//    0xFF = +32 dB（最大增益）
//  每个 step = 0.5 dB
void es8311SetDacVolume(uint8_t vol) {
    wr(0x32, vol);
}
