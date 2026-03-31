# Mp3Controller 状态机

## 结论

`Mp3Controller` 统一管理首页卡片和 `MusicPlayerState` 的 MP3 播放状态。  
它不是简单的播放器包装，而是一个显式状态机，所有播放、暂停、恢复、停止、自动下一首都必须经过这套状态流转。

## 状态机图

```text
                 +------------------+
                 |  UNINITIALIZED   |
                 +------------------+
                    | init ok
                    v
                 +------------------+
                 |       IDLE       |
                 +------------------+
                    | play(index)
                    v
                 +------------------+
        pause     |     PLAYING      | stop/exit
     +----------> +------------------+ ----------+
     |                 | next/play(other)        |
     |                 v                         |
     |           +------------------+            |
     |  resume   |      PAUSED      | stop/exit  |
     +-----------+------------------+ ----------+
                    | resume/play
                    v
                 +------------------+
                 |     PLAYING      |
                 +------------------+

                 +------------------+
                 |     STOPPED      |
                 +------------------+
                    | play(index)
                    v
                 +------------------+
                 |     PLAYING      |
                 +------------------+

任何初始化失败/播放启动失败/底层异常
            -> +------------------+
               |      ERROR       |
               +------------------+
                    | reset/reinit
                    v
               +------------------+
               |  UNINITIALIZED   |
               +------------------+
```

## 关键规则

- `UNINITIALIZED` 只允许进入 `IDLE` 或 `ERROR`
- `IDLE` 只允许进入 `PLAYING` 或 `ERROR`
- `PLAYING` 可以切到 `PAUSED`、`STOPPED`、`PLAYING`、`ERROR`
- `PAUSED` 可以恢复到 `PLAYING`，也可以切到 `STOPPED` 或 `ERROR`
- `STOPPED` 不能直接进入 `PAUSED`，只能重新 `play(index)` 回到 `PLAYING`
- `ERROR` 只能通过 `reset/reinit` 回到 `UNINITIALIZED`

## 设计目的

- 首页和播放页共用同一份播放事实，避免两个状态各自维护一套 MP3 状态
- 非法动作必须被拒绝并输出日志，防止 UI 和底层播放器状态漂移
- 自然播完后的自动下一首也走统一状态机，而不是页面层手工拼逻辑
