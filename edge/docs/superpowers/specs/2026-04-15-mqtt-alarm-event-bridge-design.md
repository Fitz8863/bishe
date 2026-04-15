# MQTT Alarm Event Bridge Design

## Goal

在每次真正触发警报声音时，向 MQTT 主题 `jetson/alarm` 发送一条 JSON 告警消息，包含 `camera_id`、`location`、`alarm_type`，其中 `alarm_type` 使用英文枚举（`smoking` / `fire`）。

## Existing Context

- `monitor_node` 当前负责：
  - 监听 `detector/result`
  - 判断违规命中窗口和冷却
  - 触发告警音播放
  - 在达到上传门槛时执行抓拍上传
- `mqtt_node` 当前负责：
  - 连接 MQTT broker
  - 发布状态到 `jetson/info`
  - 处理来自 MQTT 的控制命令
- 现有架构中，`monitor_node` 并没有 MQTT 客户端；MQTT 连接统一集中在 `mqtt_node` 中。

## Confirmed Decisions

1. 不在 `monitor_node` 内部新增 MQTT 客户端。
2. 采用最小改动方案：`monitor_node` 先发布 ROS 告警事件，`mqtt_node` 再转发到 `jetson/alarm`。
3. `alarm_type` 使用英文枚举：`smoking` / `fire`。
4. JSON 核心字段至少包含：
   - `camera_id`
   - `location`
   - `alarm_type`
5. 可附带 `timestamp_ns` 作为辅助字段，便于后端去重和审计。

## Recommended Approach

采用“ROS 告警事件桥接 MQTT”的最小方案：

- **Step 1: monitor_node 发布告警事件**
  - 在每次真正触发警报声音时，`monitor_node` 发布一个 ROS 消息到新话题，例如：`alarm/event`
  - 消息类型优先使用 `std_msgs/msg/String`
  - 内容直接是 JSON 字符串

- **Step 2: mqtt_node 订阅并转发**
  - `mqtt_node` 新增一个 ROS 订阅者，订阅 `alarm/event`
  - 在回调中直接把收到的 JSON payload 发布到 MQTT 主题 `jetson/alarm`

## Why This Approach

这个方案最符合当前职责划分：

- `monitor_node` 继续只关心“什么时候产生告警事件”
- `mqtt_node` 继续统一负责“如何与 MQTT broker 通信”
- 不重复引入 Paho MQTT 客户端
- 不破坏现有 monitor / detector / streamer 的分层关系

## Event Trigger Point

“每次真正触发警报声音”应定义为：

- 每次 `enqueueAlarmPlayback()` 被调用时，都发布一条告警事件

即：

- 不要求达到抓拍上传门槛才发 MQTT
- 不要求上传成功后才发 MQTT
- 只和“本次确实进入了播放告警音流程”绑定

## Message Format

建议 JSON：

```json
{
  "timestamp_ns": 1713170000000000000,
  "camera_id": "001",
  "location": "生产车间A区",
  "alarm_type": "smoking"
}
```

字段约定：

- `camera_id`: 来自 `monitor_node` 当前参数
- `location`: 来自 `monitor_node` 当前参数
- `alarm_type`: 当前规范化后的违规类型（`smoking` / `fire`）
- `timestamp_ns`: 当前系统时间纳秒时间戳

## Topic Design

### ROS Topic

- 建议新建：`alarm/event`

理由：
- 语义清晰
- 未来如果还有其他节点也想发告警事件，可直接复用

### MQTT Topic

- 新增：`jetson/alarm`
- 建议在 `mqtt_node` 里加参数：`alarm_topic`
- 默认值：`jetson/alarm`

## Implementation Boundaries

### monitor_node changes

- 新增一个 `std_msgs::msg::String` publisher
- 新增一个小 helper 构造 alarm JSON
- 在 `enqueueAlarmPlayback()` 成功接收任务前后发布 ROS 告警事件

### mqtt_node changes

- 新增 `alarm_topic` 参数
- 新增一个 ROS 订阅者监听 `alarm/event`
- 在回调中调用现有 MQTT client 发布逻辑

## Risks

1. 若 `enqueueAlarmPlayback()` 只是“任务入队成功”，而音频设备实际播放失败，那么 MQTT 告警仍会先发出。
   - 这是当前“每次触发警报声音”语义下可接受的近似；因为真正等待音频进程成功再发告警会让链路复杂化。
2. 若未来后端需要更多字段（比如 device、violation_count），当前 JSON 结构需要扩展。
3. 使用 `std_msgs::msg::String` 虽然最省事，但不是强类型消息；若后续告警字段增多，可以再考虑自定义消息。

## Validation Strategy

1. 为 monitor 侧 alarm JSON 构造 helper 增加测试。
2. 为 mqtt 侧 alarm topic 转发 helper 增加测试。
3. 验证 monitor 每次触发告警音时都会发布 ROS 告警事件。
4. 验证 mqtt_node 收到事件后会向 `jetson/alarm` 发布原样 JSON。

## Non-Goals

- 不让 `monitor_node` 直接接入 MQTT broker
- 不新增自定义 ROS 消息类型（本次优先最小改动）
- 不改变现有 `jetson/info` 状态上报逻辑
