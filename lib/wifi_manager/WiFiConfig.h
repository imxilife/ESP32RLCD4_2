#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_wpa2.h>
#include <time.h>

// WiFi 配网/时间同步过程中的文本提示回调
typedef void (*WiFiMessageCallback)(const char* line1, const char* line2);

// NTP 同步成功回调：loop() 收到后负责写入 RTC
typedef void (*WiFiNTPCallback)(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second,
                                uint8_t weekday);

class WiFiConfig {
public:
    WiFiConfig();

    // 设置文本提示回调（配网进度/状态）
    void setMessageCallback(WiFiMessageCallback cb);

    // 设置 NTP 同步成功回调
    void setNTPCallback(WiFiNTPCallback cb);

    // 初始化 WiFi（阻塞，含配网和 NTP 同步）—— 普通 PSK 热点配网流程
    bool begin();

    // 初始化 WiFi（阻塞）—— WPA2-Enterprise PEAP，跳过 CA 证书验证
    bool beginEnterprise(const char* ssid, const char* identity,
                         const char* username, const char* password);

    // 检查 WiFi 是否已连接
    bool isConnected();

    // 获取 IP 地址
    String getIPAddress();

    // 获取 SSID
    String getSSID();

    // 重置 WiFi 配置（清除保存的凭据）
    void reset();

    // 单独触发 NTP 同步（需已连接 WiFi）
    bool syncNTP();

private:
    WiFiManager wifiManager;
    bool wifiConnected;

    WiFiMessageCallback messageCallback;
    static WiFiMessageCallback staticMessageCallback;

    static WiFiNTPCallback staticNTPCallback;

    static void emitMessage(const char* line1, const char* line2);

    static const char* ntpServer1;
    static const char* ntpServer2;
    static const char* ntpServer3;
    static const long gmtOffset_sec;
    static const int daylightOffset_sec;

    static void configModeCallback(WiFiManager *myWiFiManager);
};

#endif // WIFI_CONFIG_H
