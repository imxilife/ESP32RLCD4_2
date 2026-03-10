#include "WiFiConfig.h"
#include <esp_sntp.h>

// 静态成员初始化
WiFiMessageCallback WiFiConfig::staticMessageCallback = nullptr;
WiFiNTPCallback     WiFiConfig::staticNTPCallback     = nullptr;

// NTP 服务器配置
const char* WiFiConfig::ntpServer1 = "ntp1.aliyun.com";
const char* WiFiConfig::ntpServer2 = "ntp2.aliyun.com";
const char* WiFiConfig::ntpServer3 = "0.cn.pool.ntp.org";
const long WiFiConfig::gmtOffset_sec = 8 * 3600;
const int  WiFiConfig::daylightOffset_sec = 0;

WiFiConfig::WiFiConfig() : wifiConnected(false), messageCallback(nullptr) {
}

void WiFiConfig::setMessageCallback(WiFiMessageCallback cb) {
    messageCallback       = cb;
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

// ── 凭据持久化 ────────────────────────────────────────────────

int WiFiConfig::loadCredentials(WiFiCredential* buf) {
    Preferences prefs;
    prefs.begin("wifi_creds", true);
    int count = prefs.getInt("cnt", 0);
    if (count > kMaxNetworks) count = kMaxNetworks;
    for (int i = 0; i < count; i++) {
        char key[8];
        snprintf(key, sizeof(key), "ssid%d", i);
        String s = prefs.getString(key, "");
        strncpy(buf[i].ssid, s.c_str(), 64);     buf[i].ssid[64]     = '\0';
        snprintf(key, sizeof(key), "pass%d", i);
        s = prefs.getString(key, "");
        strncpy(buf[i].password, s.c_str(), 64); buf[i].password[64] = '\0';
        snprintf(key, sizeof(key), "user%d", i);
        s = prefs.getString(key, "");
        strncpy(buf[i].username, s.c_str(), 64); buf[i].username[64] = '\0';
    }
    prefs.end();
    return count;
}

void WiFiConfig::persistCredentials(const WiFiCredential* buf, int count) {
    Preferences prefs;
    prefs.begin("wifi_creds", false);
    prefs.putInt("cnt", count);
    for (int i = 0; i < count; i++) {
        char key[8];
        snprintf(key, sizeof(key), "ssid%d", i); prefs.putString(key, buf[i].ssid);
        snprintf(key, sizeof(key), "pass%d", i); prefs.putString(key, buf[i].password);
        snprintf(key, sizeof(key), "user%d", i); prefs.putString(key, buf[i].username);
    }
    prefs.end();
}

void WiFiConfig::saveCredential(const char* ssid, const char* password, const char* username) {
    WiFiCredential buf[kMaxNetworks];
    int count = loadCredentials(buf);

    // 若 SSID 已存在，更新密码/用户名并移到末尾（最近使用）
    for (int i = 0; i < count; i++) {
        if (strcmp(buf[i].ssid, ssid) == 0) {
            strncpy(buf[i].password, password ? password : "", 64);
            strncpy(buf[i].username, username ? username : "", 64);
            // 移到末尾
            WiFiCredential updated = buf[i];
            memmove(&buf[i], &buf[i + 1], (count - i - 1) * sizeof(WiFiCredential));
            buf[count - 1] = updated;
            persistCredentials(buf, count);
            Serial.printf("[WiFi] 已更新保存的网络: %s\n", ssid);
            return;
        }
    }

    // 新 SSID：超出上限则淘汰最旧的（index 0）
    if (count >= kMaxNetworks) {
        memmove(&buf[0], &buf[1], (kMaxNetworks - 1) * sizeof(WiFiCredential));
        count = kMaxNetworks - 1;
    }
    strncpy(buf[count].ssid,     ssid,                    64); buf[count].ssid[64]     = '\0';
    strncpy(buf[count].password, password ? password : "", 64); buf[count].password[64] = '\0';
    strncpy(buf[count].username, username ? username : "", 64); buf[count].username[64] = '\0';
    count++;
    persistCredentials(buf, count);
    Serial.printf("[WiFi] 已保存新网络: %s（共 %d 个）\n", ssid, count);
}

// ── 静默尝试连接（无 UI，10 秒超时）────────────────────────────

bool WiFiConfig::tryConnectQuiet(const WiFiCredential& cred) {
    Serial.printf("[WiFi] 尝试自动连接: %s\n", cred.ssid);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);

    if (cred.username[0] != '\0') {
        // WPA2-Enterprise
        esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)cred.username, strlen(cred.username));
        esp_wifi_sta_wpa2_ent_set_username((uint8_t*)cred.username, strlen(cred.username));
        esp_wifi_sta_wpa2_ent_set_password((uint8_t*)cred.password, strlen(cred.password));
        esp_wifi_sta_wpa2_ent_enable();
        WiFi.begin(cred.ssid);
    } else {
        WiFi.begin(cred.ssid, cred.password[0] != '\0' ? cred.password : nullptr);
    }

    const int maxRetries = 20;  // 20 × 500ms = 10s
    for (int i = 0; i < maxRetries; i++) {
        if (WiFi.status() == WL_CONNECTED) return true;
        delay(500);
    }
    WiFi.disconnect(true);
    Serial.printf("[WiFi] 自动连接失败: %s\n", cred.ssid);
    return false;
}

// ── begin()：先试已保存网络，全部失败再启动配网门户 ───────────

bool WiFiConfig::begin() {
    Serial.println("\n=== WiFi 初始化 ===");
    emitMessage("WiFi连接中...", "");

    // 1. 尝试已保存的凭据（逆序：最后保存/最近使用的优先）
    WiFiCredential saved[kMaxNetworks];
    int savedCount = loadCredentials(saved);
    Serial.printf("[WiFi] 已保存网络: %d 个\n", savedCount);

    for (int i = savedCount - 1; i >= 0; i--) {
        if (tryConnectQuiet(saved[i])) {
            wifiConnected = true;
            char ipStr[48];
            snprintf(ipStr, sizeof(ipStr), "IP:%s", WiFi.localIP().toString().c_str());
            Serial.printf("[WiFi] 自动连接成功: %s  %s\n", WiFi.SSID().c_str(), ipStr);
            emitMessage("WiFi连接成功", ipStr);
            // 更新为最近使用（移到末尾）
            saveCredential(saved[i].ssid, saved[i].password, saved[i].username);
            syncNTP();
            return true;
        }
    }

    // 2. 全部失败 → 启动配网门户
    Serial.println("[WiFi] 无可用已保存网络，启动配网门户");

    WiFiManagerParameter param_user("user", "Enterprise Username", "", 64);
    wifiManager.addParameter(&param_user);
    wifiManager.setDebugOutput(true);
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.setConnectTimeout(3);       // PSK 快速超时（企业 WiFi 会失败，预期行为）
    wifiManager.setConfigPortalTimeout(0);  // 门户永不超时，等待用户操作
    wifiManager.setBreakAfterConfig(true);  // 提交表单后立即退出门户
    wifiManager.setSaveConfigCallback([this]() {
        emitMessage("WiFi连接中...", "");
    });

    String apName = "ESP32_RLCD_" + String((uint32_t)ESP.getEfuseMac(), HEX);
    wifiManager.startConfigPortal(apName.c_str());

    // 3. 读取门户提交的凭据
    String ssid = wifiManager.getWiFiSSID(true);
    String pass = wifiManager.getWiFiPass(true);
    String user = String(param_user.getValue());

    Serial.printf("[DEBUG] 门户退出: SSID=%s User=%s\n", ssid.c_str(), user.c_str());

    if (ssid.length() == 0) {
        Serial.println("[WiFi] 未收到 SSID，跳过连接");
        emitMessage("WiFi连接失败", "");
        delay(3000);
        return false;
    }

    // 4. 尝试连接，成功则保存凭据
    bool ok = (user.length() > 0)
        ? beginEnterprise(ssid.c_str(), user.c_str(), user.c_str(), pass.c_str())
        : beginNormal(ssid.c_str(), pass.c_str());

    if (ok) {
        saveCredential(ssid.c_str(), pass.c_str(), user.c_str());
    }
    return ok;
}

// ── NTP 同步 ─────────────────────────────────────────────────

bool WiFiConfig::syncNTP() {
    if (!wifiConnected) {
        Serial.println("无法同步 NTP：WiFi 未连接");
        return false;
    }

    Serial.println("\n=== NTP 时间同步 ===");
    emitMessage("NTP sync...", "");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
    Serial.printf("NTP 服务器: %s, %s, %s\n", ntpServer1, ntpServer2, ntpServer3);

    // 等待 SNTP 实际完成同步（不依赖 getLocalTime 的年份检查，避免返回旧系统时钟）
    int retryCount = 0;
    const int maxRetries = 20;  // 10s
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && retryCount < maxRetries) {
        Serial.print(".");
        delay(500);
        retryCount++;
    }
    Serial.println();

    if (retryCount >= maxRetries) {
        Serial.println("NTP 时间同步失败：超时");
        emitMessage("NTP sync...", "sync fail...");
        return false;
    }

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 1000)) {
        Serial.println("NTP 时间同步失败：getLocalTime 异常");
        emitMessage("NTP sync...", "sync fail...");
        return false;
    }

    Serial.printf("NTP 同步成功: %04d-%02d-%02d %02d:%02d:%02d 星期%d\n",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, timeinfo.tm_wday);
    emitMessage("NTP sync...", "sync ok");

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

// ── 普通 WPA/WPA2 PSK 连接 ────────────────────────────────────

bool WiFiConfig::beginNormal(const char* ssid, const char* password) {
    Serial.printf("\n=== WiFi 普通连接: %s ===\n", ssid);
    emitMessage("WiFi连接中...", "");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, (password && strlen(password) > 0) ? password : nullptr);

    Serial.print("正在连接");
    const int maxRetries = 40;  // 20s
    for (int i = 0; i < maxRetries; i++) {
        if (WiFi.status() == WL_CONNECTED) break;
        Serial.print(".");
        delay(500);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        char ipStr[48];
        snprintf(ipStr, sizeof(ipStr), "IP:%s", WiFi.localIP().toString().c_str());
        Serial.printf("连接成功: %s  %s\n", WiFi.SSID().c_str(), ipStr);
        emitMessage("WiFi连接成功", ipStr);
        syncNTP();
        return true;
    }

    Serial.printf("连接失败, status=%d\n", (int)WiFi.status());
    wifiConnected = false;
    emitMessage("WiFi连接失败", "");
    delay(3000);
    return false;
}

// ── WPA2-Enterprise PEAP 连接 ─────────────────────────────────

bool WiFiConfig::beginEnterprise(const char* ssid, const char* identity,
                                  const char* username, const char* password) {
    Serial.printf("\n=== WiFi 企业认证: %s (identity=%s) ===\n", ssid, identity);
    emitMessage("WiFi连接中...", "");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)identity, strlen(identity));
    esp_wifi_sta_wpa2_ent_set_username((uint8_t*)username, strlen(username));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t*)password, strlen(password));
    esp_wifi_sta_wpa2_ent_enable();
    WiFi.begin(ssid);

    Serial.print("正在连接");
    const int maxRetries = 40;  // 20s
    for (int i = 0; i < maxRetries; i++) {
        if (WiFi.status() == WL_CONNECTED) break;
        Serial.print(".");
        delay(500);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        char ipStr[48];
        snprintf(ipStr, sizeof(ipStr), "IP:%s", WiFi.localIP().toString().c_str());
        Serial.printf("连接成功: %s  %s\n", WiFi.SSID().c_str(), ipStr);
        emitMessage("WiFi连接成功", ipStr);
        syncNTP();
        return true;
    }

    Serial.printf("连接失败, status=%d\n", (int)WiFi.status());
    wifiConnected = false;
    emitMessage("WiFi连接失败", "");
    delay(3000);
    return false;
}

// ── 其他公共方法 ──────────────────────────────────────────────

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
    // 同时清除本地保存的多网络凭据
    Preferences prefs;
    prefs.begin("wifi_creds", false);
    prefs.clear();
    prefs.end();
    Serial.println("WiFi 配置已重置（含已保存网络）");
}

// ── AP 配网门户回调 ───────────────────────────────────────────

void WiFiConfig::configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("进入 WiFi 配置模式");
    Serial.printf("AP: %s  IP: %s\n",
                  WiFi.softAPSSID().c_str(), WiFi.softAPIP().toString().c_str());

    // 显示配网信息 4 秒，然后发空消息切回时钟（门户继续后台阻塞）
    char apURL[48];
    snprintf(apURL, sizeof(apURL), "http://%s", WiFi.softAPIP().toString().c_str());
    emitMessage("未找可用WiFi", apURL);
    delay(4000);
    emitMessage("", "");  // 空消息 → loop() 清除 WiFi UI，切回时钟
}
