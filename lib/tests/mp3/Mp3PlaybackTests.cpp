#include "Mp3PlaybackTests.h"

#include <Arduino.h>
#include <device/sdcard/SDCard.h>
#include <media/audio/AudioCodec.h>
#include <media/mp3_player/Mp3Player.h>

namespace {
constexpr int kMaxPlaylist = 32;
constexpr uint32_t kTrackPollMs = 200;
constexpr uint32_t kTrackStartTimeoutMs = 3000;
constexpr uint32_t kTrackFinishTimeoutMs = 10UL * 60UL * 1000UL;

bool waitForTrackToStart(Mp3Player& player, const char* path) {
    const uint32_t startMs = millis();
    Serial.printf("[MP3 TEST] Wait for track start: %s\n", path);
    while ((millis() - startMs) < kTrackStartTimeoutMs) {
        if (player.isPlaying()) {
            Serial.printf("[MP3 TEST] Track started: %s\n", path);
            return true;
        }
        delay(kTrackPollMs);
    }

    Serial.printf("[MP3 TEST] Track start timeout: %s\n", path);
    return false;
}

bool waitForTrackToFinish(Mp3Player& player, const char* path) {
    const uint32_t startMs = millis();
    Serial.printf("[MP3 TEST] Wait for track finish: %s\n", path);
    while ((millis() - startMs) < kTrackFinishTimeoutMs) {
        if (player.hasTrackFinished()) {
            Serial.printf("[MP3 TEST] Track finished: %s\n", path);
            return true;
        }
        delay(kTrackPollMs);
    }

    Serial.printf("[MP3 TEST] Track finish timeout: %s\n", path);
    return false;
}
}

bool Mp3PlaybackTests::runAllTests() {
    Serial.println("====== MP3 Playback Tests ======");
    Serial.println("[MP3 TEST] Stage 1: Mount SD card");

    SDCard sd;
    if (!sd.begin()) {
        Serial.println("[MP3 TEST] Stage 1 failed: SD mount failed");
        Serial.println("====== MP3 Playback Tests FAILED ======");
        return false;
    }
    Serial.println("[MP3 TEST] Stage 1 success: SD mounted");

    Serial.println("[MP3 TEST] Stage 2: Init audio codec");
    AudioCodec audio;
    if (!audio.begin(16000)) {
        Serial.println("[MP3 TEST] Stage 2 failed: Audio codec init failed");
        sd.end();
        Serial.println("====== MP3 Playback Tests FAILED ======");
        return false;
    }
    Serial.println("[MP3 TEST] Stage 2 success: Audio codec ready");

    Serial.println("[MP3 TEST] Stage 3: Init MP3 player");
    Mp3Player player;
    if (!player.begin()) {
        Serial.println("[MP3 TEST] Stage 3 failed: Mp3Player init failed");
        sd.end();
        Serial.println("====== MP3 Playback Tests FAILED ======");
        return false;
    }
    Serial.println("[MP3 TEST] Stage 3 success: Mp3Player ready");

    Serial.println("[MP3 TEST] Stage 4: Scan /music");
    String playlist[kMaxPlaylist];
    const int playlistCount = player.scanMp3Files("/music", playlist, kMaxPlaylist);
    Serial.printf("[MP3 TEST] Stage 4 result: %d file(s) found in /music\n", playlistCount);
    if (playlistCount <= 0) {
        Serial.println("[MP3 TEST] Stage 4 failed: /music missing or no MP3 files");
        sd.end();
        Serial.println("====== MP3 Playback Tests FAILED ======");
        return false;
    }

    bool anyPlayed = false;
    bool allPass = true;

    Serial.println("[MP3 TEST] Stage 5: Sequential playback");
    for (int i = 0; i < playlistCount; ++i) {
        const char* path = playlist[i].c_str();
        int lastSlash = playlist[i].lastIndexOf('/');
        const char* filename = (lastSlash >= 0) ? path + lastSlash + 1 : path;

        Serial.printf("[MP3 TEST] Track %d/%d prepare: %s\n",
                      i + 1, playlistCount, path);

        if (!player.play(path)) {
            Serial.printf("[MP3 TEST] Track %d/%d failed to start: %s\n",
                          i + 1, playlistCount, path);
            allPass = false;
            continue;
        }

        if (!waitForTrackToStart(player, path)) {
            Serial.printf("[MP3 TEST] Track %d/%d start confirmation failed, stop and skip: %s\n",
                          i + 1, playlistCount, path);
            player.stop();
            allPass = false;
            continue;
        }

        anyPlayed = true;
        Serial.printf("[MP3 TEST] Track %d/%d playing: %s\n",
                      i + 1, playlistCount, filename);

        if (!waitForTrackToFinish(player, path)) {
            Serial.printf("[MP3 TEST] Track %d/%d finish wait failed, stop and skip: %s\n",
                          i + 1, playlistCount, path);
            player.stop();
            allPass = false;
            continue;
        }

        Serial.printf("[MP3 TEST] Track %d/%d completed: %s\n",
                      i + 1, playlistCount, filename);
    }

    Serial.println("[MP3 TEST] Stage 6: Stop playback pipeline");
    player.stop();
    sd.end();
    Serial.println("[MP3 TEST] Stage 6 success: Playback stopped and SD unmounted");

    if (!anyPlayed) {
        Serial.println("[MP3 TEST] No playable MP3 files completed");
        Serial.println("====== MP3 Playback Tests FAILED ======");
        return false;
    }

    Serial.printf("====== MP3 Playback Tests %s ======\n",
                  allPass ? "ALL PASSED" : "COMPLETED WITH SKIPS");
    return allPass;
}
