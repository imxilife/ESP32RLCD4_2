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
    Serial.println("\n=== WiFi 初始化 (WPA2-Enterprise PEAP) ===");
    emitMessage("WiFi连接中...", "");

    // 企业 WiFi 额外字段：用户名（门户表单 Password 字段复用为企业密码）
    WiFiManagerParameter param_user("user", "Enterprise Username", "", 64);
    wifiManager.addParameter(&param_user);

    wifiManager.setDebugOutput(true);
    wifiManager.setAPCallback(configModeCallback);

    // 表单提交后，PSK 连接尝试快速超时（企业 WiFi 必然失败）
    wifiManager.setConnectTimeout(3);
    wifiManager.setConfigPortalTimeout(0);  // 门户不超时，等用户操作

    // 表单提交后强制退出门户（不管 PSK 连接是否成功）
    wifiManager.setBreakAfterConfig(true);

    // 回调仅用于 UI 提示，不在此读凭据（此时 WiFi.SSID() 还是空）
    wifiManager.setSaveConfigCallback([this]() {
        emitMessage("WiFi连接中...", "");
    });

    String apName = "ESP32_RLCD_" + String((uint32_t)ESP.getEfuseMac(), HEX);

    // 启动配网门户：阻塞直到用户提交表单 + PSK 连接尝试结束后返回
    wifiManager.startConfigPortal(apName.c_str());

    // 门户退出后读凭据：
    //   getWiFiSSID(true) / getWiFiPass(true) 从 flash 读，WiFiManager 已在发起连接前写入
    //   param_user.getValue() 从自定义字段读
    String ssid = wifiManager.getWiFiSSID(true);
    String pass = wifiManager.getWiFiPass(true);
    String user = String(param_user.getValue());

    Serial.printf("[DEBUG] 门户退出: SSID=%s User=%s\n", ssid.c_str(), user.c_str());

    if (ssid.length() > 0 && user.length() > 0) {
        return beginEnterprise(ssid.c_str(), user.c_str(), user.c_str(), pass.c_str());
    }

    Serial.println("未收到有效凭据（SSID 或 Username 为空），WiFi 连接跳过");
    emitMessage("WiFi连接失败", "");
    return false;
}

bool WiFiConfig::syncNTP() {
    if (!wifiConnected) {
        Serial.println("无法同步 NTP：WiFi 未连接");
        return false;
    }

    Serial.println("\n=== NTP 时间同步 ===");

    // 提示：NTP 时间同步中
    emitMessage("NTP sync...", "");

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

        // 提示：NTP 同步失败（分组显示：line1 保留 "NTP sync..." 标题）
        emitMessage("NTP sync...", "sync fail...");

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

    // 先提示：NTP 同步成功（分组显示：line1="NTP sync..." line2="sync ok"）
    // 必须先入队，后续 NTP 回调投递 MSG_NTP_SYNC，loop() 处理时才会清屏画时钟
    emitMessage("NTP sync...", "sync ok");

    // 通过回调将时间数据交给 loop()，由 loop() 负责写入 RTC 并立即刷新时钟
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

    return true;
}

bool WiFiConfig::beginEnterprise(const char* ssid, const char* identity,
                                  const char* username, const char* password) {
    Serial.println("\n=== WiFi 企业认证初始化 (WPA2-Enterprise PEAP) ===");
    Serial.printf("SSID    : %s\n", ssid);
    Serial.printf("Identity: %s\n", identity);
    Serial.printf("Username: %s\n", username);

    emitMessage("WiFi连接中...", "");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);

    esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)identity, strlen(identity));
    esp_wifi_sta_wpa2_ent_set_username((uint8_t*)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t*)password, strlen(password));
    esp_wifi_sta_wpa2_ent_enable();

    WiFi.begin(ssid);

    Serial.print("正在连接");
    int retryCount = 0;
    const int maxRetries = 40;  // 40 × 500ms = 20s
    while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
        Serial.print(".");
        delay(500);
        retryCount++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi 企业认证连接成功！");
        Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("IP  : %s\n", WiFi.localIP().toString().c_str());
        wifiConnected = true;

        char ipStr[48];
        snprintf(ipStr, sizeof(ipStr), "IP:%s", WiFi.localIP().toString().c_str());
        emitMessage("WiFi连接成功", ipStr);

        syncNTP();
        return true;
    } else {
        Serial.printf("WiFi 企业认证连接失败，WiFi.status()=%d\n", (int)WiFi.status());
        wifiConnected = false;
        emitMessage("WiFi连接失败", "");
        return false;
    }
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

// WiFi 配置门户启动回调（进入配网模式时调用）
void WiFiConfig::configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("进入 WiFi 配置模式");
    Serial.print("配置门户 AP 名称: ");
    Serial.println(WiFi.softAPSSID());
    Serial.print("配置门户 IP: ");
    Serial.println(WiFi.softAPIP());

    // 提示：未找到可用 WiFi，显示配网 URL
    char apURL[48];
    snprintf(apURL, sizeof(apURL), "http://%s", WiFi.softAPIP().toString().c_str());
    emitMessage("未找可用WiFi", apURL);
}
