#include "WiFiConfig.h"

// 静态成员初始化
Gui* WiFiConfig::staticGui = nullptr;

// NTP 服务器配置
const char* WiFiConfig::ntpServer1 = "ntp1.aliyun.com";
const char* WiFiConfig::ntpServer2 = "ntp2.aliyun.com";
const char* WiFiConfig::ntpServer3 = "0.cn.pool.ntp.org";
const long WiFiConfig::gmtOffset_sec = 8 * 3600;  // GMT+8 (中国标准时间)
const int WiFiConfig::daylightOffset_sec = 0;     // 不使用夏令时

WiFiConfig::WiFiConfig(Gui* guiInstance) : gui(guiInstance), wifiConnected(false) {
    staticGui = guiInstance;
}

bool WiFiConfig::begin() {
    return begin(nullptr);  // 不同步 RTC
}

bool WiFiConfig::begin(RTC85063* rtcInstance) {
    Serial.println("\n=== WiFi 初始化 ===");
    
    // 显示初始化信息
    gui->fillRect(0, 100, 400, 40, ColorWhite);
    gui->drawUTF8String(10, 110, "WiFi初始化中...", ColorBlack);
    gui->display();
    
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
        
        // 在屏幕上显示连接成功信息
        gui->fillRect(0, 100, 400, 60, ColorWhite);
        gui->drawUTF8String(10, 110, "WiFi已连接", ColorBlack);
        
        char ipStr[32];
        snprintf(ipStr, sizeof(ipStr), "IP:%s", WiFi.localIP().toString().c_str());
        gui->drawSmallDigits(10, 140, ipStr, ColorBlack);
        gui->display();
        
        delay(2000);
        gui->fillRect(0, 100, 400, 60, ColorWhite);
        gui->display();
        
        // 如果提供了 RTC 实例，进行 NTP 时间同步
        if (rtcInstance != nullptr) {
            syncNTPToRTC(rtcInstance);
        }
        
        return true;
    } else {
        Serial.println("WiFi 连接失败");
        wifiConnected = false;
        
        // 显示失败信息
        gui->fillRect(0, 100, 400, 40, ColorWhite);
        gui->drawUTF8String(10, 110, "WiFi连接失败", ColorBlack);
        gui->display();
        
        return false;
    }
}

bool WiFiConfig::syncNTPToRTC(RTC85063* rtcInstance) {
    if (!wifiConnected || rtcInstance == nullptr) {
        Serial.println("无法同步 NTP：WiFi 未连接或 RTC 实例为空");
        return false;
    }
    
    Serial.println("\n=== NTP 时间同步 ===");
    
    // 显示同步信息
    gui->fillRect(0, 100, 400, 40, ColorWhite);
    gui->drawUTF8String(10, 110, "NTP时间同步中...", ColorBlack);
    gui->display();
    
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
        
        // 显示失败信息
        gui->fillRect(0, 100, 400, 40, ColorWhite);
        gui->drawUTF8String(10, 110, "NTP同步失败", ColorBlack);
        gui->display();
        
        delay(2000);
        gui->fillRect(0, 100, 400, 40, ColorWhite);
        gui->display();
        
        return false;
    }
    
    // NTP 时间同步成功，写入 RTC
    Serial.println("NTP 时间同步成功！");
    Serial.printf("当前时间: %04d-%02d-%02d %02d:%02d:%02d 星期%d\n",
                  timeinfo.tm_year + 1900,
                  timeinfo.tm_mon + 1,
                  timeinfo.tm_mday,
                  timeinfo.tm_hour,
                  timeinfo.tm_min,
                  timeinfo.tm_sec,
                  timeinfo.tm_wday);
    
    // 将时间写入 RTC
    rtcInstance->setTime(
        timeinfo.tm_year + 1900,  // 年
        timeinfo.tm_mon + 1,       // 月 (tm_mon 是 0-11)
        timeinfo.tm_mday,          // 日
        timeinfo.tm_hour,          // 时
        timeinfo.tm_min,           // 分
        timeinfo.tm_sec,           // 秒
        timeinfo.tm_wday           // 星期 (0=周日, 1=周一, ...)
    );
    
    Serial.println("时间已写入 RTC");
    
    // 显示同步成功信息
    gui->fillRect(0, 100, 400, 60, ColorWhite);
    gui->drawUTF8String(10, 110, "NTP同步成功", ColorBlack);
    
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    gui->drawSmallDigits(10, 140, timeStr, ColorBlack);
    gui->display();
    
    delay(2000);
    gui->fillRect(0, 100, 400, 60, ColorWhite);
    gui->display();
    
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
    
    if (staticGui) {
        // 在屏幕上显示保存成功信息
        staticGui->fillRect(0, 100, 400, 60, ColorWhite);
        staticGui->drawUTF8String(10, 110, "配置已保存", ColorBlack);
        staticGui->drawUTF8String(10, 140, "正在连接...", ColorBlack);
        staticGui->display();
    }
}

// WiFi 配置门户启动回调（进入配网模式时调用）
void WiFiConfig::configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("进入 WiFi 配置模式");
    Serial.print("配置门户 AP 名称: ");
    Serial.println(WiFi.softAPSSID());
    Serial.print("配置门户 IP: ");
    Serial.println(WiFi.softAPIP());
    
    if (staticGui) {
        // 在屏幕上显示配置信息
        staticGui->fillRect(0, 100, 400, 100, ColorWhite);
        staticGui->drawUTF8String(10, 110, "WiFi配置模式", ColorBlack);
        
        char apInfo[64];
        snprintf(apInfo, sizeof(apInfo), "AP:%s", WiFi.softAPSSID().c_str());
        staticGui->drawSmallDigits(10, 140, apInfo, ColorBlack);
        
        staticGui->drawSmallDigits(10, 170, "IP:192.168.4.1", ColorBlack);
        
        staticGui->display();
    }
}
