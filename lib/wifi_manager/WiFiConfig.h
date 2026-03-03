#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Gui.h>
#include <RTC85063.h>
#include <time.h>

class WiFiConfig {
public:
    // 构造函数，传入 GUI 实例用于屏幕显示
    WiFiConfig(Gui* guiInstance);
    
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
    
    // 同步 NTP 时间到 RTC
    bool syncNTPToRTC(RTC85063* rtcInstance);

private:
    Gui* gui;
    WiFiManager wifiManager;
    bool wifiConnected;
    
    // NTP 服务器配置
    static const char* ntpServer1;
    static const char* ntpServer2;
    static const char* ntpServer3;
    static const long gmtOffset_sec;      // GMT+8 时区偏移（秒）
    static const int daylightOffset_sec;  // 夏令时偏移（秒）
    
    // 回调函数
    static void saveConfigCallback();
    static void configModeCallback(WiFiManager *myWiFiManager);
    
    // 静态 GUI 指针，用于回调函数中访问
    static Gui* staticGui;
};

#endif // WIFI_CONFIG_H
