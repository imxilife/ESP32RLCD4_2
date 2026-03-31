#pragma once

#include <Arduino.h>
#include <device/sdcard/SDCard.h>
#include <media/audio/AudioCodec.h>
#include <media/mp3_player/Mp3Player.h>

/**功能: 管理 MP3 歌单、播放状态机和跨状态共享的播放信息 */
class Mp3Controller {
public:
    static constexpr int kMaxPlaylist = 32;

    enum class State : uint8_t {
        UNINITIALIZED = 0,
        IDLE,
        PLAYING,
        PAUSED,
        STOPPED,
        ERROR,
    };

    struct TrackInfo {
        String path;
        String title;
        size_t sizeBytes = 0;
        uint32_t durationMs = 0;
        bool durationKnown = false;
    };

    struct Snapshot {
        bool initialized = false;
        bool hasPlaylist = false;
        int currentIndex = -1;
        int selectedIndex = 0;
        int playlistCount = 0;
        State state = State::UNINITIALIZED;
        uint32_t progressMs = 0;
        bool progressKnown = false;
        TrackInfo track = {};
        String errorText;
    };

    /**功能: 获取全局唯一 MP3 控制器实例 */
    static Mp3Controller& instance();

    /**功能: 根据状态机规则完成底层初始化和歌单扫描 */
    bool beginIfNeeded();

    /**功能: 清空错误状态并回到未初始化状态，供后续重新初始化 */
    void reset();

    /**功能: 驱动控制器状态机，处理自然播放结束和异常状态 */
    void tick();

    /**功能: 播放指定索引的曲目 */
    bool play(int index);

    /**功能: 按状态机规则暂停当前曲目 */
    bool pause();

    /**功能: 按状态机规则恢复当前曲目 */
    bool resume();

    /**功能: 在播放与暂停之间切换，必要时按当前选中索引启动播放 */
    bool togglePause();

    /**功能: 停止当前曲目并进入 STOPPED 状态 */
    bool stop();

    /**功能: 播放下一首曲目 */
    bool next();

    /**功能: 将 UI 选中项移动到下一首，不直接播放 */
    void selectNext();

    /**功能: 更新当前 UI 选中项 */
    void setSelectedIndex(int index);

    /**功能: 获取当前 UI 选中项 */
    int selectedIndex() const { return selectedIndex_; }

    /**功能: 获取歌单总数 */
    int playlistCount() const { return playlistCount_; }

    /**功能: 获取指定索引的曲目信息 */
    const TrackInfo& trackAt(int index) const;

    /**功能: 获取当前播放状态快照供 UI 渲染 */
    Snapshot snapshot() const;

    /**功能: 获取当前状态机状态 */
    State state() const { return state_; }

private:
    Mp3Controller() = default;

    /**功能: 根据播放状态机定义切换状态并输出日志 */
    bool transitionTo(State nextState, const char* action, bool allowSelf = false);

    /**功能: 将字符串路径转换成短标题 */
    String titleFromPath(const String& path) const;

    /**功能: 读取文件大小并填充曲目信息 */
    bool fillTrackInfo(TrackInfo& track);

    /**功能: 扫描 /music 目录并按文件名稳定排序 */
    bool scanPlaylist();

    /**功能: 对歌单做升序稳定排序 */
    void sortPlaylist();

    /**功能: 从播放器元数据同步当前曲目技术信息和时长估算 */
    void syncTrackMetrics();

    /**功能: 统一记录失败信息并切换到 ERROR 状态 */
    void setError(const char* action, const char* reason);

    /**功能: 保证索引落在歌单范围内 */
    int normalizeIndex(int index) const;

    /**
     * 功能: 记录 Mp3Controller 内部播放状态机
     *
     *                 +------------------+
     *                 |  UNINITIALIZED   |
     *                 +------------------+
     *                    | init ok
     *                    v
     *                 +------------------+
     *                 |       IDLE       |
     *                 +------------------+
     *                    | play(index)
     *                    v
     *                 +------------------+
     *        pause     |     PLAYING      | stop/exit
     *     +----------> +------------------+ ----------+
     *     |                 | next/prev                |
     *     |                 | play(other)              |
     *     |                 v                          |
     *     |           +------------------+             |
     *     |  resume   |      PAUSED      | stop/exit   |
     *     +-----------+------------------+ -----------+
     *                    | resume/play
     *                    v
     *                 +------------------+
     *                 |     PLAYING      |
     *                 +------------------+
     *
     *                 +------------------+
     *                 |     STOPPED      |
     *                 +------------------+
     *                    | play(index)
     *                    v
     *                 +------------------+
     *                 |     PLAYING      |
     *                 +------------------+
     *
     * 任何初始化失败/播放启动失败/底层异常
     *            -> +------------------+
     *               |      ERROR       |
     *               +------------------+
     *                    | reset/reinit
     *                    v
     *               +------------------+
     *               |  UNINITIALIZED   |
     *               +------------------+
     */
    State state_ = State::UNINITIALIZED;
    SDCard sd_;
    AudioCodec audio_;
    Mp3Player player_;
    TrackInfo playlist_[kMaxPlaylist];
    int playlistCount_ = 0;
    int currentIndex_ = -1;
    int selectedIndex_ = 0;
    bool audioReady_ = false;
    bool sdReady_ = false;
    bool playerReady_ = false;
    String errorText_;
};
