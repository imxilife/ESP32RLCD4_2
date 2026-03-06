#include "WiFiConfig.h"

// 静态成员初始化
WiFiMessageCallback WiFiConfig::staticMessageCallback = nullptr;
WiFiNTPCallback     WiFiConfig::staticNTPCallback     = nullptr;

// NTP 服务器配置
const char* WiFiConfig::ntpServer1 = "ntp1.aliyun.com";
const char* WiFiConfig::ntpServer2 = "ntp2.aliyun.com";
const char* WiFiConfig::ntpServer3 = "0.cn.pool.ntp.org";
const long WiFiConfig::gmtOffset_sec = 8 * 3600;  // GMT+8 (中国标准时间)
const int WiFiConfig::daylightOffset_sec = 0;     // 不使用夏令时

WiFiConfig::WiFiConfig() : wifiConnected(false), messageCallback(nullptr) {
}

void WiFiConfig::setMessageCallback(WiFiMessageCallback cb) {
    messageCallback      = cb;
    staticMessageCallback = cb;
}

void WiFiConfig::setNTPCallback(WiFiNTPCallback cb) {
    staticNTPCallback = cb;
}

void WiFiConfig::emitMessage(const char* line1, const char* line2) {
    if (staticMessageCallback) {
        staticMessageCallback(line1, line2);
    }
}

bool WiFiConfig::begin() {
    Serial.println("\n=== WiFi 初始化 ===");

    // 提示：WiFi 初始化中
    emitMessage("WiFi初始化中...", "");

    // 设置回调函数
    wifiManager.setSaveConfigCallback(saveConfigCallback);
    wifiManager.setAPCallback(configModeCallback);

    // 设置连接超时时间（秒）
    wifiManager.setConnectTimeout(10);  // 每次连接尝试 10 秒超时

    // 设置配置门户超时时间（秒），0 表示不超时
    wifiManager.setConfigPortalTimeout(0);  // 配网模式不超时，等待用户配置

    // 设置 AP 的 SSID（使用设备 MAC 地址确保唯一性）
    String apName = "ESP32_RLCD_" + String((uint32_t)ESP.getEfuseMac(), HEX);

    // autoConnect 会自动完成以下流程：
    // 1. 检查是否有保存的 WiFi 配置
    // 2. 如果有，尝试连接（默认重试多次）
    // 3. 如果没有或连接失败，自动创建热点并启动配置门户
    // 4. 等待用户配置完成后自动连接新的 WiFi
    if (wifiManager.autoConnect(apName.c_str())) {
        Serial.println("WiFi 连接成功！");
        Serial.print("SSID: ");
        Serial.println(WiFi.SSID());
        Serial.print("IP 地址: ");
        Serial.println(WiFi.localIP());
        wifiConnected = true;

        // 提示：WiFi 已连接 + IP 信息
        char ipStr[48];
        snprintf(ipStr, sizeof(ipStr), "IP:%s", WiFi.localIP().toString().c_str());
        emitMessage("WiFi已连接", ipStr);

        // WiFi 连接成功后触发 NTP 同步，结果通过回调通知 loop() 写入 RTC
        syncNTP();

        return true;
    } else {
        Serial.println("WiFi 连接失败");
        wifiConnected = false;

        // 提示：WiFi 连接失败
        emitMessage("WiFi连接失败", "");

        return false;
    }
}

bool WiFiConfig::syncNTP() {
    if (!wifiConnected) {
        Serial.println("无法同步 NTP：WiFi 未连接");
        return false;
    }

    Serial.println("\n=== NTP 时间同步 ===");

    // 提示：NTP 时间同步中
    emitMessage("NTP时间同步中...", "");

    // 配置 NTP 服务器
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);

    Serial.print("正在连接 NTP 服务器: ");
    Serial.print(ntpServer1);
    Serial.print(", ");
    Serial.print(ntpServer2);
    Serial.print(", ");
    Serial.println(ntpServer3);

    // 等待时间同步（最多等待 10 秒）
    struct tm timeinfo;
    int retryCount = 0;
    const int maxRetries = 20;  // 20 * 500ms = 10秒

    while (!getLocalTime(&timeinfo) && retryCount < maxRetries) {
        Serial.print(".");
        delay(500);
        retryCount++;
    }
    Serial.println();

    if (retryCount >= maxRetries) {
        Serial.println("NTP 时间同步失败：超时");

        // 提示：NTP 同步失败
        emitMessage("NTP同步失败", "");

        return false;
    }

    // NTP 时间同步成功
    Serial.println("NTP 时间同步成功！");
    Serial.printf("当前时间: %04d-%02d-%02d %02d:%02d:%02d 星期%d\n",
                  timeinfo.tm_year + 1900,
                  timeinfo.tm_mon + 1,
                  timeinfo.tm_mday,
                  timeinfo.tm_hour,
                  timeinfo.tm_min,
                  timeinfo.tm_sec,
                  timeinfo.tm_wday);

    // 通过回调将时间数据交给 loop()，由 loop() 负责写入 RTC
    if (staticNTPCallback) {
        staticNTPCallback(
            (uint16_t)(timeinfo.tm_year + 1900),
            (uint8_t)(timeinfo.tm_mon + 1),
            (uint8_t)(timeinfo.tm_mday),
            (uint8_t)(timeinfo.tm_hour),
            (uint8_t)(timeinfo.tm_min),
            (uint8_t)(timeinfo.tm_sec),
            (uint8_t)(timeinfo.tm_wday)
        );
    }

    // 提示：NTP 同步成功 + 当前时间
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    emitMessage("NTP同步成功", timeStr);

    return true;
}

bool WiFiConfig::isConnected() {
    return wifiConnected && (WiFi.status() == WL_CONNECTED);
}

String WiFiConfig::getIPAddress() {
    return WiFi.localIP().toString();
}

String WiFiConfig::getSSID() {
    return WiFi.SSID();
}

void WiFiConfig::reset() {
    wifiManager.resetSettings();
    Serial.println("WiFi 配置已重置");
}

// WiFi 配置保存回调
void WiFiConfig::saveConfigCallback() {
    Serial.println("WiFi 配置已保存");

    // 提示：配置已保存，正在连接
    emitMessage("配置已保存", "正在连接...");
}

// WiFi 配置门户启动回调（进入配网模式时调用）
void WiFiConfig::configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("进入 WiFi 配置模式");
    Serial.print("配置门户 AP 名称: ");
    Serial.println(WiFi.softAPSSID());
    Serial.print("配置门户 IP: ");
    Serial.println(WiFi.softAPIP());

    // 提示：进入 WiFi 配置模式 + AP 信息
    char apInfo[64];
    snprintf(apInfo, sizeof(apInfo), "AP:%s", WiFi.softAPSSID().c_str());
    emitMessage("WiFi配置模式", apInfo);
}
