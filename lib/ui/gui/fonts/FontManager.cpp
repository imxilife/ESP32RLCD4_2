#include "FontManager.h"

#include <Arduino.h>
#include <SPIFFS.h>

/**
 * FontManager.cpp
 *
 * 类的作用：
 * - FontManager 是项目字体系统的统一入口。
 * - 它负责 SPIFFS 挂载、字体规格注册、稳定 Font 描述符、fallback 链、懒加载、LRU 卸载和状态查询。
 * - 它不解析字体 bin 内容；具体 F24BIN1 header 校验、索引解析和字形读取由 FontBinLoader 完成。
 * - 单个字体加载后的运行状态由 FontRuntime 保存，避免 loader 和运行态职责混在一起。
 *
 * 字体加载到使用的流程：
 * step1: main.cpp 在 setup() 中调用 FontManager::instance().init()。
 * step2: init() 挂载 SPIFFS，并注册项目字体规格。
 * step3: setupSlots() 为每个字体建立稳定 Font 描述符和 fallback 链，但不打开 bin 文件。
 * step4: 页面通过 font(FontId) 获取稳定 const Font*，并交给 Gui::setFont()。
 * step5: Gui::drawText() 或 Gui::measureTextWidth() 解码 UTF-8，得到 Unicode codepoint。
 * step6: Gui 调用 Font.getGlyph，也就是 FontManager::getGlyphThunk()。
 * step7: getGlyphThunk() 根据 Font.data 找回 FontSlot，并调用 FontManager::getGlyph()。
 * step8: getGlyph() 调用 load(FontId)；如果目标字体未加载，loader_ 会加载该槽位的 FontRuntime。
 * step9: FontBinLoader 打开 SPIFFS bin、校验 F24BIN1 header，并把元数据写入 FontRuntime。
 * step10: FontRuntime 优先持有 PSRAM 索引缓存；PSRAM 不可用时保留 SPIFFS 按需读索引路径。
 * step11: FontBinLoader 对 codepoint 做二分查找，按需读取单个字形 bitmap 到 FontRuntime 小缓存。
 * step12: Gui 拿到 bitmap / width / height / stride / advanceX 后写入帧缓冲。
 * step13: 如果目标字体没有该码点，Gui 沿 Font.fallback 链继续查找，fallback 字体同样懒加载。
 * step14: 每次字体访问都会 touch() 更新 LRU，trimLoadedFonts() 超限时卸载最久未用字体。
 * step15: end() 或 LRU unload() 释放 FontRuntime 的文件句柄和 PSRAM 索引缓存。
 */

// 固定字体规格表：
// - id: 业务侧使用的字体枚举。
// - name: 串口日志里的短名称，便于排查懒加载和 LRU 行为。
// - path: SPIFFS 中的字体 bin 路径，必须与 data/ 下的文件名一致。
// - defaultLineHeight: 字体未加载前暴露给 Gui 的默认行高，加载成功后会被 bin header 覆盖。
// - fallbackIndex: 找不到字形时的 fallback 槽位，kNoFallback 表示没有回退字体。
const FontManager::FontSpec FontManager::kSpecs[FontManager::kFontCount] = {
    {FontId::ZhMain, "zhMain", "/font_zh_main_24.bin", 24, FontManager::kNoFallback},
    {FontId::ZhSub, "zhSub", "/font_zh_sub_20.bin", 20, 0},
    {FontId::EnMain, "enMain", "/font_en_main_18.bin", 18, 0},
    {FontId::EnSub, "enSub", "/font_en_sub_16.bin", 16, 2},
    {FontId::Digits60, "digits60", "/font_digits_60.bin", 60, 2},
};

// 全局单例入口。局部 static 避免静态初始化顺序问题，首次使用时构造。
FontManager& FontManager::instance() {
    static FontManager manager;
    return manager;
}

// 构造函数只初始化内存状态和稳定 Font 槽位，不挂载 SPIFFS，也不打开任何字体文件。
FontManager::FontManager()
    : fs_(nullptr),
      initialized_(false),
      config_(),
      loader_(),
      accessClock_(0),
      lastError_("not initialized"),
      failedPath_(nullptr) {
    setupSlots();
}

// 初始化字体系统：挂载 SPIFFS、保存配置、重建稳定 Font 槽位。
// 注意：这里不加载任何字体 bin，避免启动阶段一次性占用 PSRAM 和打开文件。
bool FontManager::init(const FontManagerConfig& config) {
    end();

    config_ = config;
    if (config_.maxLoadedFonts == 0) {
        config_.maxLoadedFonts = 1;
    }
    setupSlots();

    if (!SPIFFS.begin(false)) {
        setFailure(nullptr, "SPIFFS mount failed");
        Serial.println("[FontManager] SPIFFS mount failed");
        return false;
    }

    fs_ = &SPIFFS;
    initialized_ = true;
    lastError_ = "ok";
    failedPath_ = nullptr;
    Serial.printf("[FontManager] init ok maxLoaded=%u indexBudget=%u bitmapBudget=%u preferPsram=%d\n",
                  static_cast<unsigned int>(config_.maxLoadedFonts),
                  static_cast<unsigned int>(config_.maxIndexCacheBytes),
                  static_cast<unsigned int>(config_.maxBitmapCacheBytes),
                  config_.preferPsram ? 1 : 0);
    return true;
}

// 结束字体系统：卸载所有运行态，关闭 SPIFFS，并回到未初始化状态。
// 稳定 Font 槽位会重新设置，避免后续误用旧 fallback 指针。
void FontManager::end() {
    unloadAll();
    if (initialized_) {
        SPIFFS.end();
    }
    fs_ = nullptr;
    initialized_ = false;
    accessClock_ = 0;
    lastError_ = "not initialized";
    failedPath_ = nullptr;
    setupSlots();
}

// 返回稳定 Font 指针。此方法只提供描述符，不触发文件打开或 PSRAM 分配。
const Font* FontManager::font(FontId id) {
    FontSlot* slot = slotFor(id);
    if (!initialized_ || slot == nullptr) {
        return nullptr;
    }
    return &slot->font;
}

// 显式或懒加载某个字体。
// 成功后 slot.runtime 持有打开的 SPIFFS 文件句柄，并可能持有 PSRAM 索引缓存。
bool FontManager::load(FontId id) {
    FontSlot* slot = slotFor(id);
    if (slot == nullptr) {
        setFailure(nullptr, "invalid font id");
        return false;
    }
    if (!initialized_ || fs_ == nullptr) {
        setFailure(slot->spec != nullptr ? slot->spec->path : nullptr, "font manager not initialized");
        return false;
    }
    if (slot->loadFailed) {
        setFailure(slot->spec->path, slot->runtime.lastError());
        return false;
    }
    if (slot->runtime.ready()) {
        touch(*slot);
        trimLoadedFonts(id);
        return true;
    }

    const uint32_t loadStartMs = millis();
    Serial.printf("[FontManager] load start %s %s t=%lu\n",
                  slot->spec->name,
                  slot->spec->path,
                  static_cast<unsigned long>(loadStartMs));
    if (!loader_.load(*fs_,
                      slot->spec->path,
                      config_.preferPsram,
                      config_.maxIndexCacheBytes,
                      config_.maxBitmapCacheBytes,
                      slot->runtime)) {
        slot->lastUse = 0;
        slot->loadFailed = true;
        setFailure(slot->spec->path, slot->runtime.lastError());
        Serial.printf("[FontManager] load failed %s error=%s dur=%lu\n",
                      slot->spec->path,
                      slot->runtime.lastError(),
                      static_cast<unsigned long>(millis() - loadStartMs));
        return false;
    }

    // lineHeight 以 bin header 为准，默认值只用于未加载前的安全占位。
    slot->font.lineHeight = slot->runtime.lineHeight();
    slot->loadFailed = false;
    touch(*slot);
    trimLoadedFonts(id);
    lastError_ = "ok";
    failedPath_ = nullptr;

    Serial.printf("[FontManager] loaded %s glyphs=%u indexBytes=%u psram=%d bitmapBytes=%u bitmapPsram=%d dur=%lu\n",
                  slot->spec->name,
                  static_cast<unsigned int>(slot->runtime.glyphCount()),
                  static_cast<unsigned int>(slot->runtime.indexBytes()),
                  slot->runtime.indexInPsram() ? 1 : 0,
                  static_cast<unsigned int>(slot->runtime.bitmapBytes()),
                  slot->runtime.bitmapInPsram() ? 1 : 0,
                  static_cast<unsigned long>(millis() - loadStartMs));
    return true;
}

// 卸载单个字体运行态。
// 这会关闭文件并释放 PSRAM 索引，但不会销毁 Font 描述符；后续访问仍可再次懒加载。
void FontManager::unload(FontId id) {
    FontSlot* slot = slotFor(id);
    if (slot == nullptr) {
        return;
    }
    if (slot->runtime.ready()) {
        Serial.printf("[FontManager] unload %s\n", slot->spec->name);
    }
    loader_.unload(slot->runtime);
    slot->font.lineHeight = slot->spec->defaultLineHeight;
    slot->lastUse = 0;
    slot->loadFailed = false;
}

// 卸载所有字体运行态，用于 end() 或清空缓存。
void FontManager::unloadAll() {
    for (uint8_t i = 0; i < kFontCount; ++i) {
        loader_.unload(slots_[i].runtime);
        slots_[i].lastUse = 0;
        slots_[i].loadFailed = false;
        if (slots_[i].spec != nullptr) {
            slots_[i].font.lineHeight = slots_[i].spec->defaultLineHeight;
        }
    }
}

// 查询指定字体当前是否已经打开并通过格式校验。
bool FontManager::isLoaded(FontId id) const {
    const FontSlot* slot = slotFor(id);
    return slot != nullptr && slot->runtime.ready();
}

// 返回诊断状态。未加载字体也会返回固定 path 和 runtime 最近错误。
FontStatus FontManager::status(FontId id) const {
    const FontSlot* slot = slotFor(id);
    if (slot == nullptr || slot->spec == nullptr) {
        return {false, false, nullptr, "invalid font id", 0, 0};
    }

    return {
        slot->runtime.ready(),
        slot->runtime.indexInPsram(),
        slot->spec->path,
        slot->runtime.lastError(),
        slot->runtime.glyphCount(),
        slot->runtime.indexBytes(),
    };
}

// 返回最近一次全局失败原因。单个字体 runtime 的错误可通过 status(id).error 获取。
const char* FontManager::lastError() const {
    return lastError_;
}

// 返回最近一次失败关联的路径；SPIFFS 挂载失败或非法 id 时可能为空。
const char* FontManager::failedPath() const {
    return failedPath_;
}

// Font.getGlyph 的静态桥接函数。
// Font 只能保存函数指针和 void* data，因此这里用 data 找回对应 FontSlot。
const uint8_t* FontManager::getGlyphThunk(uint32_t codepoint,
                                          int& w,
                                          int& h,
                                          int& strideBytes,
                                          int& advanceX,
                                          const void* data) {
    FontSlot* slot = static_cast<FontSlot*>(const_cast<void*>(data));
    if (slot == nullptr) {
        return nullptr;
    }
    return FontManager::instance().getGlyph(*slot, codepoint, w, h, strideBytes, advanceX);
}

// Font.getGlyphMetrics 的静态桥接函数。测宽只需要 advance/尺寸，不读取 bitmap。
bool FontManager::getGlyphMetricsThunk(uint32_t codepoint,
                                       int& w,
                                       int& h,
                                       int& strideBytes,
                                       int& advanceX,
                                       const void* data) {
    FontSlot* slot = static_cast<FontSlot*>(const_cast<void*>(data));
    if (slot == nullptr) {
        return false;
    }
    return FontManager::instance().getGlyphMetrics(*slot, codepoint, w, h, strideBytes, advanceX);
}

// FontId 当前直接映射到 slots_ 下标，必须保持 FontId 枚举顺序与 kSpecs 一致。
bool FontManager::isValidId(FontId id) {
    return static_cast<uint8_t>(id) < kFontCount;
}

// 将 FontId 转为槽位下标。调用前通常先经过 isValidId() 校验。
uint8_t FontManager::toIndex(FontId id) {
    return static_cast<uint8_t>(id);
}

// 获取可写槽位；非法 FontId 返回 nullptr。
FontManager::FontSlot* FontManager::slotFor(FontId id) {
    if (!isValidId(id)) {
        return nullptr;
    }
    return &slots_[toIndex(id)];
}

// 获取只读槽位；用于 const 查询方法。
const FontManager::FontSlot* FontManager::slotFor(FontId id) const {
    if (!isValidId(id)) {
        return nullptr;
    }
    return &slots_[toIndex(id)];
}

// 字形查找入口：
// - 先确保当前槽位对应字体已经加载到 FontRuntime。
// - 再更新 LRU。
// - 最后委托 FontBinLoader 在 runtime 中查找和读取 bitmap。
const uint8_t* FontManager::getGlyph(FontSlot& slot,
                                     uint32_t codepoint,
                                     int& w,
                                     int& h,
                                     int& strideBytes,
                                     int& advanceX) {
    if (slot.spec == nullptr) {
        return nullptr;
    }
    if (!load(slot.spec->id)) {
        return nullptr;
    }

    touch(slot);
    trimLoadedFonts(slot.spec->id);
    return loader_.getGlyph(slot.runtime, codepoint, w, h, strideBytes, advanceX);
}

// 字形指标查询入口。用于测宽路径，只查索引 entry，不读取 bitmap。
bool FontManager::getGlyphMetrics(FontSlot& slot,
                                  uint32_t codepoint,
                                  int& w,
                                  int& h,
                                  int& strideBytes,
                                  int& advanceX) {
    if (slot.spec == nullptr) {
        return false;
    }
    if (!load(slot.spec->id)) {
        return false;
    }

    touch(slot);
    trimLoadedFonts(slot.spec->id);
    return loader_.getGlyphMetrics(slot.runtime, codepoint, w, h, strideBytes, advanceX);
}

// 初始化或重建全部 FontSlot。
// 关键点：Font.data 指向自己的 FontSlot，fallback 指向另一个槽位的稳定 Font 描述符。
void FontManager::setupSlots() {
    for (uint8_t i = 0; i < kFontCount; ++i) {
        slots_[i].spec = &kSpecs[i];
        slots_[i].font.lineHeight = kSpecs[i].defaultLineHeight;
        slots_[i].font.getGlyph = &FontManager::getGlyphThunk;
        slots_[i].font.getGlyphMetrics = &FontManager::getGlyphMetricsThunk;
        slots_[i].font.data = &slots_[i];
        slots_[i].font.fallback =
            (kSpecs[i].fallbackIndex == kNoFallback) ? nullptr : &slots_[kSpecs[i].fallbackIndex].font;
        slots_[i].loadFailed = false;
    }
}

// 更新 LRU 访问时间。
// accessClock_ 极端溢出到 0 时跳回 1，避免 0 和“未加载/已卸载”语义冲突。
void FontManager::touch(FontSlot& slot) {
    if (++accessClock_ == 0) {
        accessClock_ = 1;
    }
    slot.lastUse = accessClock_;
}

// 根据 maxLoadedFonts 和 maxIndexCacheBytes 修剪缓存。
// keepId 表示当前正在使用的字体，不能作为本轮淘汰对象。
void FontManager::trimLoadedFonts(FontId keepId) {
    const uint8_t keepIndex = toIndex(keepId);

    while (loadedCount() > config_.maxLoadedFonts ||
           loadedIndexBytes() > config_.maxIndexCacheBytes) {
        int victim = -1;
        uint32_t oldest = UINT32_MAX;
        for (uint8_t i = 0; i < kFontCount; ++i) {
            if (i == keepIndex || !slots_[i].runtime.ready()) {
                continue;
            }
            if (slots_[i].lastUse < oldest) {
                oldest = slots_[i].lastUse;
                victim = i;
            }
        }

        if (victim < 0) {
            break;
        }
        unload(kSpecs[victim].id);
    }
}

// 统计当前已加载字体数量，用于 LRU 数量阈值判断。
uint8_t FontManager::loadedCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < kFontCount; ++i) {
        if (slots_[i].runtime.ready()) {
            ++count;
        }
    }
    return count;
}

// 统计已放入 PSRAM 的索引字节数，用于索引缓存预算判断。
size_t FontManager::loadedIndexBytes() const {
    size_t total = 0;
    for (uint8_t i = 0; i < kFontCount; ++i) {
        if (slots_[i].runtime.ready() && slots_[i].runtime.indexInPsram()) {
            total += slots_[i].runtime.indexBytes();
        }
    }
    return total;
}

// 记录全局失败路径和错误。
// path 可以为空，例如 SPIFFS 挂载失败或传入非法 FontId。
void FontManager::setFailure(const char* path, const char* error) {
    failedPath_ = path;
    lastError_ = error != nullptr ? error : "unknown error";
}
