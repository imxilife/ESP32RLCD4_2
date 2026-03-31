#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

#include "Mp3Player.h"
#include <media/audio/Es8311Init.h>
#include "driver/i2s.h"
#include "esp_heap_caps.h"
#include <Arduino.h>
#include <cstdio>
#include <dirent.h>
#include <string>
#include <sys/stat.h>

#define MP3_I2S_PORT  I2S_NUM_0
#define PA_PIN        46

struct Mp3Player::Impl {
    FILE* file = nullptr;
};

namespace {
constexpr const char* kSdMountPoint = "/sdcard";

/**功能: 将业务层传入的逻辑路径转换为 SD 卡挂载后的真实文件系统路径 */
std::string resolveMountedPath(const char* path) {
    // 空路径直接指向挂载根目录，方便上层用统一入口处理目录和文件。
    if (path == nullptr || path[0] == '\0') return kSdMountPoint;

    // 先放入固定挂载点，后续再拼接业务路径。
    std::string fullPath = kSdMountPoint;
    // 如果上传入的不是绝对路径，手动补一个 '/'，避免路径拼接错误。
    if (path[0] != '/') fullPath += '/';
    // 将业务路径接到挂载点后，得到真实可访问路径。
    fullPath += path;
    return fullPath;
}
}

/**功能: 构造 MP3 播放器实例并创建内部实现对象 */
Mp3Player::Mp3Player()
    : impl_(new Impl()) {
}

/**功能: 释放播放器持有的文件句柄、缓冲区和解码器状态 */
Mp3Player::~Mp3Player() {
    // 先处理内部实现对象，避免文件句柄泄漏。
    if (impl_ != nullptr) {
        // 如果析构时文件还开着，先关闭底层文件。
        if (impl_->file != nullptr) fclose(impl_->file);
        // 释放实现对象本身。
        delete impl_;
        impl_ = nullptr;
    }
    // 释放解码临时 PCM 缓冲，避免 PSRAM 泄漏。
    if (pcmTemp_ != nullptr) {
        heap_caps_free(pcmTemp_);
        pcmTemp_ = nullptr;
    }
    // 释放堆上的解码器状态对象，避免把 minimp3 状态残留到下一次生命周期。
    if (decoderState_ != nullptr) {
        heap_caps_free(decoderState_);
        decoderState_ = nullptr;
    }
    // 释放读取 MP3 压缩数据的输入缓冲。
    if (readBuf_ != nullptr) {
        heap_caps_free(readBuf_);
        readBuf_ = nullptr;
    }
    // 逐个释放双缓冲 PCM 输出区。
    for (int i = 0; i < 2; ++i) {
        if (pcmBuf_[i] != nullptr) {
            heap_caps_free(pcmBuf_[i]);
            pcmBuf_[i] = nullptr;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
//  begin() —— 分配双缓冲和信号量
// ═══════════════════════════════════════════════════════════════
/**功能: 初始化播放器运行期所需的双缓冲、读取缓冲和解码器状态 */
bool Mp3Player::begin() {
    Serial.println("[MP3] begin() start");
    // 依次创建两个 PCM 双缓冲，供解码任务和写出任务交替使用。
    for (int i = 0; i < 2; i++) {
        // PCM 输出缓冲使用 PSRAM，减轻片上 RAM 压力。
        pcmBuf_[i] = (int32_t*)heap_caps_malloc(kPcmBufBytes, MALLOC_CAP_SPIRAM);
        if (!pcmBuf_[i]) {
            Serial.println("[MP3] PCM buf alloc failed");
            return false;
        }
        // bufReady_ 表示“该缓冲已经装好 PCM，可以给 writer 消费”。
        bufReady_[i] = xSemaphoreCreateBinary();
        // bufFree_ 表示“该缓冲已经被 writer 用完，可以给 decoder 继续写”。
        bufFree_[i]  = xSemaphoreCreateBinary();
        // 初始化时两个缓冲都尚未装数据，因此先标记为空闲。
        xSemaphoreGive(bufFree_[i]);  // 初始状态：两个缓冲区都空闲
    }

    // 读取缓冲保存从 SD 文件中读出的压缩 MP3 字节流。
    readBuf_ = (uint8_t*)heap_caps_malloc(kReadBufSize, MALLOC_CAP_SPIRAM);
    if (!readBuf_) {
        Serial.println("[MP3] Read buf alloc failed");
        return false;
    }

    // pcmTemp_ 保存 minimp3 解出的 int16 PCM，避免把大数组压在任务栈上。
    pcmTemp_ = (int16_t*)heap_caps_malloc(sizeof(int16_t) * MINIMP3_MAX_SAMPLES_PER_FRAME,
                                          MALLOC_CAP_SPIRAM);
    if (!pcmTemp_) {
        Serial.println("[MP3] PCM temp buf alloc failed");
        return false;
    }

    // decoderState_ 持有 minimp3 的长生命周期解码状态，避免任务栈上放大对象。
    decoderState_ = (mp3dec_t*)heap_caps_malloc(sizeof(mp3dec_t), MALLOC_CAP_8BIT);
    if (!decoderState_) {
        Serial.println("[MP3] Decoder state alloc failed");
        return false;
    }

    Serial.println("[MP3] Player ready");
    return true;
}

// ═══════════════════════════════════════════════════════════════
//  play() —— 启动双任务播放 MP3
// ═══════════════════════════════════════════════════════════════
/**功能: 打开指定 MP3 文件并启动解码任务和写出任务 */
bool Mp3Player::play(const char* path) {
    Serial.printf("[MP3] play() request: %s\n", path != nullptr ? path : "(null)");
    // 播放器处于忙状态时拒绝重入，避免双任务状态机被覆盖。
    if (playing_) return false;

    // 将业务路径映射到真实挂载路径，确保 POSIX 文件接口能正确打开。
    const std::string fullPath = resolveMountedPath(path);
    // 以二进制只读方式打开 MP3 文件。
    impl_->file = fopen(fullPath.c_str(), "rb");
    if (impl_->file == nullptr) {
        Serial.printf("[MP3] Cannot open: %s\n", path);
        return false;
    }

    // 跳到文件末尾获取总大小，便于输出诊断日志。
    fseek(impl_->file, 0, SEEK_END);
    // 记录文件总字节数，帮助判断大文件播放时的调试现场。
    const long fileSize = ftell(impl_->file);
    // 回到文件起始位置，后续正式开始顺序解码。
    rewind(impl_->file);

    Serial.printf("[MP3] Playing: %s (%ld bytes)\n", path, fileSize);

    // 清除停止标记，让两个任务进入正常工作流程。
    stopRequested_ = false;
    // 新曲目启动时总是从非暂停状态开始。
    paused_ = false;
    // 标记播放器进入忙状态，供外层轮询判断。
    playing_ = true;
    // 新曲目启动时先清空“自然播放完成”状态。
    trackFinished_ = false;
    // 输入缓冲初始时没有有效数据。
    readBufValid_ = 0;
    // 输入缓冲读指针从 0 开始。
    readBufPos_ = 0;
    // 两个输出缓冲还未写入有效 PCM。
    pcmBufLen_[0] = 0;
    pcmBufLen_[1] = 0;
    consumedCompressedBytes_ = 0;
    trackMetrics_ = {};
    trackMetrics_.fileSizeBytes = static_cast<size_t>(fileSize);

    // 重置信号量状态
    for (int i = 0; i < 2; i++) {
        // 清掉可能残留的 ready 信号，避免旧播放状态串到新曲目。
        xSemaphoreTake(bufReady_[i], 0);
        // 清掉可能残留的 free 信号，统一重新建立初始关系。
        xSemaphoreTake(bufFree_[i], 0);
        // 重新标记为可写，表示 decoder 可以从任意一个缓冲开始生产。
        xSemaphoreGive(bufFree_[i]);
    }

    // 创建解码任务。栈给到 32KB，是为了覆盖 minimp3 内部 scratch 工作区。
    const BaseType_t decoderCreated = xTaskCreatePinnedToCore(
        decoderTaskFunc, "Mp3Decoder",
        32768, this, 5, &decoderTask_, 1);
    if (decoderCreated != pdPASS) {
        Serial.println("[MP3] Failed to create decoder task");
        // 解码任务创建失败时，立刻关闭文件，避免文件句柄悬挂。
        fclose(impl_->file);
        impl_->file = nullptr;
        // 回滚播放状态，通知上层这次启动失败。
        playing_ = false;
        trackFinished_ = false;
        return false;
    }

    // 创建写出任务，将 decoder 产出的 PCM 送到 I2S。
    const BaseType_t writerCreated = xTaskCreatePinnedToCore(
        writerTaskFunc, "Mp3Writer",
        4096, this, 6, &writerTask_, 1);
    if (writerCreated != pdPASS) {
        Serial.println("[MP3] Failed to create writer task");
        // writer 创建失败后，要求 decoder 也同步退出，避免后台任务悬挂。
        stopRequested_ = true;
        // 主动释放所有可能等待的信号量，确保 decoder 不会永远卡住。
        xSemaphoreGive(bufFree_[0]);
        xSemaphoreGive(bufFree_[1]);
        xSemaphoreGive(bufReady_[0]);
        xSemaphoreGive(bufReady_[1]);
        // 等待 decoder 观察到 stopRequested_ 后退出。
        for (int i = 0; i < 50 && playing_; ++i) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        // 若文件还没被 decoder 关闭，这里兜底关闭。
        if (impl_->file != nullptr) {
            fclose(impl_->file);
            impl_->file = nullptr;
        }
        // 回滚整体播放状态。
        playing_ = false;
        trackFinished_ = false;
        return false;
    }
    Serial.printf("[MP3] play() started tasks: %s\n", path);
    return true;
}

/**功能: 请求当前曲目进入暂停状态，保留当前位置等待恢复 */
bool Mp3Player::pause() {
    if (!playing_ || paused_) {
        Serial.printf("[MP3] pause() reject: playing=%s paused=%s\n",
                      playing_ ? "true" : "false",
                      paused_ ? "true" : "false");
        return false;
    }
    paused_ = true;
    Serial.println("[MP3] pause() accepted");
    return true;
}

/**功能: 请求当前曲目从暂停状态恢复继续播放 */
bool Mp3Player::resume() {
    if (!playing_ || !paused_) {
        Serial.printf("[MP3] resume() reject: playing=%s paused=%s\n",
                      playing_ ? "true" : "false",
                      paused_ ? "true" : "false");
        return false;
    }
    paused_ = false;
    Serial.println("[MP3] resume() accepted");
    return true;
}

// ═══════════════════════════════════════════════════════════════
//  stop() —— 停止播放，等待任务退出
// ═══════════════════════════════════════════════════════════════
/**功能: 请求当前曲目停止播放并等待后台任务完成退出 */
void Mp3Player::stop() {
    Serial.println("[MP3] stop() request");
    // 已经停止时直接返回，避免重复发停止信号。
    if (!playing_) return;
    // 设置停止标记，让 decoder 和 writer 在下一轮循环感知退出。
    stopRequested_ = true;
    // 停止时同步清空暂停标记，确保任务不会继续停在 pause 等待分支。
    paused_ = false;

    // 解锁可能阻塞在信号量上的任务
    // 释放 free 信号量，避免 decoder 卡在等待可写缓冲。
    xSemaphoreGive(bufFree_[0]);
    xSemaphoreGive(bufFree_[1]);
    // 释放 ready 信号量，避免 writer 卡在等待可读缓冲。
    xSemaphoreGive(bufReady_[0]);
    xSemaphoreGive(bufReady_[1]);

    // 最长等待约 2 秒，给 writer 收尾、关 PA、恢复 I2S 时钟。
    for (int i = 0; i < 200 && playing_; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // 任务结束后清空任务句柄，避免上层误判后台任务仍在运行。
    decoderTask_ = nullptr;
    writerTask_  = nullptr;
    Serial.println("[MP3] stop() completed");
}

// ═══════════════════════════════════════════════════════════════
//  scanMp3Files() —— 扫描目录下的 .mp3 文件
// ═══════════════════════════════════════════════════════════════
/**功能: 枚举指定目录下一层的 MP3 文件并返回数量 */
int Mp3Player::scanMp3Files(const char* dir, String names[], int maxCount) {
    // 将逻辑目录映射到已挂载 SD 卡上的真实目录路径。
    const std::string fullDir = resolveMountedPath(dir);
    Serial.printf("[MP3] scanMp3Files() open dir: %s\n", fullDir.c_str());
    // 打开目录句柄，后续逐项枚举目录项。
    DIR* root = opendir(fullDir.c_str());
    if (root == nullptr) {
        Serial.printf("[MP3] scanMp3Files() failed to open dir: %s\n", fullDir.c_str());
        return 0;
    }

    // count 记录已收集到的 MP3 数量。
    int count = 0;
    // entry 保存 readdir() 每次返回的目录项指针。
    dirent* entry = nullptr;
    // 只做单层目录扫描，不递归子目录，直到达到容量上限。
    while ((entry = readdir(root)) != nullptr && count < maxCount) {
        // 跳过当前目录和父目录两个固定项。
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 拼出当前目录项的完整真实路径，用于 stat 判定文件类型。
        std::string entryPath = fullDir + "/" + entry->d_name;
        // st 用来承接 stat 结果，判断当前项是普通文件还是子目录。
        struct stat st = {};
        // stat 失败或当前项是目录时直接跳过，本测试只关心普通文件。
        if (stat(entryPath.c_str(), &st) != 0 || S_ISDIR(st.st_mode)) continue;

        // 目录项文件名转为 Arduino String，便于后续做后缀判断。
        String name = entry->d_name;
        // 仅收集大小写两种 MP3 后缀，避免非 MP3 文件混入播放列表。
        if (name.endsWith(".mp3") || name.endsWith(".MP3")) {
            // 保存给上层的是业务路径，而不是 /sdcard 绝对挂载路径。
            names[count] = String(dir) + "/" + name;
            Serial.printf("[MP3] Found: %s\n", names[count].c_str());
            count++;
        }
    }

    // 枚举完成后关闭目录句柄。
    closedir(root);
    Serial.printf("[MP3] scanMp3Files() total: %d\n", count);
    return count;
}

// ═══════════════════════════════════════════════════════════════
//  PA 控制 —— 复用 AudioCodec 的同一套时序
// ═══════════════════════════════════════════════════════════════
/**功能: 打开功放并恢复可听音量，准备向喇叭输出声音 */
void Mp3Player::enablePA() {
    // 先清空 I2S DMA 中可能残留的旧数据，避免开声瞬间输出脏音频。
    i2s_zero_dma_buffer(MP3_I2S_PORT);
    // 将 DAC 音量设为 0dB，对应正常播放增益。
    es8311SetDacVolume(0xBF);       // 0 dB
    // 打开外部功放，使能扬声器输出。
    digitalWrite(PA_PIN, HIGH);
}

/**功能: 关闭功放并清空 I2S 输出状态，结束当前播放会话 */
void Mp3Player::disablePA() {
    // 稍等 DMA 将末尾数据送完，减少截断和爆音。
    vTaskDelay(pdMS_TO_TICKS(100)); // 等 DMA 播完
    // 先关功放，防止后续时钟恢复时把残留信号送到喇叭。
    digitalWrite(PA_PIN, LOW);
    // 再将 DAC 音量拉到静音。
    es8311SetDacVolume(0x00);
    // 最后清空 I2S DMA，避免下一次播放携带旧数据。
    i2s_zero_dma_buffer(MP3_I2S_PORT);
}

// ═══════════════════════════════════════════════════════════════
//  refillReadBuf() —— 从 SD 补充 MP3 压缩数据
// ═══════════════════════════════════════════════════════════════
//  将未消耗数据 memmove 到缓冲区头部，再从文件补充
/**功能: 将未消费的 MP3 字节前移并继续从文件尾部补充压缩数据 */
int Mp3Player::refillReadBuf() {
    // remaining 表示缓冲里还没被 decoder 消费掉的有效字节数。
    int remaining = readBufValid_ - readBufPos_;
    // 若缓冲中还有残留字节且读指针不在起点，则把残留数据搬到头部连续存放。
    if (remaining > 0 && readBufPos_ > 0) {
        memmove(readBuf_, readBuf_ + readBufPos_, remaining);
    }
    // 更新当前有效数据长度为残留字节数。
    readBufValid_ = remaining;
    // 读指针重置到头部，因为剩余数据已被搬到起始位置。
    readBufPos_ = 0;

    // 计算缓冲尾部还有多少空间可继续从文件补数据。
    int space = kReadBufSize - readBufValid_;
    // 只有存在空闲空间、文件有效且未到 EOF 时，才继续读文件。
    if (space > 0 && impl_->file != nullptr && !feof(impl_->file)) {
        // 新读出的数据直接接在当前有效数据尾部。
        const size_t bytesRead = fread(readBuf_ + readBufValid_, 1, space, impl_->file);
        // 若本轮读到字节，则扩展有效长度。
        if (bytesRead > 0) readBufValid_ += static_cast<int>(bytesRead);
        // 返回本轮新增的字节数，供上层判断是否继续尝试解码。
        return static_cast<int>(bytesRead);
    }
    // 没读到新数据时返回 0，表示可能已到文件尾。
    return 0;
}

// ═══════════════════════════════════════════════════════════════
//  convertToI2s() —— PCM int16 → I2S int32 格式转换
// ═══════════════════════════════════════════════════════════════
//  I2S 格式：32-bit stereo，16-bit PCM 左对齐 (sample << 16)
//  Mono MP3 → 复制到左右声道；Stereo → 直接映射
/**功能: 将 minimp3 解出的 int16 PCM 转成 ES8311/I2S 可直接发送的 32bit 立体声格式 */
void Mp3Player::convertToI2s(const int16_t* pcm, int samples, int channels,
                              int32_t* out, int* outBytes) {
    // idx 表示已写入输出缓冲的 int32 样本个数。
    int idx = 0;
    // 单声道 MP3 需要复制到左右两个声道，保证喇叭左右声道都有信号。
    if (channels == 1) {
        for (int i = 0; i < samples; i++) {
            // 将 16bit PCM 左移到 32bit slot 高位，匹配当前 I2S 格式。
            int32_t s = ((int32_t)pcm[i]) << 16;
            out[idx++] = s;  // L
            out[idx++] = s;  // R
        }
    } else {
        // 双声道 MP3 已经是 LRLR 排列，只需逐样本左移并顺序写出。
        for (int i = 0; i < samples * 2; i++) {
            out[idx++] = ((int32_t)pcm[i]) << 16;
        }
    }
    // 输出字节数供 writer 精确写 I2S。
    *outBytes = idx * (int)sizeof(int32_t);
}

// ═══════════════════════════════════════════════════════════════
//  decoderTaskFunc() —— 解码器任务 (producer)
// ═══════════════════════════════════════════════════════════════
//  读 SD → minimp3 解码 → 填充双缓冲中的一个 → 信号通知 writer
/**功能: 持续读取 MP3 压缩数据并解码成 PCM 放入双缓冲 */
void Mp3Player::decoderTaskFunc(void* arg) {
    // arg 由 xTaskCreatePinnedToCore 传入，固定就是当前播放器实例。
    Mp3Player* self = static_cast<Mp3Player*>(arg);
    Serial.println("[MP3] Decoder task start");

    // 解码临时缓冲为空时无法继续，直接请求整个播放链路停止。
    if (self->pcmTemp_ == nullptr) {
        Serial.println("[MP3] Decoder task abort: pcmTemp_ is null");
        self->stopRequested_ = true;
        self->playing_ = false;
        self->trackFinished_ = false;
        vTaskDelete(nullptr);
        return;
    }

    // 解码器状态对象为空时，说明 begin() 未正确完成或内存已损坏。
    if (self->decoderState_ == nullptr) {
        Serial.println("[MP3] Decoder task abort: decoderState_ is null");
        self->stopRequested_ = true;
        self->playing_ = false;
        self->trackFinished_ = false;
        vTaskDelete(nullptr);
        return;
    }

    // 每次新曲目开始前都重置 minimp3 内部状态，避免上一首残留。
    mp3dec_init(self->decoderState_);

    // info 保存每一帧解码后的采样率、声道数、帧字节数等元数据。
    mp3dec_frame_info_t info;

    // firstFrame 用于在拿到首个有效音频帧后再配置 I2S 采样率。
    bool firstFrame = true;
    // activeBuf 表示当前 decoder 正准备写入哪一个双缓冲槽。
    int  activeBuf = 0;
    // errorCount 统计连续找不到有效帧的次数，避免损坏文件死循环。
    int  errorCount = 0;

    // 初始填充读取缓冲区
    self->refillReadBuf();

    while (!self->stopRequested_) {
        // 暂停时不再推进文件读取和解码，保留当前位置等待恢复。
        if (self->paused_) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        // available 表示当前输入缓冲里还能给解码器消费的字节数。
        int available = self->readBufValid_ - self->readBufPos_;

        // 数据不足时尝试补充
        if (available < 512) {
            // 继续从文件补压缩数据，尽量凑够一帧需要的字节。
            int added = self->refillReadBuf();
            available = self->readBufValid_ - self->readBufPos_;
            // 没有可用字节且文件也读不出新数据时，说明已经到 EOF。
            if (available == 0 && added == 0) break;  // EOF
        }

        // 等待缓冲区可写
        if (xSemaphoreTake(self->bufFree_[activeBuf],
                           pdMS_TO_TICKS(100)) != pdTRUE) {
            // writer 还没消费完上一个缓冲时，稍后重试并顺带检查 stop 标记。
            continue;  // 超时重试（检查 stopRequested_）
        }
        // 如果等待期间收到了 stop 请求，直接结束本轮。
        if (self->stopRequested_) break;

        // 解码一帧
        int samples = mp3dec_decode_frame(self->decoderState_,
            self->readBuf_ + self->readBufPos_, available,
            self->pcmTemp_, &info);

        // frame_bytes > 0 说明本轮至少消费掉了一帧压缩数据。
        if (info.frame_bytes > 0) {
            // 推进输入缓冲读指针，下一轮从剩余压缩数据继续解码。
            self->readBufPos_ += info.frame_bytes;
            // 记录累计已消费的压缩数据量，供上层估算播放进度。
            self->consumedCompressedBytes_ += static_cast<uint32_t>(info.frame_bytes);
            // 找到有效帧后清零错误计数。
            errorCount = 0;
        } else if (info.frame_bytes == 0) {
            // 无法识别帧，可能是 ID3 tag 或损坏数据
            // 跳过 1 字节继续搜索同步头
            if (available > 0) {
                // 只前进 1 字节，做最保守的同步头重搜。
                self->readBufPos_++;
                // 无法识别帧时也推进整体压缩流消费计数，避免进度卡住。
                self->consumedCompressedBytes_ += 1;
                // 记录连续失败次数，防止卡死在坏文件里。
                errorCount++;
                if (errorCount > 1024) {
                    Serial.println("[MP3] Too many errors, abort");
                    break;
                }
            }
            // 当前未产出有效 PCM，立即归还该缓冲给下一轮继续写。
            xSemaphoreGive(self->bufFree_[activeBuf]);  // 归还缓冲区
            continue;
        }

        // 有些帧虽然被解析掉了，但可能没有实际 PCM 输出，例如纯元数据帧。
        if (samples == 0) {
            // 帧被消耗但无输出（如 ID3 帧），继续
            xSemaphoreGive(self->bufFree_[activeBuf]);
            continue;
        }

        // 首帧：配置 I2S 采样率
        if (firstFrame && info.hz > 0) {
            // 只在第一帧拿到真实采样率后更新 I2S，避免提前写死错误参数。
            firstFrame = false;
            Serial.printf("[MP3] %d Hz, %d ch, %d kbps\n",
                          info.hz, info.channels, info.bitrate_kbps);
            // 记录当前轨道的技术参数，供控制器和 UI 查询。
            self->trackMetrics_.sampleRate = static_cast<uint32_t>(info.hz);
            self->trackMetrics_.channels = static_cast<uint16_t>(info.channels);
            self->trackMetrics_.bitrateKbps = static_cast<uint16_t>(info.bitrate_kbps);
            self->trackMetrics_.metadataReady = true;
            if (info.bitrate_kbps > 0 && self->trackMetrics_.fileSizeBytes > 0) {
                self->trackMetrics_.durationKnown = true;
                self->trackMetrics_.durationMs =
                    static_cast<uint32_t>((self->trackMetrics_.fileSizeBytes * 8ULL) / info.bitrate_kbps);
            }
            // 将 I2S 时钟切到当前 MP3 实际采样率，以正确播放音高和速度。
            i2s_set_clk(MP3_I2S_PORT, info.hz,
                        I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);
        }

        // bitrate 已知时，用已消费压缩流大小估算当前播放进度。
        if (self->trackMetrics_.bitrateKbps > 0) {
            self->trackMetrics_.progressMs =
                static_cast<uint32_t>((static_cast<uint64_t>(self->consumedCompressedBytes_) * 8ULL) /
                                      self->trackMetrics_.bitrateKbps);
        }

        // PCM → I2S 格式转换，写入双缓冲
        self->convertToI2s(self->pcmTemp_, samples, info.channels,
                           self->pcmBuf_[activeBuf],
                           &self->pcmBufLen_[activeBuf]);

        // 通知 writer 缓冲区已就绪
        // 当前槽位已装好可播放 PCM，通知 writer 可以消费。
        xSemaphoreGive(self->bufReady_[activeBuf]);
        // 生产者切到另一个槽位，形成 ping-pong 双缓冲。
        activeBuf ^= 1;  // 切换到另一个缓冲区
    }

    // 发送 EOF sentinel
    for (int i = 0; i < 2; i++) {
        // 等待每个槽位真正空闲后再写 EOF，避免覆盖 writer 尚未消费的数据。
        xSemaphoreTake(self->bufFree_[i], pdMS_TO_TICKS(200));
        // 用 -1 作为 EOF 哨兵值，让 writer 知道播放自然结束。
        self->pcmBufLen_[i] = -1;
        // 通知 writer 读取该槽位，完成收尾退出。
        xSemaphoreGive(self->bufReady_[i]);
    }

    // 文件由 decoder 负责关闭，表示不再产生新的压缩数据输入。
    if (self->impl_->file != nullptr) {
        fclose(self->impl_->file);
        self->impl_->file = nullptr;
    }
    Serial.println("[MP3] Decoder task done");
    // 当前任务职责结束，主动删除自身。
    vTaskDelete(nullptr);
}

// ═══════════════════════════════════════════════════════════════
//  writerTaskFunc() —— 写入任务 (consumer)
// ═══════════════════════════════════════════════════════════════
//  从双缓冲读取 PCM → i2s_write() → ES8311 → PA → Speaker
/**功能: 从双缓冲中取出 PCM 并持续写入 I2S 直到曲目结束或被停止 */
void Mp3Player::writerTaskFunc(void* arg) {
    // 将任务参数还原成播放器实例，访问共享缓冲和状态位。
    Mp3Player* self = static_cast<Mp3Player*>(arg);
    Serial.println("[MP3] Writer task start");

    // PA 启动序列
    self->enablePA();

    // consumerBuf 表示当前 writer 正在消费哪一个 PCM 槽位。
    int consumerBuf = 0;

    while (!self->stopRequested_) {
        // 暂停时不再向 I2S 写入新数据，保留当前缓冲和输出位置。
        if (self->paused_) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        // 等待 decoder 填充缓冲区
        if (xSemaphoreTake(self->bufReady_[consumerBuf],
                           pdMS_TO_TICKS(100)) != pdTRUE) {
            // decoder 尚未产出 PCM 时循环等待，同时允许外部 stop 打断。
            continue;  // 超时重试
        }
        // 如果等待期间收到了 stop 请求，则直接退出写出循环。
        if (self->stopRequested_) break;

        // 读取当前槽位的有效字节数。
        int len = self->pcmBufLen_[consumerBuf];
        if (len <= 0) {
            Serial.println("[MP3] Writer received EOF sentinel");
            // 收到 EOF 哨兵后说明 decoder 已经没有更多 PCM 数据可产出。
            break;  // EOF sentinel
        }

        // 分块写入 I2S
        // data 指向当前双缓冲槽位的 PCM 起始地址。
        uint8_t* data = (uint8_t*)self->pcmBuf_[consumerBuf];
        // written 记录当前槽位已送入 I2S 的字节数。
        int written = 0;
        while (written < len && !self->stopRequested_) {
            // bytesOut 记录本轮 i2s_write 实际成功写出的字节数。
            size_t bytesOut = 0;
            // 每次最多写 512 字节，降低单次阻塞时间，便于响应 stop。
            int want = len - written;
            if (want > 512) want = 512;
            i2s_write(MP3_I2S_PORT, data + written, want,
                      &bytesOut, pdMS_TO_TICKS(50));
            // 前移已写字节计数，直到整个槽位全部送完。
            written += (int)bytesOut;
        }

        // 归还缓冲区给 decoder
        // 当前槽位已消费完成，标记为可重新写入。
        xSemaphoreGive(self->bufFree_[consumerBuf]);
        // 切到下一个槽位继续消费，形成 ping-pong。
        consumerBuf ^= 1;
    }

    // PA 关闭序列
    self->disablePA();

    // 恢复原始采样率 (16kHz for record-playback)
    // 收尾时恢复系统默认 I2S 时钟，避免影响项目内其他音频模块。
    i2s_set_clk(MP3_I2S_PORT, 16000,
                I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);

    // writer 退出意味着当前曲目不再有音频输出，因此清除播放中状态。
    self->playing_ = false;
    self->paused_ = false;
    // 若不是被 stop 主动打断，而是正常跑到 EOF，则标记曲目自然结束。
    self->trackFinished_ = !self->stopRequested_;
    Serial.printf("[MP3] Writer task done: trackFinished=%s\n",
                  self->trackFinished_ ? "true" : "false");
    // 写出任务职责结束，主动删除自身。
    vTaskDelete(nullptr);
}
