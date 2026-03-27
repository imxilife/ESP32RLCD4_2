#pragma once

#include <Arduino.h>

class Adafruit_SHTC3;

/**
 * 温湿度传感器封装类
 * 基于 SHTC3 传感器
 */
class Humiture {
public:
    /**
     * 构造函数
     */
    Humiture();

    ~Humiture();

    /**
     * 初始化传感器
     * @param sda SDA 引脚
     * @param scl SCL 引脚
     * @return true 初始化成功，false 失败
     */
    bool begin(int sda = -1, int scl = -1);

    /**
     * 读取温度
     * @return 温度值（摄氏度）
     */
    float readTemperature();

    /**
     * 读取湿度
     * @return 湿度值（百分比）
     */
    float readHumidity();

    /**
     * 同时读取温湿度
     * @param temperature 温度输出参数
     * @param humidity 湿度输出参数
     * @return true 读取成功，false 失败
     */
    bool read(float &temperature, float &humidity);

    /**
     * 检查传感器是否已初始化
     * @return true 已初始化，false 未初始化
     */
    bool isInitialized() const;

private:
    Adafruit_SHTC3* shtc3_;
    bool initialized_;
};
