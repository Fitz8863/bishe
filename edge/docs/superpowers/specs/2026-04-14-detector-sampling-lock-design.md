# Detector Sampling Lock Design

## Goal

在单摄像头模式下减少 YOLO 不必要的逐帧推理开销：默认每秒仅抽 1 帧执行 YOLO；当任意一次检测出现任意检测框时，进入 3 秒全帧 YOLO 检测窗口；窗口内再次检出任意框则刷新新的 3 秒；只有当窗口期内没有新的检测框时，才恢复默认抽样模式。

## Existing Context

- `bishe_detector` 当前对 `camera/image_raw` 的每一帧都入队并执行 YOLO，最终逐帧发布 `detector/result`。
- `bishe_streamer` 当前直接订阅 `detector/result` 并使用 `annotated_image` 作为推流输入。
- `bishe_monitor` 当前也直接订阅 `detector/result`，并通过 `has_violation` / `violation_type` 进行违规累计和抓拍。
- 因此，“跳过 YOLO”不能等价于“这帧不发布 `detector/result`”，否则会直接破坏推流链路的逐帧输入。

## Confirmed Decisions

1. 这是单摄像头场景，优先减少 YOLO 推理频率。
2. 锁定触发条件不是仅限违规类别，而是 **任意检测框**。
3. `detector/result` 必须继续逐帧发布，保证 `streamer_node` 的视频连续性。
4. 在未执行 YOLO 的帧上，`detector_node` 直接透传原始图像，并设置：
   - `has_violation=false`
   - `confidence=0.0`
   - `violation_type=""`

## Recommended Approach

采用“检测门控 + 逐帧透传发布”的方案，仅在 `bishe_detector` 内部引入轻量状态机：

- **Sampling 模式**
  - 默认模式。
  - 仅允许时间门控命中的帧进入 YOLO。
  - 其余帧不进推理队列，直接快速透传发布 `DetectorResult`。

- **Locked 模式**
  - 一旦某次 YOLO 输出包含任意检测框，进入该模式。
  - 3 秒窗口内全部帧都进入 YOLO。
  - 若窗口内任意一帧再次检出任意框，则把锁定截止时间刷新到 `now + 3s`。
  - 当截止时间到达且期间没有新的检出时，退回 Sampling 模式。

## Why This Approach

相比直接拆分视频主链路和检测主链路，这个方案改动最小：

- 不需要修改 `streamer_node` 和 `monitor_node` 的订阅结构。
- 不需要新增消息类型。
- 只在 `detector_node` 内部决定“这帧是否需要 YOLO”，对下游表现仍保持逐帧结果流。

## State Machine

### Sampling

- 维护 `next_sample_time`。
- 当前帧时间戳大于等于 `next_sample_time` 时，这一帧进入 YOLO。
- 一旦选中，刷新 `next_sample_time = now + sampling_interval`。

### Locked

- 维护 `locked_until`。
- 只要 `now < locked_until`，所有帧都进入 YOLO。
- 任意 YOLO 结果出现检测框时，刷新 `locked_until = now + lock_duration`。

## Data Flow

1. `camera_node` 继续逐帧发布 `camera/image_raw`。
2. `detector_node::imageCallback()` 收到帧后先做门控判断：
   - 若需要 YOLO：进入现有 worker queue。
   - 若跳过 YOLO：直接构造透传 `DetectorResult` 并发布。
3. `workerLoop()` 在推理完成后根据检测结果决定是否刷新锁定窗口。
4. `streamer_node` 和 `monitor_node` 保持现状，无需适配新消息结构。

## Parameters

新增 detector 参数：

- `sampling_interval_ms`：默认 `1000`
- `lock_duration_ms`：默认 `3000`

参数设计要求：

- `sampling_interval_ms >= 1`
- `lock_duration_ms >= 0`
- 支持运行时动态更新，便于后续调参

## Edge Cases

1. **锁定窗口期间再次检测到框**：刷新新的 3 秒。
2. **跳过 YOLO 的帧**：仍逐帧发布结果，避免推流掉帧。
3. **高帧率下采样**：按时间门控，不按固定 N 帧，避免帧率抖动导致抽样不稳定。
4. **锁定窗口到期**：若最近没有新的检测框，恢复到抽样模式。
5. **下游监控**：默认抽样会降低非锁定阶段的违规命中频率，这是该策略的预期行为。

## Risks

1. 默认抽样态下，推流画面大部分帧不会有检测框，这是策略结果，不是错误。
2. `monitor_node` 的命中速度会变慢，因为非锁定阶段每秒只有一次真实检测机会。
3. 需要确保门控状态变量在 `imageCallback` 与 `workerLoop` 间线程安全。

## Validation Strategy

1. 为门控决策提取独立可测逻辑，做单元测试。
2. 验证默认模式下 1 秒只放行 1 帧进 YOLO。
3. 验证出现检测框后进入锁定模式。
4. 验证锁定期间再次命中会刷新锁定时间。
5. 验证跳过 YOLO 的帧仍然逐帧发布 `detector/result`。

## Non-Goals

- 不重构 `streamer_node` 改为直接订阅 `camera/image_raw`
- 不新增新的检测消息类型
- 不修改 `monitor_node` 的触发策略
