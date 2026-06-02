#include "MemoService.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <cstring>

extern QueueHandle_t g_msgQueue;

namespace {
constexpr uint8_t kMaxMemoItems = 16;
MemoItem g_items[kMaxMemoItems];
uint16_t g_nextId = 1;

bool isSameDay(const MemoItem& item, const RTCTime& date) {
    return item.active &&
           item.year == date.year &&
           item.month == date.month &&
           item.day == date.day;
}

void copyMemoText(char* dst, const char* src) {
    if (dst == nullptr) return;
    dst[0] = '\0';
    if (src == nullptr) return;
    strncpy(dst, src, MEMO_TEXT_MAX - 1);
    dst[MEMO_TEXT_MAX - 1] = '\0';
}
}

namespace MemoService {
void begin() {
    // Memo 目前只保存在固定大小 RAM 表中；后续语音链路接入时复用同一组增删查接口。
}

uint16_t addMemo(const RTCTime& date, const char* text, uint8_t priority) {
    if (text == nullptr || text[0] == '\0') return 0;

    MemoItem* slot = nullptr;
    for (auto& item : g_items) {
        if (!item.active) {
            slot = &item;
            break;
        }
    }
    if (slot == nullptr) {
        slot = &g_items[0];
        for (auto& item : g_items) {
            if (item.priority < slot->priority) slot = &item;
        }
    }

    slot->id = g_nextId++;
    if (g_nextId == 0) g_nextId = 1;
    slot->year = date.year;
    slot->month = date.month;
    slot->day = date.day;
    slot->priority = priority;
    slot->active = true;
    copyMemoText(slot->text, text);

    postUpdate(date);
    return slot->id;
}

bool deleteMemo(uint16_t id) {
    if (id == 0) return false;
    for (auto& item : g_items) {
        if (item.active && item.id == id) {
            RTCTime date = {};
            date.year = item.year;
            date.month = item.month;
            date.day = item.day;
            item.active = false;
            postUpdate(date);
            return true;
        }
    }
    return false;
}

uint8_t topToday(const RTCTime& date, MemoItem* out, uint8_t maxCount) {
    if (out == nullptr || maxCount == 0) return 0;

    uint8_t count = 0;
    for (const auto& item : g_items) {
        if (!isSameDay(item, date)) continue;

        uint8_t insertAt = count;
        while (insertAt > 0 && out[insertAt - 1].priority < item.priority) {
            if (insertAt < maxCount) out[insertAt] = out[insertAt - 1];
            --insertAt;
        }

        if (insertAt < maxCount) out[insertAt] = item;
        if (count < maxCount) ++count;
    }
    return count;
}

uint8_t countToday(const RTCTime& date) {
    uint8_t count = 0;
    for (const auto& item : g_items) {
        if (isSameDay(item, date)) ++count;
    }
    return count;
}

void postUpdate(const RTCTime& date) {
    if (g_msgQueue == nullptr) return;

    AppMessage msg = {};
    msg.type = MSG_MEMO_UPDATE;
    msg.memo.count = countToday(date);
    xQueueSend(g_msgQueue, &msg, 0);
}
}
