#include "WeatherService.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <core/app_message/app_message.h>
#include <features/network/NetworkService.h>
#include <esp32/rom/miniz.h>
#include <esp_heap_caps.h>
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
constexpr uint32_t kWeatherTaskStackBytes = 12 * 1024;
constexpr uint16_t kFallbackIconCode = 999;
constexpr size_t kPayloadPreviewMax = 160;
constexpr size_t kGzipOutputMax = 8 * 1024;
const char* kWeatherHeaderKeys[] = {
    "Content-Encoding",
    "Content-Type",
    "Transfer-Encoding",
};
const size_t kWeatherHeaderCount = sizeof(kWeatherHeaderKeys) / sizeof(kWeatherHeaderKeys[0]);

bool g_started = false;
WeatherStatusMsg g_snapshot = {};

struct WeatherRequest {
    String host;
    String apiKey;
    String location;
    String url;
};

String compactSecretToken(const char* raw) {
    String out;
    if (raw == nullptr) return out;

    const size_t len = strlen(raw);
    out.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        const char ch = raw[i];
        if (ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t') {
            continue;
        }
        out += ch;
    }
    return out;
}

String normalizeApiHost(const char* raw) {
    String host = compactSecretToken(raw);
    if (host.startsWith("https://")) {
        host.remove(0, 8);
    } else if (host.startsWith("http://")) {
        host.remove(0, 7);
    }

    const int slash = host.indexOf('/');
    if (slash >= 0) {
        host = host.substring(0, slash);
    }
    return host;
}

// Build the request from compile-time secrets and remove hidden whitespace before HTTP setup.
bool buildWeatherRequest(WeatherRequest& request) {
    request.host = normalizeApiHost(QWEATHER_API_HOST);
    request.apiKey = compactSecretToken(QWEATHER_API_KEY);
    request.location = compactSecretToken(QWEATHER_LOCATION);

    if (request.host.length() == 0 ||
        request.apiKey.length() == 0 ||
        request.location.length() == 0) {
        request.url = "";
        return false;
    }

    request.url = String("https://") + request.host +
                  "/v7/weather/now?location=" + request.location +
                  "&lang=zh&unit=m";
    return true;
}

bool isConfigured() {
    WeatherRequest request;
    return buildWeatherRequest(request);
}

void logConfigSummary() {
    WeatherRequest request;
    const bool ok = buildWeatherRequest(request);
    Serial.printf("[Weather] config ok=%u host=%s keyLen=%u location=%s rawHostLen=%u rawKeyLen=%u rawLocationLen=%u\n",
                  static_cast<unsigned int>(ok),
                  request.host.length() > 0 ? request.host.c_str() : "<empty>",
                  static_cast<unsigned int>(request.apiKey.length()),
                  request.location.length() > 0 ? request.location.c_str() : "<empty>",
                  static_cast<unsigned int>(strlen(QWEATHER_API_HOST)),
                  static_cast<unsigned int>(strlen(QWEATHER_API_KEY)),
                  static_cast<unsigned int>(strlen(QWEATHER_LOCATION)));
}

bool isGzipPayload(const String& payload) {
    return payload.length() >= 2 &&
           static_cast<uint8_t>(payload[0]) == 0x1F &&
           static_cast<uint8_t>(payload[1]) == 0x8B;
}

uint16_t readLe16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t readLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

bool skipGzipZeroTerminatedField(const uint8_t* data, size_t len, size_t& pos) {
    while (pos < len) {
        if (data[pos++] == 0) return true;
    }
    return false;
}

void* weatherMalloc(size_t size) {
    void* p = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (p != nullptr) return p;
    return heap_caps_malloc(size, MALLOC_CAP_8BIT);
}

bool inflateRawDeflateToBuffer(const uint8_t* src, size_t srcLen, char* dst,
                               size_t dstCap, size_t& outLen) {
    outLen = 0;
    if (src == nullptr || srcLen == 0 || dst == nullptr || dstCap == 0) {
        return false;
    }

    tinfl_decompressor* infl = static_cast<tinfl_decompressor*>(
        weatherMalloc(sizeof(tinfl_decompressor)));
    if (infl == nullptr) {
        Serial.printf("[Weather] gzip inflater malloc failed size=%u\n",
                      static_cast<unsigned int>(sizeof(tinfl_decompressor)));
        return false;
    }

    // tinfl_decompressor is large enough to overflow an 8KB task stack, so it must stay on heap.
    tinfl_init(infl);
    size_t inUsed = srcLen;
    size_t written = dstCap;
    const tinfl_status status = tinfl_decompress(
        infl,
        src,
        &inUsed,
        reinterpret_cast<mz_uint8*>(dst),
        reinterpret_cast<mz_uint8*>(dst),
        &written,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    heap_caps_free(infl);

    outLen = written;
    Serial.printf("[Weather] gzip inflate status=%d inUsed=%u/%u out=%u/%u\n",
                  static_cast<int>(status),
                  static_cast<unsigned int>(inUsed),
                  static_cast<unsigned int>(srcLen),
                  static_cast<unsigned int>(written),
                  static_cast<unsigned int>(dstCap));
    return status == TINFL_STATUS_DONE && inUsed == srcLen;
}

// Gzip wraps a raw deflate stream. Parse the gzip container, then use ESP32 ROM miniz.
bool decompressGzipPayload(const String& payload, String& out) {
    out = "";
    const size_t len = payload.length();
    const uint8_t* data = reinterpret_cast<const uint8_t*>(payload.c_str());

    if (len < 18 || data[0] != 0x1F || data[1] != 0x8B || data[2] != 8) {
        Serial.printf("[Weather] gzip invalid header len=%u\n",
                      static_cast<unsigned int>(len));
        return false;
    }

    const uint8_t flags = data[3];
    if ((flags & 0xE0) != 0) {
        Serial.printf("[Weather] gzip unsupported reserved flags=0x%02X\n",
                      static_cast<unsigned int>(flags));
        return false;
    }

    size_t pos = 10;
    if ((flags & 0x04) != 0) {
        if (pos + 2 > len) return false;
        const uint16_t extraLen = readLe16(data + pos);
        pos += 2;
        if (pos + extraLen > len) return false;
        pos += extraLen;
    }
    if ((flags & 0x08) != 0 && !skipGzipZeroTerminatedField(data, len, pos)) {
        return false;
    }
    if ((flags & 0x10) != 0 && !skipGzipZeroTerminatedField(data, len, pos)) {
        return false;
    }
    if ((flags & 0x02) != 0) {
        if (pos + 2 > len) return false;
        pos += 2;
    }

    if (pos + 8 > len) {
        Serial.printf("[Weather] gzip header exceeds payload headerLen=%u len=%u\n",
                      static_cast<unsigned int>(pos),
                      static_cast<unsigned int>(len));
        return false;
    }

    const size_t deflateLen = len - pos - 8;
    const uint32_t expectedCrc = readLe32(data + len - 8);
    const uint32_t expectedSize = readLe32(data + len - 4);
    if (expectedSize == 0 || expectedSize > kGzipOutputMax) {
        Serial.printf("[Weather] gzip output size unsupported isize=%lu max=%u deflateLen=%u\n",
                      static_cast<unsigned long>(expectedSize),
                      static_cast<unsigned int>(kGzipOutputMax),
                      static_cast<unsigned int>(deflateLen));
        return false;
    }

    char* decoded = static_cast<char*>(weatherMalloc(expectedSize + 1));
    if (decoded == nullptr) {
        Serial.printf("[Weather] gzip malloc failed size=%lu\n",
                      static_cast<unsigned long>(expectedSize + 1));
        return false;
    }

    size_t actualSize = 0;
    if (!inflateRawDeflateToBuffer(data + pos, deflateLen, decoded, expectedSize, actualSize)) {
        Serial.printf("[Weather] gzip inflate failed deflateLen=%u outCap=%lu\n",
                      static_cast<unsigned int>(deflateLen),
                      static_cast<unsigned long>(expectedSize));
        heap_caps_free(decoded);
        return false;
    }

    decoded[actualSize] = '\0';
    const uint32_t actualCrc = mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const unsigned char*>(decoded), actualSize);
    Serial.printf("[Weather] gzip decoded headerLen=%u deflateLen=%u outLen=%u expectedSize=%lu crcOk=%u\n",
                  static_cast<unsigned int>(pos),
                  static_cast<unsigned int>(deflateLen),
                  static_cast<unsigned int>(actualSize),
                  static_cast<unsigned long>(expectedSize),
                  static_cast<unsigned int>(actualCrc == expectedCrc));

    out.reserve(actualSize + 1);
    out.concat(decoded, static_cast<unsigned int>(actualSize));
    heap_caps_free(decoded);
    return actualSize == expectedSize;
}

// Keep response logging bounded and printable so compressed bytes do not corrupt serial output.
void logPayloadPreview(const char* tag, const String& payload) {
    char preview[kPayloadPreviewMax + 1] = {};
    const size_t count = payload.length() < kPayloadPreviewMax ? payload.length() : kPayloadPreviewMax;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t ch = static_cast<uint8_t>(payload[i]);
        preview[i] = (ch >= 0x20 && ch <= 0x7E) ? static_cast<char>(ch) : '.';
    }
    preview[count] = '\0';
    Serial.printf("[Weather] %s len=%u firstBytes=%02X %02X %02X %02X preview=%s%s\n",
                  tag ? tag : "payload",
                  static_cast<unsigned int>(payload.length()),
                  payload.length() > 0 ? static_cast<unsigned int>(static_cast<uint8_t>(payload[0])) : 0,
                  payload.length() > 1 ? static_cast<unsigned int>(static_cast<uint8_t>(payload[1])) : 0,
                  payload.length() > 2 ? static_cast<unsigned int>(static_cast<uint8_t>(payload[2])) : 0,
                  payload.length() > 3 ? static_cast<unsigned int>(static_cast<uint8_t>(payload[3])) : 0,
                  preview,
                  payload.length() > kPayloadPreviewMax ? "..." : "");
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
    char codeBuf[8] = {};
    char tempBuf[8] = {};
    char iconBuf[8] = {};
    char textBuf[WEATHER_TEXT_MAX] = {};

    if (extractJsonString(payload, "code", codeBuf, sizeof(codeBuf)) &&
        strcmp(codeBuf, "200") != 0) {
        Serial.printf("[Weather] qweather code=%s\n", codeBuf);
    }
    if (!extractJsonString(payload, "temp", tempBuf, sizeof(tempBuf))) {
        Serial.println("[Weather] parse missing field: now.temp");
        return false;
    }
    if (!extractJsonString(payload, "icon", iconBuf, sizeof(iconBuf))) {
        Serial.println("[Weather] parse missing field: now.icon");
        return false;
    }
    if (!extractJsonString(payload, "text", textBuf, sizeof(textBuf))) {
        Serial.println("[Weather] parse missing field: now.text");
        return false;
    }

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
    if (g_msgQueue == nullptr) {
        Serial.println("[Weather] publish skipped: g_msgQueue is null");
        return;
    }
    AppMessage msg = {};
    msg.type = MSG_WEATHER_UPDATE;
    msg.weather = weather;
    if (xQueueSend(g_msgQueue, &msg, 0) != pdPASS) {
        Serial.println("[Weather] publish failed: app message queue full");
        return;
    }
    Serial.printf("[Weather] published temp=%d icon=%u text=%s\n",
                  static_cast<int>(weather.temperatureC),
                  static_cast<unsigned int>(weather.iconCode),
                  weather.text);
}

bool fetchWeatherNow() {
    const uint32_t startMs = millis();
    WeatherRequest request;
    if (!buildWeatherRequest(request)) {
        Serial.println("[Weather] fetch skipped: QWeather config missing after sanitize");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    Serial.printf("[Weather] fetch start host=%s location=%s keyLen=%u url=%s\n",
                  request.host.c_str(),
                  request.location.c_str(),
                  static_cast<unsigned int>(request.apiKey.length()),
                  request.url.c_str());
    if (!http.begin(client, request.url)) {
        Serial.printf("[Weather] http begin failed elapsed=%lu\n",
                      static_cast<unsigned long>(millis() - startMs));
        return false;
    }

    http.setReuse(false);
    http.useHTTP10(true);
    http.setTimeout(8000);
    http.collectHeaders(kWeatherHeaderKeys, kWeatherHeaderCount);
    http.addHeader("Accept", "application/json");
    http.addHeader("Accept-Encoding", "identity");
    http.addHeader("X-QW-Api-Key", request.apiKey);

    const int status = http.GET();
    const String contentEncoding = http.header("Content-Encoding");
    const String contentType = http.header("Content-Type");
    const String transferEncoding = http.header("Transfer-Encoding");
    Serial.printf("[Weather] http status=%d elapsed=%lu contentLength=%d\n",
                  status,
                  static_cast<unsigned long>(millis() - startMs),
                  http.getSize());
    Serial.printf("[Weather] response headers contentEncoding=%s contentType=%s transferEncoding=%s\n",
                  contentEncoding.length() > 0 ? contentEncoding.c_str() : "<empty>",
                  contentType.length() > 0 ? contentType.c_str() : "<empty>",
                  transferEncoding.length() > 0 ? transferEncoding.c_str() : "<empty>");
    if (status != HTTP_CODE_OK) {
        const String body = http.getString();
        Serial.printf("[Weather] request failed status=%d error=%s\n",
                      status,
                      http.errorToString(status).c_str());
        if (body.length() > 0) {
            logPayloadPreview("error body", body);
        }
        http.end();
        return false;
    }

    const String body = http.getString();
    http.end();
    logPayloadPreview("response", body);

    String payload = body;
    if (isGzipPayload(body) || contentEncoding.equalsIgnoreCase("gzip")) {
        Serial.println("[Weather] response is gzip; decompressing before parse");
        if (!decompressGzipPayload(body, payload)) {
            Serial.println("[Weather] gzip decompress failed");
            return false;
        }
        logPayloadPreview("decompressed response", payload);
        Serial.printf("[Weather] decompressed json=%s\n", payload.c_str());
    }

    WeatherStatusMsg next = {};
    if (!parseWeatherPayload(payload, next)) {
        Serial.println("[Weather] parse failed");
        return false;
    }

    g_snapshot = next;
    publishWeather(g_snapshot);
    Serial.printf("[Weather] fetch ok elapsed=%lu\n",
                  static_cast<unsigned long>(millis() - startMs));
    return true;
}

void weatherTask(void*) {
    Serial.println("[Weather] task start");
    logConfigSummary();

    if (!isConfigured()) {
        Serial.println("[Weather] QWeather config missing; skip weather task");
        vTaskDelete(nullptr);
        return;
    }

    bool waitingLogged = false;
    while (true) {
        if (!NetworkService::isConnected()) {
            if (!waitingLogged) {
                Serial.println("[Weather] waiting for network before fetch");
                waitingLogged = true;
            }
            vTaskDelay(pdMS_TO_TICKS(kNetworkRetryMs));
            continue;
        }
        if (waitingLogged) {
            Serial.printf("[Weather] network ready ssid=%s ip=%s\n",
                          NetworkService::ssid().c_str(),
                          NetworkService::ipAddress().c_str());
            waitingLogged = false;
        }

        const bool ok = fetchWeatherNow();
        Serial.printf("[Weather] refresh done ok=%u nextMs=%lu\n",
                      static_cast<unsigned int>(ok),
                      static_cast<unsigned long>(kWeatherRefreshMs));
        vTaskDelay(pdMS_TO_TICKS(kWeatherRefreshMs));
    }
}
}

namespace WeatherService {
void begin() {
    if (g_started) {
        Serial.println("[Weather] begin skipped: already started");
        return;
    }
    g_started = true;
    const BaseType_t created = xTaskCreate(weatherTask, "weatherTask", kWeatherTaskStackBytes, nullptr, 1, nullptr);
    if (created != pdPASS) {
        g_started = false;
        Serial.println("[Weather] task create failed");
        return;
    }
    Serial.println("[Weather] task created");
}

WeatherStatusMsg snapshot() {
    return g_snapshot;
}
}
