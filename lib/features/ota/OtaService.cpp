#include "OtaService.h"

#include <core/app_message/app_message.h>
#include <features/network/NetworkService.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <esp_crt_bundle.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

extern "C" esp_err_t arduino_esp_crt_bundle_attach(void *conf);

#ifndef APP_VERSION
#define APP_VERSION "0.1.0"
#endif

#ifndef OTA_MANIFEST_URL
#define OTA_MANIFEST_URL ""
#endif

extern QueueHandle_t g_msgQueue;

namespace {
constexpr uint16_t kServerPort = 80;
constexpr uint32_t kTaskStackBytes = 10240;

const char* kUploadPage = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 OTA</title>
  <style>
    body { font-family: sans-serif; max-width: 560px; margin: 40px auto; padding: 0 16px; }
    .box { border: 1px solid #ccc; border-radius: 12px; padding: 20px; }
    h1 { margin-top: 0; }
    button { padding: 10px 18px; border-radius: 8px; border: none; background: #111; color: #fff; }
  </style>
</head>
<body>
  <div class="box">
    <h1>Firmware Upload</h1>
    <form method="POST" action="/update" enctype="multipart/form-data">
      <input type="file" name="firmware" accept=".bin" required>
      <p><button type="submit">Upload</button></p>
    </form>
  </div>
</body>
</html>
)HTML";

String extractJsonString(const String& body, const char* key) {
    String token = String("\"") + key + "\"";
    int keyPos = body.indexOf(token);
    if (keyPos < 0) return "";

    int colonPos = body.indexOf(':', keyPos + token.length());
    if (colonPos < 0) return "";

    int valueStart = body.indexOf('"', colonPos + 1);
    if (valueStart < 0) return "";

    int valueEnd = body.indexOf('"', valueStart + 1);
    if (valueEnd < 0) return "";

    return body.substring(valueStart + 1, valueEnd);
}

void copyString(char* dst, size_t dstSize, const char* src) {
    if (dstSize == 0) return;
    memset(dst, 0, dstSize);
    if (src != nullptr) {
        strncpy(dst, src, dstSize - 1);
    }
}
}  // namespace

struct OtaService::Impl {
    WebServer* server = nullptr;
    bool serverStarted = false;
    volatile bool checking = false;
    volatile bool upgrading = false;
    String remoteVersion;
    String remoteFirmwareUrl;

    esp_ota_handle_t uploadHandle = 0;
    const esp_partition_t* updatePartition = nullptr;
    size_t uploadTotal = 0;
    size_t uploadWritten = 0;
    bool uploadFailed = false;
};

OtaService::OtaService()
    : impl_(new Impl()) {}

OtaService::~OtaService() {
    end();
    delete impl_;
    impl_ = nullptr;
}

void OtaService::begin() {
    ensureServer();
    publishHome();
}

void OtaService::end() {
    stopServer();
}

void OtaService::tick() {
    ensureServer();
    if (impl_->serverStarted && impl_->server != nullptr) {
        impl_->server->handleClient();
    }
}

void OtaService::publishHome() {
    String ipUrl = currentIp();
    if (ipUrl.length() > 0) {
        ipUrl = "http://" + ipUrl + "/";
    }
    publishStatus(OtaPhase::HOME, 0, false, APP_VERSION, impl_->remoteVersion.c_str(),
                  ipUrl.c_str(), "");
}

void OtaService::startCheck() {
    if (isBusy()) return;

    if (!hasNetwork()) {
        publishStatus(OtaPhase::RESULT, 0, false, APP_VERSION, "",
                      "", "超时退出");
        return;
    }

    impl_->checking = true;
    publishStatus(OtaPhase::CHECKING, 0, false, APP_VERSION, "",
                  currentIp().c_str(), "请稍候");
    xTaskCreate(checkTaskEntry, "otaCheck", kTaskStackBytes, this, 2, nullptr);
}

void OtaService::confirmRemoteUpgrade() {
    if (impl_->upgrading || impl_->remoteFirmwareUrl.length() == 0) return;

    impl_->upgrading = true;
    publishStatus(OtaPhase::UPDATING, 0, false, APP_VERSION,
                  impl_->remoteVersion.c_str(), currentIp().c_str(),
                  "当前有固件在升级中，请勿断电");
    xTaskCreate(upgradeTaskEntry, "otaUpgrade", kTaskStackBytes, this, 3, nullptr);
}

bool OtaService::isBusy() const {
    return impl_->checking || impl_->upgrading;
}

void OtaService::ensureServer() {
    if (!hasNetwork()) {
        stopServer();
        return;
    }

    if (impl_->server == nullptr) {
        impl_->server = new WebServer(kServerPort);

        impl_->server->on("/", HTTP_GET, [this]() {
            impl_->server->send(200, "text/html; charset=utf-8", kUploadPage);
        });

        impl_->server->on("/update", HTTP_POST,
            [this]() {
                if (impl_->uploadFailed) {
                    impl_->server->send(500, "text/plain", "OTA failed");
                    publishStatus(OtaPhase::RESULT, 0, false, APP_VERSION,
                                  impl_->remoteVersion.c_str(), currentIp().c_str(),
                                  "超时退出");
                } else {
                    impl_->server->send(200, "text/plain", "OK");
                    publishStatus(OtaPhase::RESULT, 100, true, APP_VERSION,
                                  impl_->remoteVersion.c_str(), currentIp().c_str(),
                                  "升级成功");
                }
            },
            [this]() {
                HTTPUpload& upload = impl_->server->upload();
                switch (upload.status) {
                case UPLOAD_FILE_START: {
                    impl_->uploadFailed = false;
                    impl_->upgrading = true;
                    impl_->uploadTotal = upload.totalSize;
                    impl_->uploadWritten = 0;
                    impl_->uploadHandle = 0;
                    impl_->updatePartition = esp_ota_get_next_update_partition(nullptr);
                    if (impl_->updatePartition == nullptr ||
                        esp_ota_begin(impl_->updatePartition, OTA_SIZE_UNKNOWN,
                                      &impl_->uploadHandle) != ESP_OK) {
                        impl_->uploadFailed = true;
                    } else {
                        publishStatus(OtaPhase::UPDATING, 0, false, APP_VERSION, "",
                                      currentIp().c_str(),
                                      "当前有固件在升级中，请勿断电");
                    }
                    break;
                }
                case UPLOAD_FILE_WRITE: {
                    if (!impl_->uploadFailed) {
                        if (esp_ota_write(impl_->uploadHandle, upload.buf,
                                          upload.currentSize) != ESP_OK) {
                            impl_->uploadFailed = true;
                        } else {
                            impl_->uploadWritten += upload.currentSize;
                            uint8_t percent = 0;
                            if (impl_->uploadTotal > 0) {
                                percent = (uint8_t)((impl_->uploadWritten * 100U) /
                                                    impl_->uploadTotal);
                            }
                            publishStatus(OtaPhase::UPDATING, percent, false,
                                          APP_VERSION, "", currentIp().c_str(),
                                          "当前有固件在升级中，请勿断电");
                        }
                    }
                    break;
                }
                case UPLOAD_FILE_END: {
                    if (!impl_->uploadFailed) {
                        bool ok = esp_ota_end(impl_->uploadHandle) == ESP_OK &&
                                  esp_ota_set_boot_partition(impl_->updatePartition) == ESP_OK;
                        impl_->uploadFailed = !ok;
                    } else if (impl_->uploadHandle != 0) {
                        esp_ota_abort(impl_->uploadHandle);
                    }
                    impl_->uploadHandle = 0;
                    impl_->upgrading = false;
                    break;
                }
                case UPLOAD_FILE_ABORTED:
                default:
                    impl_->uploadFailed = true;
                    impl_->upgrading = false;
                    if (impl_->uploadHandle != 0) {
                        esp_ota_abort(impl_->uploadHandle);
                    }
                    impl_->uploadHandle = 0;
                    break;
                }
            });

        impl_->server->begin();
        impl_->serverStarted = true;
        Serial.printf("[OTA] Upload server started: http://%s/\n", currentIp().c_str());
    }
}

void OtaService::stopServer() {
    if (impl_->server != nullptr) {
        impl_->server->stop();
        delete impl_->server;
        impl_->server = nullptr;
    }
    impl_->serverStarted = false;
}

void OtaService::publishStatus(OtaPhase phase, uint8_t progress, bool success,
                               const char* currentVersion, const char* targetVersion,
                               const char* ip, const char* message) {
    if (g_msgQueue == nullptr) return;

    AppMessage msg;
    msg.type = MSG_OTA_STATUS;
    msg.ota.phase = phase;
    msg.ota.progressPercent = progress;
    msg.ota.success = success;
    copyString(msg.ota.currentVersion, sizeof(msg.ota.currentVersion), currentVersion);
    copyString(msg.ota.targetVersion, sizeof(msg.ota.targetVersion), targetVersion);
    copyString(msg.ota.ip, sizeof(msg.ota.ip), ip);
    copyString(msg.ota.message, sizeof(msg.ota.message), message);
    xQueueSend(g_msgQueue, &msg, 0);
}

bool OtaService::parseManifest(const String& body, String& version, String& firmwareUrl) const {
    version = extractJsonString(body, "version");
    firmwareUrl = extractJsonString(body, "firmware_url");
    return version.length() > 0 && firmwareUrl.length() > 0;
}

int OtaService::compareVersions(const char* lhs, const char* rhs) const {
    if (lhs == nullptr || rhs == nullptr) return 0;

    const char* p1 = lhs;
    const char* p2 = rhs;
    while (*p1 != '\0' || *p2 != '\0') {
        long v1 = 0;
        long v2 = 0;
        while (*p1 >= '0' && *p1 <= '9') {
            v1 = v1 * 10 + (*p1 - '0');
            p1++;
        }
        while (*p2 >= '0' && *p2 <= '9') {
            v2 = v2 * 10 + (*p2 - '0');
            p2++;
        }
        if (v1 != v2) return (v1 > v2) ? 1 : -1;
        if (*p1 == '.') p1++;
        if (*p2 == '.') p2++;
    }
    return 0;
}

bool OtaService::startLocalUploadIfPossible() {
    if (!hasNetwork()) return false;
    ensureServer();
    publishHome();
    return true;
}

bool OtaService::hasNetwork() const {
    return NetworkService::isConnected();
}

String OtaService::currentIp() const {
    if (!hasNetwork()) return "";
    return NetworkService::ipAddress();
}

void OtaService::checkTaskEntry(void* arg) {
    OtaService* self = static_cast<OtaService*>(arg);

    const char* manifestUrl = OTA_MANIFEST_URL;
    if (manifestUrl[0] == '\0') {
        self->impl_->checking = false;
        if (!self->startLocalUploadIfPossible()) {
            self->publishStatus(OtaPhase::RESULT, 0, false, APP_VERSION, "",
                                "", "超时退出");
        }
        vTaskDelete(nullptr);
        return;
    }

    HTTPClient http;
    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    bool ok = false;
    int statusCode = -1;
    String body;

    String manifest = manifestUrl;
    if (manifest.startsWith("https://")) {
        secureClient.setInsecure();
        ok = http.begin(secureClient, manifest);
    } else {
        ok = http.begin(plainClient, manifest);
    }

    if (ok) {
        http.setTimeout(5000);
        statusCode = http.GET();
        if (statusCode == HTTP_CODE_OK) {
            body = http.getString();
        }
        http.end();
    }

    self->impl_->checking = false;

    if (statusCode != HTTP_CODE_OK) {
        self->publishStatus(OtaPhase::RESULT, 0, false, APP_VERSION, "",
                            self->currentIp().c_str(), "超时退出");
        vTaskDelete(nullptr);
        return;
    }

    String version;
    String firmwareUrl;
    if (!self->parseManifest(body, version, firmwareUrl)) {
        self->publishStatus(OtaPhase::RESULT, 0, false, APP_VERSION, "",
                            self->currentIp().c_str(), "超时退出");
        vTaskDelete(nullptr);
        return;
    }

    if (self->compareVersions(version.c_str(), APP_VERSION) > 0) {
        self->impl_->remoteVersion = version;
        self->impl_->remoteFirmwareUrl = firmwareUrl;
        self->publishStatus(OtaPhase::CONFIRM, 0, false, APP_VERSION,
                            self->impl_->remoteVersion.c_str(),
                            self->currentIp().c_str(), "KEY1=升级 KEY2=取消");
    } else {
        self->impl_->remoteVersion = version;
        self->impl_->remoteFirmwareUrl = firmwareUrl;
        if (!self->startLocalUploadIfPossible()) {
            self->publishStatus(OtaPhase::RESULT, 0, false, APP_VERSION,
                                version.c_str(), "", "超时退出");
        }
    }

    vTaskDelete(nullptr);
}

void OtaService::upgradeTaskEntry(void* arg) {
    OtaService* self = static_cast<OtaService*>(arg);

    esp_http_client_config_t httpConfig = {};
    httpConfig.url = self->impl_->remoteFirmwareUrl.c_str();
    httpConfig.timeout_ms = 10000;
    httpConfig.crt_bundle_attach = arduino_esp_crt_bundle_attach;

    esp_https_ota_config_t otaConfig = {};
    otaConfig.http_config = &httpConfig;

    esp_https_ota_handle_t handle = nullptr;
    esp_err_t err = esp_https_ota_begin(&otaConfig, &handle);
    if (err != ESP_OK) {
        self->impl_->upgrading = false;
        self->publishStatus(OtaPhase::RESULT, 0, false, APP_VERSION,
                            self->impl_->remoteVersion.c_str(),
                            self->currentIp().c_str(), "超时退出");
        vTaskDelete(nullptr);
        return;
    }

    while (true) {
        err = esp_https_ota_perform(handle);
        int total = esp_https_ota_get_image_size(handle);
        int read = esp_https_ota_get_image_len_read(handle);
        uint8_t percent = 0;
        if (total > 0 && read >= 0) {
            percent = (uint8_t)((read * 100U) / total);
        }
        self->publishStatus(OtaPhase::UPDATING, percent, false, APP_VERSION,
                            self->impl_->remoteVersion.c_str(),
                            self->currentIp().c_str(),
                            "当前有固件在升级中，请勿断电");
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;
    }

    bool success = (err == ESP_OK) && esp_https_ota_is_complete_data_received(handle) &&
                   (esp_https_ota_finish(handle) == ESP_OK);
    if (!success && handle != nullptr) {
        esp_https_ota_abort(handle);
    }

    self->impl_->upgrading = false;
    self->publishStatus(OtaPhase::RESULT, success ? 100 : 0, success, APP_VERSION,
                        self->impl_->remoteVersion.c_str(),
                        self->currentIp().c_str(),
                        success ? "升级成功" : "超时退出");
    vTaskDelete(nullptr);
}
