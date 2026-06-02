#include "WeatherService.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <core/app_message/app_message.h>
#include <features/network/NetworkService.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <cstring>

extern QueueHandle_t g_msgQueue;

#ifndef QWEATHER_API_HOST
#define QWEATHER_API_HOST ""
#endif

#ifndef QWEATHER_API_KEY
#define QWEATHER_API_KEY ""
#endif

#ifndef QWEATHER_LOCATION
#define QWEATHER_LOCATION ""
#endif

namespace {
constexpr uint32_t kWeatherRefreshMs = 20UL * 60UL * 1000UL;
constexpr uint32_t kNetworkRetryMs = 5000;
constexpr uint16_t kFallbackIconCode = 999;

bool g_started = false;
WeatherStatusMsg g_snapshot = {};

bool isConfigured() {
    return strlen(QWEATHER_API_HOST) > 0 &&
           strlen(QWEATHER_API_KEY) > 0 &&
           strlen(QWEATHER_LOCATION) > 0;
}

bool extractJsonString(const String& json, const char* key, char* out, size_t outSize) {
    if (key == nullptr || out == nullptr || outSize == 0) return false;
    out[0] = '\0';

    const String needle = String("\"") + key + "\":";
    int pos = json.indexOf(needle);
    if (pos < 0) return false;
    pos += needle.length();

    while (pos < (int)json.length() && json[pos] == ' ') ++pos;
    if (pos >= (int)json.length() || json[pos] != '"') return false;
    ++pos;

    size_t written = 0;
    bool escaped = false;
    for (; pos < (int)json.length(); ++pos) {
        char ch = json[pos];
        if (escaped) {
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
            continue;
        } else if (ch == '"') {
            break;
        }

        if (written + 1 < outSize) {
            out[written++] = ch;
        }
    }
    out[written] = '\0';
    return written > 0;
}

void uppercaseAscii(char* text) {
    if (text == nullptr) return;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (text[i] >= 'a' && text[i] <= 'z') {
            text[i] = static_cast<char>(text[i] - 'a' + 'A');
        }
    }
}

bool parseWeatherPayload(const String& payload, WeatherStatusMsg& out) {
    char tempBuf[8] = {};
    char iconBuf[8] = {};
    char textBuf[WEATHER_TEXT_MAX] = {};

    if (!extractJsonString(payload, "temp", tempBuf, sizeof(tempBuf))) return false;
    if (!extractJsonString(payload, "icon", iconBuf, sizeof(iconBuf))) return false;
    if (!extractJsonString(payload, "text", textBuf, sizeof(textBuf))) return false;

    out.valid = true;
    out.temperatureC = static_cast<int16_t>(atoi(tempBuf));
    out.iconCode = static_cast<uint16_t>(atoi(iconBuf));
    if (out.iconCode == 0) out.iconCode = kFallbackIconCode;
    out.updatedAtMs = millis();
    strncpy(out.text, textBuf, WEATHER_TEXT_MAX - 1);
    out.text[WEATHER_TEXT_MAX - 1] = '\0';
    uppercaseAscii(out.text);
    return true;
}

void publishWeather(const WeatherStatusMsg& weather) {
    if (g_msgQueue == nullptr) return;
    AppMessage msg = {};
    msg.type = MSG_WEATHER_UPDATE;
    msg.weather = weather;
    xQueueSend(g_msgQueue, &msg, 0);
}

bool fetchWeatherNow() {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String("https://") + QWEATHER_API_HOST +
                 "/v7/weather/now?location=" + QWEATHER_LOCATION +
                 "&lang=en&unit=m";

    if (!http.begin(client, url)) {
        Serial.println("[Weather] http begin failed");
        return false;
    }

    http.addHeader("X-QW-Api-Key", QWEATHER_API_KEY);
    http.addHeader("Accept-Encoding", "identity");
    http.setTimeout(8000);

    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        Serial.printf("[Weather] request failed: %d\n", status);
        http.end();
        return false;
    }

    const String body = http.getString();
    http.end();

    WeatherStatusMsg next = {};
    if (!parseWeatherPayload(body, next)) {
        Serial.println("[Weather] parse failed");
        return false;
    }

    g_snapshot = next;
    publishWeather(g_snapshot);
    return true;
}

void weatherTask(void*) {
    if (!isConfigured()) {
        Serial.println("[Weather] QWeather config missing; skip weather task");
        vTaskDelete(nullptr);
        return;
    }

    while (true) {
        if (!NetworkService::isConnected()) {
            vTaskDelay(pdMS_TO_TICKS(kNetworkRetryMs));
            continue;
        }

        fetchWeatherNow();
        vTaskDelay(pdMS_TO_TICKS(kWeatherRefreshMs));
    }
}
}

namespace WeatherService {
void begin() {
    if (g_started) return;
    g_started = true;
    xTaskCreate(weatherTask, "weatherTask", 8192, nullptr, 1, nullptr);
}

WeatherStatusMsg snapshot() {
    return g_snapshot;
}
}
