# Monitor Alarm Upload Gating Design

## Goal

将当前 `monitor_node` 的“达到违规触发条件后立即播放告警音并抓拍上传”策略，改为“双层触发”策略：每次达到违规触发条件时先播放告警音并累计连续告警次数；只有当连续告警次数达到配置阈值后，才执行一次抓拍上传；支持上传后清零与静默超时清零。

## Existing Context

- `monitor_node` 当前在 `resultCallback()` 中对 `fire` / `smoking` 做窗口计数与冷却判断。
- 一旦达到 `trigger_frame_threshold` 且通过 `trigger_cooldown_seconds`，就会调用 `enqueueCaptureAndAlarm()`。
- `enqueueCaptureAndAlarm()` 当前把两件事绑定在一起异步执行：
  1. `playAlarm()`
  2. `captureAndUpload()`
- 因此当前无法做到“先多次播放告警音，再抓拍上传”。

## Confirmed Decisions

1. 不采用视觉多模态大模型复核，改为本地告警次数门控上传。
2. 连续告警次数门槛必须能从 `cameras.yaml` 手动配置。
3. 连续次数在抓拍上传后可按配置清零。
4. 连续次数在长时间无新告警时也可按配置自动清零。
5. 上传完成后进入下一轮新的统计与告警循环。

## Recommended Approach

采用“违规命中判定 + 告警次数判定”的双层策略：

- **第一层：违规触发层**
  - 保留现有 `hit_times`、`trigger_frame_threshold`、`trigger_cooldown_seconds` 逻辑。
  - 只有达到这一层，才认为“本次应该执行一次告警”。

- **第二层：上传门控层**
  - 每次第一层触发成功时：
    - 播放一次告警音
    - 对该违规类型的 `alarm_trigger_count` 加 1
  - 只有当 `alarm_trigger_count >= upload_after_alarm_count` 时：
    - 才抓拍上传
    - 之后按配置决定是否清零计数

## Why This Approach

这个方案最符合当前代码结构：

- 只需修改 `bishe_monitor` 和 launch/config 接线。
- 不需要改 `detector_node`、`streamer_node`、`mqtt_node` 的主链路。
- 复用现有 `AsyncTaskWorker`、`captureAndUpload()` 和上传接口。
- 把误报上传的门槛推高，但不影响本地告警音提示。

## State Extension

当前 `EventState` 只有：

- `hit_times`
- `last_trigger_time`

建议扩展为：

- `alarm_trigger_count`
  - 当前这轮已连续触发过多少次告警音
- `last_alarm_time`
  - 最近一次成功触发告警音的时间

## New Parameters

新增 monitor 参数：

- `upload_after_alarm_count`：默认建议 `2`
- `reset_alarm_count_after_upload`：默认建议 `true`
- `alarm_count_reset_timeout_seconds`：默认建议 `30`

参数语义：

- `upload_after_alarm_count >= 1`
  - 连续多少次告警音后才抓拍上传
- `reset_alarm_count_after_upload`
  - 上传完成后是否立即把本轮连续告警次数清零
- `alarm_count_reset_timeout_seconds >= 0`
  - 若超过该静默时长没有新的告警触发，则自动清零
  - 为 `0` 表示禁用静默超时清零

## Behavioral Rules

以某一违规类型（如 `smoking`）为例：

1. 检测命中达到 `trigger_frame_threshold`
2. 通过 `trigger_cooldown_seconds` 检查
3. 更新本次违规触发时间并清空 `hit_times`
4. 如配置了静默清零且距离 `last_alarm_time` 已超时，则先清空 `alarm_trigger_count`
5. 播放一次告警音
6. `alarm_trigger_count += 1`
7. 若 `alarm_trigger_count < upload_after_alarm_count`
   - 本次结束，不抓拍上传
8. 若 `alarm_trigger_count >= upload_after_alarm_count`
   - 抓拍上传
   - 若 `reset_alarm_count_after_upload=true`，则清零计数

## Example Behavior

当配置为：

```yaml
upload_after_alarm_count: 3
reset_alarm_count_after_upload: true
alarm_count_reset_timeout_seconds: 30
```

则行为为：

- 第 1 次告警：播放语音，不上传
- 第 2 次告警：播放语音，不上传
- 第 3 次告警：播放语音并抓拍上传，然后计数清零
- 若任意两次告警之间静默超过 30 秒，则下一次重新按第 1 次开始算

## Data Flow Changes

当前：

- `resultCallback()` -> `enqueueCaptureAndAlarm()` -> `playAlarm()` + `captureAndUpload()`

调整后：

- `resultCallback()` -> 先做违规触发层判断
- 通过后先调用 `playAlarm()`（或 enqueue 仅播放音频）
- 再判断是否达到上传门槛
- 只有达到上传门槛时才调 `enqueueCaptureUpload()` 或类似上传任务函数

## Risk Notes

1. 若 `upload_after_alarm_count` 设得太高，可能会延迟实际证据留存。
2. 若 `alarm_count_reset_timeout_seconds` 太短，可能导致连续统计很难累积到上传门槛。
3. 若 `reset_alarm_count_after_upload=false`，需要明确是否允许后续每次达到门槛都持续上传；这会影响上传频率。

## Validation Strategy

1. 为告警上传门控逻辑提取可测试 helper，避免所有逻辑都堆在 `monitor_node.cpp`。
2. 验证“只告警不上传”路径。
3. 验证“达到第 N 次后上传并清零”路径。
4. 验证“静默超时后计数清零”路径。
5. 验证 `reset_alarm_count_after_upload=false` 时计数保留的行为。

## Non-Goals

- 不修改 `detector_node` 的检测逻辑
- 不修改 `streamer_node` 的推流逻辑
- 不引入外部多模态模型复核
