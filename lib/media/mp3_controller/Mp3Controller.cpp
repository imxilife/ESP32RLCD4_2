#include "Mp3Controller.h"

#include <algorithm>
#include <string>
#include <sys/stat.h>

namespace {
const Mp3Controller::TrackInfo kEmptyTrack = {};

std::string resolveMountedPath(const char* path) {
    if (path == nullptr || path[0] == '\0') return "/sdcard";
    std::string fullPath = "/sdcard";
    if (path[0] != '/') fullPath += '/';
    fullPath += path;
    return fullPath;
}

const char* stateName(Mp3Controller::State state) {
    switch (state) {
    case Mp3Controller::State::UNINITIALIZED: return "UNINITIALIZED";
    case Mp3Controller::State::IDLE: return "IDLE";
    case Mp3Controller::State::PLAYING: return "PLAYING";
    case Mp3Controller::State::PAUSED: return "PAUSED";
    case Mp3Controller::State::STOPPED: return "STOPPED";
    case Mp3Controller::State::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}
}

/**功能: 获取全局唯一 MP3 控制器实例 */
Mp3Controller& Mp3Controller::instance() {
    static Mp3Controller controller;
    return controller;
}

/**功能: 根据状态机规则完成底层初始化和歌单扫描 */
bool Mp3Controller::beginIfNeeded() {
    if (state_ != State::UNINITIALIZED && state_ != State::ERROR) return state_ != State::ERROR;

    if (state_ == State::ERROR) {
        Serial.println("[MP3 CTRL] beginIfNeeded() reinit from ERROR");
        transitionTo(State::UNINITIALIZED, "reinit from error", true);
    } else {
        Serial.println("[MP3 CTRL] beginIfNeeded() start");
    }

    errorText_ = "";

    if (!audioReady_) {
        Serial.println("[MP3 CTRL] init audio codec");
        audioReady_ = audio_.begin(16000);
        if (!audioReady_) {
            setError("begin", "audio codec init failed");
            return false;
        }
    }

    if (!sdReady_) {
        Serial.println("[MP3 CTRL] init sd card");
        sdReady_ = sd_.begin();
        if (!sdReady_) {
            setError("begin", "sd card init failed");
            return false;
        }
    }

    if (!playerReady_) {
        Serial.println("[MP3 CTRL] init mp3 player");
        playerReady_ = player_.begin();
        if (!playerReady_) {
            setError("begin", "mp3 player init failed");
            return false;
        }
    }

    if (!scanPlaylist()) return false;
    return transitionTo(State::IDLE, "init ok");
}

/**功能: 清空错误状态并回到未初始化状态，供后续重新初始化 */
void Mp3Controller::reset() {
    Serial.printf("[MP3 CTRL] reset from %s\n", stateName(state_));
    if (player_.isPlaying() || player_.isPaused()) {
        player_.stop();
    }
    state_ = State::UNINITIALIZED;
    currentIndex_ = -1;
    selectedIndex_ = 0;
    errorText_ = "";
    playlistCount_ = 0;
}

/**功能: 驱动控制器状态机，处理自然播放结束和异常状态 */
void Mp3Controller::tick() {
    if (state_ == State::UNINITIALIZED || state_ == State::ERROR) return;

    if (player_.hasTrackFinished()) {
        Serial.printf("[MP3 CTRL] natural finish on index=%d\n", currentIndex_);
        if (playlistCount_ <= 0) {
            transitionTo(State::STOPPED, "track finished with empty playlist");
            return;
        }

        const int nextIndex = normalizeIndex(currentIndex_ + 1);
        if (!play(nextIndex)) {
            setError("auto next", "failed to start next track");
        }
        return;
    }

    if (state_ == State::PLAYING || state_ == State::PAUSED) {
        syncTrackMetrics();
    }
}

/**功能: 播放指定索引的曲目 */
bool Mp3Controller::play(int index) {
    if (!beginIfNeeded()) return false;
    if (playlistCount_ <= 0) {
        setError("play", "playlist is empty");
        return false;
    }

    const int normalized = normalizeIndex(index);
    if (normalized < 0 || normalized >= playlistCount_) {
        Serial.printf("[MP3 CTRL] reject play: invalid index=%d\n", index);
        return false;
    }

    if (!(state_ == State::IDLE || state_ == State::STOPPED || state_ == State::PAUSED || state_ == State::PLAYING)) {
        Serial.printf("[MP3 CTRL] reject play: state=%s index=%d\n", stateName(state_), normalized);
        return false;
    }

    if (player_.isPlaying() || player_.isPaused()) {
        player_.stop();
    }

    selectedIndex_ = normalized;
    currentIndex_ = normalized;
    errorText_ = "";

    Serial.printf("[MP3 CTRL] play index=%d title=%s\n",
                  currentIndex_, playlist_[currentIndex_].title.c_str());
    if (!player_.play(playlist_[currentIndex_].path.c_str())) {
        setError("play", "player start failed");
        return false;
    }

    syncTrackMetrics();
    return transitionTo(State::PLAYING, "play", true);
}

/**功能: 按状态机规则暂停当前曲目 */
bool Mp3Controller::pause() {
    if (state_ == State::PAUSED) return transitionTo(State::PAUSED, "pause noop", true);
    if (state_ != State::PLAYING) {
        Serial.printf("[MP3 CTRL] reject pause: state=%s\n", stateName(state_));
        return false;
    }
    if (!player_.pause()) {
        setError("pause", "player pause failed");
        return false;
    }
    return transitionTo(State::PAUSED, "pause");
}

/**功能: 按状态机规则恢复当前曲目 */
bool Mp3Controller::resume() {
    if (state_ != State::PAUSED) {
        Serial.printf("[MP3 CTRL] reject resume: state=%s\n", stateName(state_));
        return false;
    }
    if (!player_.resume()) {
        setError("resume", "player resume failed");
        return false;
    }
    return transitionTo(State::PLAYING, "resume");
}

/**功能: 在播放与暂停之间切换，必要时按当前选中索引启动播放 */
bool Mp3Controller::togglePause() {
    switch (state_) {
    case State::PLAYING:
        return pause();
    case State::PAUSED:
        return resume();
    case State::IDLE:
    case State::STOPPED:
        return play(selectedIndex_);
    default:
        Serial.printf("[MP3 CTRL] reject togglePause: state=%s\n", stateName(state_));
        return false;
    }
}

/**功能: 停止当前曲目并进入 STOPPED 状态 */
bool Mp3Controller::stop() {
    if (state_ == State::STOPPED) return transitionTo(State::STOPPED, "stop noop", true);
    if (!(state_ == State::PLAYING || state_ == State::PAUSED)) {
        Serial.printf("[MP3 CTRL] reject stop: state=%s\n", stateName(state_));
        return false;
    }
    player_.stop();
    return transitionTo(State::STOPPED, "stop");
}

/**功能: 播放下一首曲目 */
bool Mp3Controller::next() {
    if (!beginIfNeeded()) return false;
    if (playlistCount_ <= 0) return false;
    const int nextIndex = normalizeIndex((currentIndex_ >= 0 ? currentIndex_ : selectedIndex_) + 1);
    return play(nextIndex);
}

/**功能: 将 UI 选中项移动到下一首，不直接播放 */
void Mp3Controller::selectNext() {
    if (playlistCount_ <= 0) return;
    selectedIndex_ = normalizeIndex(selectedIndex_ + 1);
    Serial.printf("[MP3 CTRL] selectNext -> %d\n", selectedIndex_);
}

/**功能: 更新当前 UI 选中项 */
void Mp3Controller::setSelectedIndex(int index) {
    if (playlistCount_ <= 0) {
        selectedIndex_ = 0;
        return;
    }
    selectedIndex_ = normalizeIndex(index);
}

/**功能: 获取指定索引的曲目信息 */
const Mp3Controller::TrackInfo& Mp3Controller::trackAt(int index) const {
    if (index < 0 || index >= playlistCount_) return kEmptyTrack;
    return playlist_[index];
}

/**功能: 获取当前播放状态快照供 UI 渲染 */
Mp3Controller::Snapshot Mp3Controller::snapshot() const {
    Snapshot snap;
    snap.initialized = state_ != State::UNINITIALIZED;
    snap.hasPlaylist = playlistCount_ > 0;
    snap.currentIndex = currentIndex_;
    snap.selectedIndex = selectedIndex_;
    snap.playlistCount = playlistCount_;
    snap.state = state_;
    snap.errorText = errorText_;

    if (currentIndex_ >= 0 && currentIndex_ < playlistCount_) {
        snap.track = playlist_[currentIndex_];
        snap.progressMs = player_.currentProgressMs();
        snap.progressKnown = snap.track.durationKnown && snap.progressMs <= snap.track.durationMs;
    } else if (selectedIndex_ >= 0 && selectedIndex_ < playlistCount_) {
        snap.track = playlist_[selectedIndex_];
    }
    return snap;
}

/**功能: 根据播放状态机定义切换状态并输出日志 */
bool Mp3Controller::transitionTo(State nextState, const char* action, bool allowSelf) {
    const bool same = state_ == nextState;
    if (same && !allowSelf) {
        Serial.printf("[MP3 CTRL] reject transition %s -> %s by %s: same state\n",
                      stateName(state_), stateName(nextState), action);
        return false;
    }

    bool allowed = false;
    switch (state_) {
    case State::UNINITIALIZED:
        allowed = (nextState == State::IDLE || nextState == State::ERROR);
        break;
    case State::IDLE:
        allowed = (nextState == State::PLAYING || nextState == State::ERROR);
        break;
    case State::PLAYING:
        allowed = (nextState == State::PLAYING || nextState == State::PAUSED ||
                   nextState == State::STOPPED || nextState == State::ERROR);
        break;
    case State::PAUSED:
        allowed = (nextState == State::PLAYING || nextState == State::STOPPED ||
                   nextState == State::PAUSED || nextState == State::ERROR);
        break;
    case State::STOPPED:
        allowed = (nextState == State::PLAYING || nextState == State::STOPPED ||
                   nextState == State::ERROR);
        break;
    case State::ERROR:
        allowed = (nextState == State::UNINITIALIZED);
        break;
    }

    if (!allowed) {
        Serial.printf("[MP3 CTRL] reject transition %s -> %s by %s\n",
                      stateName(state_), stateName(nextState), action);
        return false;
    }

    Serial.printf("[MP3 CTRL] transition %s -> %s by %s\n",
                  stateName(state_), stateName(nextState), action);
    state_ = nextState;
    return true;
}

/**功能: 将字符串路径转换成短标题 */
String Mp3Controller::titleFromPath(const String& path) const {
    int lastSlash = path.lastIndexOf('/');
    String title = (lastSlash >= 0) ? path.substring(lastSlash + 1) : path;
    if (title.endsWith(".mp3")) title.remove(title.length() - 4);
    if (title.endsWith(".MP3")) title.remove(title.length() - 4);
    return title;
}

/**功能: 读取文件大小并填充曲目信息 */
bool Mp3Controller::fillTrackInfo(TrackInfo& track) {
    const std::string fullPath = resolveMountedPath(track.path.c_str());
    struct stat st = {};
    if (stat(fullPath.c_str(), &st) != 0) {
        Serial.printf("[MP3 CTRL] failed to stat: %s\n", track.path.c_str());
        track.sizeBytes = 0;
        return false;
    }
    track.sizeBytes = static_cast<size_t>(st.st_size);
    return true;
}

/**功能: 扫描 /music 目录并按文件名稳定排序 */
bool Mp3Controller::scanPlaylist() {
    Serial.println("[MP3 CTRL] scan /music");
    String names[kMaxPlaylist];
    playlistCount_ = player_.scanMp3Files("/music", names, kMaxPlaylist);
    currentIndex_ = -1;
    selectedIndex_ = 0;

    for (int i = 0; i < playlistCount_; ++i) {
        playlist_[i] = {};
        playlist_[i].path = names[i];
        playlist_[i].title = titleFromPath(names[i]);
        fillTrackInfo(playlist_[i]);
    }

    if (playlistCount_ > 1) sortPlaylist();
    Serial.printf("[MP3 CTRL] scan result: %d track(s)\n", playlistCount_);
    return true;
}

/**功能: 对歌单做升序稳定排序 */
void Mp3Controller::sortPlaylist() {
    std::stable_sort(playlist_, playlist_ + playlistCount_,
                     [](const TrackInfo& lhs, const TrackInfo& rhs) {
                         return lhs.title < rhs.title;
                     });
}

/**功能: 从播放器元数据同步当前曲目技术信息和时长估算 */
void Mp3Controller::syncTrackMetrics() {
    if (currentIndex_ < 0 || currentIndex_ >= playlistCount_) return;
    const Mp3Player::TrackMetrics metrics = player_.trackMetrics();
    if (metrics.fileSizeBytes > 0) playlist_[currentIndex_].sizeBytes = metrics.fileSizeBytes;
    if (metrics.durationKnown) {
        playlist_[currentIndex_].durationKnown = true;
        playlist_[currentIndex_].durationMs = metrics.durationMs;
    }
}

/**功能: 统一记录失败信息并切换到 ERROR 状态 */
void Mp3Controller::setError(const char* action, const char* reason) {
    errorText_ = String(action) + ": " + reason;
    Serial.printf("[MP3 CTRL] ERROR %s\n", errorText_.c_str());
    if (state_ == State::ERROR) return;
    if (!transitionTo(State::ERROR, action, true)) {
        state_ = State::ERROR;
    }
}

/**功能: 保证索引落在歌单范围内 */
int Mp3Controller::normalizeIndex(int index) const {
    if (playlistCount_ <= 0) return 0;
    if (index < 0) return playlistCount_ - 1;
    if (index >= playlistCount_) return 0;
    return index;
}
