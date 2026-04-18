# ESP8266 舵机控制器

ESP8266 微控制器固件，负责接收 MQTT 控制指令并通过串口转发给 STM32 下位机。

## 功能概述

- 订阅 MQTT 主题 `jetson/esp8266/cmd` 接收舵机控制指令
- 发布 MQTT 主题 `jetson/esp8266/info` 发送设备心跳状态
- 通过串口转发二进制控制数据包给 STM32

## 文件说明

| 文件 | 说明 | 适用场景 |
|-----|------|---------|
| `esp8266-oled-mqtt-uart/esp8266-oled-mqtt-uart.ino` | 校园网认证 + MQTT 控制器 | 需要 portal 认证的网络环境 |
| `use_selfwifi.cc` (参考) | 直连 WiFi + MQTT 控制器 | 无需认证的 WiFi 网络 |

## MQTT 主题

### 订阅

| 主题 | 说明 |
|-----|------|
| `jetson/esp8266/cmd` | 舵机控制指令 |

**指令格式**：
```json
{"id": "001", "col": 5, "row": 3}
```

- `id`: 设备 ID (固定为 "001")
- `col`: 水平位置，范围 -8 ~ 8
- `row`: 垂直位置，范围 -10 ~ 10

### 发布

| 主题 | 说明 |
|-----|------|
| `jetson/esp8266/info` | 设备心跳 |

**心跳格式**：
```json
{"id": "001", "status": "online", "uptime": 12345}
```

## 串口通信协议

通过 Hardware Serial 发送 6 字节二进制包：

| 字节 | 含义 | 示例 |
|-----|------|-------|
| 0 | 帧头 | 0xAA |
| 1 | 功能码 | 0x01 |
| 2 | COL 值 (uint8_t) | 0x05 |
| 3 | ROW 值 (uint8_t) | 0x03 |
| 4 | 校验和 | 0xAD |
| 5 | 帧尾 | 0x55 |

**校验和计算**：`byte[0] + byte[1] + byte[2] + byte[3]`

## 硬件连接

```
ESP8266 (D9, D10)  ---SoftwareSerial--->  STM32 (RX, TX)
```

## 配置参数

### MQTT 配置

```cpp
const char *mqtt_broker = "10.60.83.159";
const char *mqtt_username = "user";
const char *mqtt_password = "fuck123456";
const int mqtt_port = 1883;
```

### 设备 ID

```cpp
const char *device_id = "001";
```

### WiFi 配置 (use_selfwifi.cc)

```cpp
const char *ssid = "ZTE-ExDsxH";
const char *password = "520heweijie";
```

### 校园网配置 (esp8266-oled-mqtt-uart.ino)

```cpp
const char *ssid = "GUET-WiFi";
const char *campus_account = "2200340118";
const char *campus_account_suffix = "@unicom";
const char *campus_password = "A159357zop";
```

## 使用步骤

### 方式一：直连 WiFi (use_selfwifi.cc)

1. 修改 WiFi SSID 和密码
2. 使用 Arduino IDE 烧录到 ESP8266
3. 上电自动连接 WiFi 和 MQTT

### 方式二：校园网认证 (esp8266-oled-mqtt-uart.ino)

1. 修改校园网账号和密码
2. 使用 Arduino IDE 烧录到 ESP8266
3. 上电后自动连接校园网并进行 portal 认证
4. 认证成功后连接 MQTT

## 依赖库

- `ESP8266WiFi`
- `PubSubClient`
- `ArduinoJson`
- `SoftwareSerial` (use_selfwifi.cc)
- `ACROBOTIC_SSD1306` (use_selfwifi.cc OLED)

## 舵机范围

- **COL (左右)**: -8 ~ 8
- **ROW (上下)**: -10 ~ 10

## 工作流程

1. 订阅 MQTT `jetson/esp8266/cmd` 主题
2. 收到 JSON 指令，解析 `id`, `col`, `row`
3. 匹配设备 ID 后打包成 6 字节二进制
4. 通过 Serial 发送给 STM32
5. 每秒发送心跳到 `jetson/esp8266/info`

---

## 控制示例

### MQTT 发送指令

```bash
# 使用 mosquitto_pub
mosquitto_pub -t jetson/esp8266/cmd -m '{"id":"001","col":5,"row":3}'
```

---

## 许可证

MIT License