# ESP32 OTA 升级流程速查

## 1. OTA 是什么

OTA（Over-The-Air）指设备在不接串口烧录的情况下，通过网络接收新固件并切换启动分区。

本项目当前支持两条 OTA 路径：

- 远程 OTA：设备先检查远程 manifest，再下载固件升级
- 本地网页上传 OTA：浏览器访问设备 IP，上传 `firmware.bin`

## 2. 本项目里的 OTA 升级过程

### 2.1 远程 OTA

流程如下：

1. 进入 `OTAState`
2. 显示当前固件版本
3. 按 `KEY1` 触发“检测升级”
4. 请求 `OTA_MANIFEST_URL`
5. 解析 manifest：

```json
{
  "version": "1.0.1",
  "firmware_url": "https://host/path/firmware.bin"
}
```

6. 若远程版本高于本地版本，进入确认页
7. 再按 `KEY1`，开始下载并写入 OTA 分区
8. 显示升级进度
9. 写入完成后切换 boot 分区
10. 显示“升级成功”
11. 自动重启，进入新固件

失败路径：

- manifest 请求失败
- JSON 解析失败
- 固件下载失败
- OTA 写入失败

统一表现为：

- 显示“超时退出”
- 回 OTA 首页

### 2.2 本地网页上传 OTA

流程如下：

1. 设备连上 WiFi
2. OTA 页面显示设备地址，例如 `http://192.168.1.10/`
3. 浏览器打开该地址
4. 选择本地 `firmware.bin`
5. 点击上传
6. 设备收到上传数据后开始写入 OTA 分区
7. 页面侧完成上传，设备侧显示升级进度
8. 写入完成后设置新的 boot 分区
9. 显示“升级成功”
10. 自动重启进入新固件

## 3. OTA 升级的底层本质

无论远程下载还是网页上传，底层核心都是一样的：

1. 找到下一个 OTA app 分区
2. 把新固件写进去
3. 校验写入结果
4. 把 boot 分区指向新 app
5. 重启

因此 OTA 能否正常工作，除了代码外，还依赖：

- 正确的 `partitions.csv`
- 至少两个 OTA app 分区
- 新固件大小不能超过单个 app 分区

## 4. 本项目用到的关键 API

### 4.1 远程 OTA：`esp_https_ota`

本项目远程 OTA 主要使用：

- `esp_https_ota_begin(...)`
- `esp_https_ota_perform(...)`
- `esp_https_ota_get_image_size(...)`
- `esp_https_ota_get_image_len_read(...)`
- `esp_https_ota_finish(...)`
- `esp_https_ota_abort(...)`

最小流程：

```cpp
esp_http_client_config_t httpConfig = {};
httpConfig.url = "https://host/firmware.bin";

esp_https_ota_config_t otaConfig = {};
otaConfig.http_config = &httpConfig;

esp_https_ota_handle_t handle = nullptr;
esp_err_t err = esp_https_ota_begin(&otaConfig, &handle);

while (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
    err = esp_https_ota_perform(handle);
}

if (err == ESP_OK) {
    esp_https_ota_finish(handle);
}
```

进度获取：

```cpp
int total = esp_https_ota_get_image_size(handle);
int read  = esp_https_ota_get_image_len_read(handle);
```

### 4.2 本地上传 OTA：`esp_ota_*`

本项目网页上传 OTA 主要使用：

- `esp_ota_get_next_update_partition(...)`
- `esp_ota_begin(...)`
- `esp_ota_write(...)`
- `esp_ota_end(...)`
- `esp_ota_set_boot_partition(...)`

最小流程：

```cpp
const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);
esp_ota_handle_t handle = 0;

esp_ota_begin(part, OTA_SIZE_UNKNOWN, &handle);
esp_ota_write(handle, data, size);
esp_ota_end(handle);
esp_ota_set_boot_partition(part);
```

### 4.3 自动重启

升级成功后重启使用：

```cpp
esp_restart();
```

## 5. 本项目相关入口

当前实现相关文件：

- [OtaService.cpp](/Users/kelly/CodeRepo/demo/ESP32_RLCD4_2/lib/features/ota/OtaService.cpp)
- [OtaState.cpp](/Users/kelly/CodeRepo/demo/ESP32_RLCD4_2/lib/core/state/OtaState.cpp)
- [partitions.csv](/Users/kelly/CodeRepo/demo/ESP32_RLCD4_2/partitions.csv)
- [platformio.ini](/Users/kelly/CodeRepo/demo/ESP32_RLCD4_2/platformio.ini)

## 6. 使用时记住这 4 点

最重要的只有 4 条：

- OTA 本质是“写入另一个 app 分区，再切换启动目标”
- 双分区是 OTA 的前提
- 固件大小必须小于单个 app 分区
- 成功升级后必须重启，新固件才会真正生效
