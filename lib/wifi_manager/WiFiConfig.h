#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <RTC85063.h>
#include <time.h>

// WiFi 配网/时间同步过程中的文本提示回调。
// line1/line2 为要显示的两行文本字符串，可以为 nullptr 或空串表示不显示。
typedef void (*WiFiMessageCallback)(const char* line1, const char* line2);

class WiFiConfig {
public:
    // 构造函数，不再持有 GUI，显示由外部通过回调完成
    WiFiConfig();

    // 设置文本提示回调，用于将进度/状态消息发送到上层（例如经由消息队列更新 GUI）
    void setMessageCallback(WiFiMessageCallback cb);
    
    // 初始化 WiFi，返回是否连接成功
    bool begin();
    
    // 初始化 WiFi 并同步 NTP 时间到 RTC
    bool begin(RTC85063* rtcInstance);
    
    // 检查 WiFi 是否已连接
    bool isConnected();
    
    // 获取 IP 地址
    String getIPAddress();
    
    // 获取 SSID
    String getSSID();
    
    // 重置 WiFi 配置（清除保存的凭据）
    void reset();
    
    // 同步 NTP 时间到 RTC（仍为阻塞调用）
    bool syncNTPToRTC(RTC85063* rtcInstance);

private:
    WiFiManager wifiManager;
    bool wifiConnected;

    // 文本提示回调（成员与静态各一份，静态用于 WiFiManager 的静态回调函数中）
    WiFiMessageCallback messageCallback;
    static WiFiMessageCallback staticMessageCallback;

    // 统一触发文本提示回调的辅助函数
    static void emitMessage(const char* line1, const char* line2);

    // NTP 服务器配置
    static const char* ntpServer1;
    static const char* ntpServer2;
    static const char* ntpServer3;
    static const long gmtOffset_sec;      // GMT+8 时区偏移（秒）
    static const int daylightOffset_sec;  // 夏令时偏移（秒）

    // WiFiManager 回调函数
    static void saveConfigCallback();
    static void configModeCallback(WiFiManager *myWiFiManager);
};

#endif // WIFI_CONFIG_H
