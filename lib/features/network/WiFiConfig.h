#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <Arduino.h>

class WiFiManager;

// WiFi 配网/时间同步过程中的文本提示回调
typedef void (*WiFiMessageCallback)(const char* line1, const char* line2);

// NTP 同步成功回调：loop() 收到后负责写入 RTC
typedef void (*WiFiNTPCallback)(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second,
                                uint8_t weekday);

// 保存的单条 WiFi 凭据（username 为空 = 普通 WPA/WPA2）
struct WiFiCredential {
    char ssid[65];
    char password[65];
    char username[65];  // 空 = 普通 PSK；非空 = WPA2-Enterprise
};

static const int kMaxNetworks = 5;  // 最多保存的网络数

class WiFiConfig {
public:
    struct Impl;

    WiFiConfig();
    ~WiFiConfig();

    // 设置文本提示回调（配网进度/状态）
    void setMessageCallback(WiFiMessageCallback cb);

    // 设置 NTP 同步成功回调
    void setNTPCallback(WiFiNTPCallback cb);

    // 初始化 WiFi（阻塞）：先尝试已保存凭据，全部失败则启动配网门户
    bool begin();

    // 直接连接：普通 WPA/WPA2 PSK（含 UI 提示 + NTP）
    bool beginNormal(const char* ssid, const char* password);

    // 直接连接：WPA2-Enterprise PEAP（含 UI 提示 + NTP）
    bool beginEnterprise(const char* ssid, const char* identity,
                         const char* username, const char* password);

    // 检查 WiFi 是否已连接
    bool isConnected();

    // 获取 IP 地址
    String getIPAddress();

    // 获取 SSID
    String getSSID();

    // 重置 WiFi 配置（清除 WiFiManager 设置 + 已保存凭据）
    void reset();

    // 单独触发 NTP 同步（需已连接 WiFi）
    bool syncNTP();

private:
    Impl* impl_;
    bool wifiConnected;

    WiFiMessageCallback messageCallback;
    static WiFiMessageCallback staticMessageCallback;
    static WiFiNTPCallback staticNTPCallback;

    static void emitMessage(const char* line1, const char* line2);
    static void configModeCallback(WiFiManager *myWiFiManager);

    // ── 多网络凭据持久化（Preferences NVS）────────────────────────
    // 返回实际加载数量（≤ kMaxNetworks）
    int  loadCredentials(WiFiCredential* buf);
    // 新增或更新一条凭据（按 SSID 去重；超出上限时淘汰最旧的）
    void saveCredential(const char* ssid, const char* password, const char* username);
    // 内部：将 buf[0..count-1] 写入 Preferences
    void persistCredentials(const WiFiCredential* buf, int count);

    // 静默尝试连接（无 UI 消息，10 秒超时）：成功返回 true
    bool tryConnectQuiet(const WiFiCredential& cred);

    static const char* ntpServer1;
    static const char* ntpServer2;
    static const char* ntpServer3;
    static const long gmtOffset_sec;
    static const int daylightOffset_sec;
};

#endif // WIFI_CONFIG_H
