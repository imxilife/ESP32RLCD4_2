#pragma once

#include <Arduino.h>
#include <core/app_message/app_message.h>

class OtaService {
public:
    OtaService();
    ~OtaService();

    void begin();
    void end();
    void tick();

    void publishHome();
    void startCheck();
    void confirmRemoteUpgrade();

    bool isBusy() const;

private:
    struct Impl;
    Impl* impl_;

    void ensureServer();
    void stopServer();
    void publishStatus(OtaPhase phase, uint8_t progress, bool success,
                       const char* currentVersion, const char* targetVersion,
                       const char* ip, const char* message);

    bool parseManifest(const String& body, String& version, String& firmwareUrl) const;
    int compareVersions(const char* lhs, const char* rhs) const;
    bool startLocalUploadIfPossible();
    bool hasNetwork() const;
    String currentIp() const;

    static void checkTaskEntry(void* arg);
    static void upgradeTaskEntry(void* arg);
};
