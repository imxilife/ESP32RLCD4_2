/**
 * @file Es7210Init.cpp
 * @brief ES7210 ADC 初始化 —— 4 通道麦克风采集芯片
 *
 * ══════════════════════════════════════════════════════════════
 *  ES7210 芯片概述
 * ══════════════════════════════════════════════════════════════
 *
 *  ES7210 是一颗 4 通道 ADC（模拟→数字转换器），支持最多 4 个麦克风输入。
 *  在本项目中只使用了 MIC1（主麦克风）。
 *
 *  I2C 地址：0x40（7-bit），用于配置寄存器
 *  I2S 接口：向 ESP32 发送采集到的音频数据（SDOUT → GPIO10 → ESP32 I2S DIN）
 *
 * ══════════════════════════════════════════════════════════════
 *  ES7210 在系统中的位置（数据流）
 * ══════════════════════════════════════════════════════════════
 *
 *   MIC1（主麦克风）───→ ES7210 MIC1 输入
 *                              │
 *                         模拟前端（偏置、PGA增益）
 *                              │
 *                         Sigma-Delta ADC
 *                              │
 *                         数字滤波（HPF 高通滤波去直流）
 *                              │
 *                              ▼
 *                    I2S SDOUT → GPIO10 → ESP32 I2S RX
 *
 * ══════════════════════════════════════════════════════════════
 *  I2S 数据输出格式
 * ══════════════════════════════════════════════════════════════
 *
 *  模式：I2S Normal (Philips)，非 TDM
 *  位宽：16-bit 有效数据
 *  Slot：32-bit（16-bit 数据 + 16-bit padding，数据左对齐在高位）
 *  声道：Stereo（L = MIC1, R = MIC3 或无信号）
 *
 *  非 TDM 模式下，ES7210 最多输出 2 个 MIC 的数据：
 *    - 选择 MIC1+MIC3 时：L 声道 = MIC1, R 声道 = MIC3
 *    - 选择 MIC1+MIC2 时：L 声道 = MIC1, R 声道 = MIC2
 *    - 如果选 ≥3 个 MIC，必须用 TDM 模式（4 个 slot）
 *
 * ══════════════════════════════════════════════════════════════
 *  MIC 增益配置
 * ══════════════════════════════════════════════════════════════
 *
 *  REG43~REG46 分别控制 MIC1~MIC4 的 PGA（可编程增益放大器）：
 *    bit4 = 增益使能（0=禁用, 1=使能）
 *    bits[3:0] = 增益值索引：
 *      0=0dB, 1=3dB, 2=6dB, ..., 8=24dB, 9=27dB, 10=30dB,
 *      11=33dB, 12=36dB, 13=37.5dB, 14=39dB (max)
 *
 *  本项目设置：MIC1 = 33dB (index=11)
 *
 * ══════════════════════════════════════════════════════════════
 *  寄存器初始化序列来源
 * ══════════════════════════════════════════════════════════════
 *
 *  完全参照 Espressif esp_codec_dev 组件中 es7210.c 的初始化流程：
 *    es7210_open() → es7210_mic_select() → es7210_set_fs() → es7210_start()
 *  通过 Arduino Wire 库直接写 I2C 寄存器。
 */

#include "Es7210Init.h"
#include <Wire.h>
#include <Arduino.h>

// ── 硬件常量 ──────────────────────────────────────────────────
#define ES7210_ADDR  0x40   // ES7210 I2C 7-bit 地址

// ── I2C 读写辅助函数 ─────────────────────────────────────────

// wr7(): 向 ES7210 的指定寄存器写入一个字节
static void wr7(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ES7210_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

// rd7(): 从 ES7210 的指定寄存器读取一个字节
static uint8_t rd7(uint8_t reg) {
    Wire.beginTransmission(ES7210_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false);   // repeated start
    Wire.requestFrom((uint8_t)ES7210_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0x00;
}

// upd7(): 读-改-写（Read-Modify-Write）
// 只修改 mask 指定的位，其余位保持不变
// 等效于: reg = (reg & ~mask) | (data & mask)
static void upd7(uint8_t reg, uint8_t mask, uint8_t data) {
    uint8_t v = rd7(reg);
    wr7(reg, (v & ~mask) | (mask & data));
}

// ═══════════════════════════════════════════════════════════════
//  initEs7210() —— ES7210 完整初始化
// ═══════════════════════════════════════════════════════════════
//
//  初始化分四个阶段：
//    1. es7210_open()       —— 芯片复位、时钟配置、模拟偏置
//    2. es7210_mic_select() —— 选择使用哪些 MIC，配置增益和电源
//    3. es7210_set_fs()     —— 设置位宽和 I2S 格式
//    4. es7210_start()      —— 启动 ADC，重新配置 MIC，设置最终增益
//
bool initEs7210() {

    // ╔═══════════════════════════════════════════════════════════╗
    // ║  阶段 1: es7210_open() —— 芯片复位和基础配置              ║
    // ╚═══════════════════════════════════════════════════════════╝

    // 软复位：写 0xFF 触发全芯片复位，写 0x41 退出复位状态
    wr7(0x00, 0xFF);  // RESET_REG00: 全芯片软复位
    wr7(0x00, 0x41);  // 退出复位，进入正常工作状态

    // 关闭所有时钟（稍后在 es7210_start 阶段重新打开）
    wr7(0x01, 0x3F);  // CLOCK_OFF_REG01: 所有 MIC 和 ADC 时钟关闭

    // 时序控制：芯片内部状态机的转换延时
    wr7(0x09, 0x30);  // TIME_CONTROL0_REG09: 芯片状态转换周期
    wr7(0x0A, 0x30);  // TIME_CONTROL1_REG0A: 上电状态转换周期

    // 高通滤波器（HPF）配置：去除直流偏置
    // 麦克风信号通常有直流分量，HPF 将其滤除，只保留交流音频信号
    wr7(0x23, 0x2A);  // ADC12_HPF2_REG23: ADC1/2 高通滤波器系数
    wr7(0x22, 0x0A);  // ADC12_HPF1_REG22: ADC1/2 高通滤波器系数
    wr7(0x20, 0x0A);  // ADC34_HPF2_REG20: ADC3/4 高通滤波器系数
    wr7(0x21, 0x2A);  // ADC34_HPF1_REG21: ADC3/4 高通滤波器系数

    // 设置从机模式（Slave mode）
    // REG08 bit0: 0=从机（ESP32 提供 BCLK/WS），1=主机
    upd7(0x08, 0x01, 0x00);

    // 模拟电源和偏置配置
    wr7(0x40, 0x43);  // ANALOG_REG40: VMID 参考电压，5kΩ 启动（上电序列需要）
    wr7(0x41, 0x70);  // MIC12_BIAS_REG41: MIC1/2 偏置电压 = 2.87V
    wr7(0x42, 0x70);  // MIC34_BIAS_REG42: MIC3/4 偏置电压 = 2.87V
    wr7(0x07, 0x20);  // OSR_REG07: 过采样率 = 32
    wr7(0x02, 0xC1);  // MAINCLK_REG02: DLL bypass，使用外部 MCLK

    // ╔═══════════════════════════════════════════════════════════╗
    // ║  阶段 2: es7210_mic_select() —— 麦克风选择和电源配置      ║
    // ╚═══════════════════════════════════════════════════════════╝
    //
    //  选择 MIC1（主麦克风）：
    //    MIC1 = 主麦克风，采集人声
    //  非 TDM 模式（REG12=0x00），I2S stereo 输出：
    //    L 声道 = MIC1
    //    R 声道 = MIC3（如果启用）或静音

    // 先禁用所有 MIC 的增益
    for (int i = 0; i < 4; i++) upd7(0x43 + i, 0x10, 0x00);

    // 关闭所有 MIC 电源
    wr7(0x4B, 0xFF);  // MIC12_POWER_REG4B: MIC1/2 关电
    wr7(0x4C, 0xFF);  // MIC34_POWER_REG4C: MIC3/4 关电

    // 使能所选 MIC 的时钟和电源
    upd7(0x01, 0x0F, 0x00);  // CLOCK_OFF_REG01: 使能所有 MIC 时钟
    wr7(0x4B, 0x00);          // MIC12_POWER_REG4B: MIC1/2 上电
    wr7(0x4C, 0x00);          // MIC34_POWER_REG4C: MIC3/4 上电

    // 使能 MIC1 增益（暂设 0dB，最终增益在 start 阶段设置）
    upd7(0x43, 0x10, 0x10);  // MIC1_GAIN_REG43 bit4=1: 增益使能
    upd7(0x43, 0x0F, 0x00);  // MIC1 增益 = 0dB（初始值）

    // 使能 MIC3 增益
    upd7(0x45, 0x10, 0x10);  // MIC3_GAIN_REG45 bit4=1: 增益使能
    upd7(0x45, 0x0F, 0x00);  // MIC3 增益 = 0dB（初始值）

    // 设置非 TDM 模式（2 通道 stereo）
    // REG12 = 0x00: 普通 I2S stereo
    // REG12 = 0x02: TDM 模式（4 通道需要）
    wr7(0x12, 0x00);

    // 保存当前时钟寄存器值（es7210_start 阶段恢复用）
    uint8_t offReg = rd7(0x01);

    // ╔═══════════════════════════════════════════════════════════╗
    // ║  阶段 3: es7210_set_fs() —— I2S 格式和位宽配置            ║
    // ╚═══════════════════════════════════════════════════════════╝

    // 设置 16-bit 位宽
    // REG11 bits[7:5] 编码：
    //   000 = 24-bit（默认）
    //   011 = 16-bit   ← 我们使用 0x60
    //   100 = 32-bit
    wr7(0x11, (rd7(0x11) & 0x1F) | 0x60);

    // 设置 I2S 格式为 Normal (Philips)
    // REG11 bits[1:0]:
    //   00 = I2S Normal  ← 我们使用这个
    //   01 = Left Justified
    //   10 = DSP/PCM-A
    //   11 = DSP/PCM-B
    wr7(0x11, rd7(0x11) & 0xFC);

    // 从机模式下采样率由 ESP32 I2S master 控制，无需配置 ES7210 的时钟分频

    // ╔═══════════════════════════════════════════════════════════╗
    // ║  阶段 4: es7210_start() —— 启动 ADC                      ║
    // ╚═══════════════════════════════════════════════════════════╝

    wr7(0x01, offReg);   // 恢复保存的时钟寄存器
    wr7(0x06, 0x00);     // POWER_DOWN_REG06 = 0: 全芯片上电

    // 模拟前端参考电压
    wr7(0x40, 0x43);

    // MIC1~MIC4 的独立电源控制（0x08 = 正常工作）
    wr7(0x47, 0x08);     // MIC1_POWER_REG47
    wr7(0x48, 0x08);     // MIC2_POWER_REG48
    wr7(0x49, 0x08);     // MIC3_POWER_REG49
    wr7(0x4A, 0x08);     // MIC4_POWER_REG4A

    // ── es7210_start 内部会再次调用 mic_select（重新配置一遍） ──
    // 这是参考驱动的标准流程，确保 start 时 MIC 状态确定
    for (int i = 0; i < 4; i++) upd7(0x43 + i, 0x10, 0x00);
    wr7(0x4B, 0xFF);
    wr7(0x4C, 0xFF);
    upd7(0x01, 0x0F, 0x00);   // 所有 MIC 时钟使能
    wr7(0x4B, 0x00);           // MIC1/2 上电
    wr7(0x4C, 0x00);           // MIC3/4 上电
    upd7(0x43, 0x10, 0x10);   // MIC1 增益使能
    upd7(0x43, 0x0F, 0x00);
    upd7(0x45, 0x10, 0x10);   // MIC3 增益使能
    upd7(0x45, 0x0F, 0x00);
    wr7(0x12, 0x00);           // 非 TDM 模式
    wr7(0x40, 0x43);

    // 启动序列：先写 0x71 触发内部启动逻辑，再写 0x41 进入正常运行
    wr7(0x00, 0x71);
    wr7(0x00, 0x41);

    // ── 设置最终 MIC 增益 ──
    // MIC1: 33dB（index=11），用于采集人声
    // MIC3: 30dB（index=10），备用/AEC 参考
    // 增益表: 0=0dB, 1=3dB, 2=6dB, ..., 8=24dB, 10=30dB, 11=33dB, 12=36dB
    upd7(0x43, 0x0F, 11);    // MIC1: 33dB
    upd7(0x45, 0x0F, 10);    // MIC3: 30dB

    // ── 验证 I2C 通信 ──
    // REG00 在初始化完成后应为 0x41
    uint8_t reg00 = rd7(0x00);
    Serial.printf("[ES7210] REG00 = 0x%02X (expect 0x41)\n", reg00);

    return true;
}

// ═══════════════════════════════════════════════════════════════
//  dumpEs7210Regs() —— 诊断：回读关键寄存器并验证
// ═══════════════════════════════════════════════════════════════
//  用于调试：打印所有关键寄存器的当前值，与期望值对比。
//  如果有 MISMATCH，说明 I2C 写入失败或芯片状态异常。
void dumpEs7210Regs() {
    Serial.println("[ES7210] === Register Dump ===");

    struct { uint8_t reg; const char* name; uint8_t expect; } regs[] = {
        { 0x00, "REG00(reset/mode)",       0x41 },   // 正常运行状态
        { 0x01, "REG01(clk_off)",          0x30 },   // MIC 时钟已使能
        { 0x06, "REG06(power_down)",       0x00 },   // 0 = 已上电
        { 0x07, "REG07(OSR)",              0x20 },   // 过采样率 = 32
        { 0x08, "REG08(mode_cfg)",         0x10 },   // bit0=0 = 从机模式
        { 0x09, "REG09(time_ctrl0)",       0x30 },
        { 0x0A, "REG0A(time_ctrl1)",       0x30 },
        { 0x11, "REG11(SDP_I/F1)",         0x60 },   // 16-bit I2S Normal
        { 0x12, "REG12(SDP_I/F2)",         0x00 },   // 非 TDM
        { 0x40, "REG40(analog)",           0x43 },
        { 0x41, "REG41(MIC12_bias)",       0x70 },   // 2.87V 偏置
        { 0x42, "REG42(MIC34_bias)",       0x70 },
        { 0x43, "REG43(MIC1_gain)",        0x1B },   // enable=1, gain=11(33dB)
        { 0x45, "REG45(MIC3_gain)",        0x1A },   // enable=1, gain=10(30dB)
        { 0x4B, "REG4B(MIC12_power)",      0x00 },   // 已上电
        { 0x4C, "REG4C(MIC34_power)",      0x00 },   // 已上电
    };

    bool allOk = true;
    for (auto& r : regs) {
        uint8_t v = rd7(r.reg);
        bool ok = (v == r.expect);
        Serial.printf("  %s = 0x%02X  (expect 0x%02X) %s\n",
                      r.name, v, r.expect, ok ? "OK" : "<<MISMATCH>>");
        if (!ok) allOk = false;
    }
    Serial.printf("[ES7210] === %s ===\n", allOk ? "ALL OK" : "HAS MISMATCH");
}
