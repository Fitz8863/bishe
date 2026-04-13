# AGENTS.md

本文件面向在本仓库中工作的 agent / coding assistant，目标是帮助后续修改尽量稳定、少踩坑。

## 1. 项目概况

- MCU：`STM32F103C8T6`
- 技术栈：`STM32CubeMX + HAL + FreeRTOS + Keil MDK-ARM`
- 主工程：`STM32-BISHE/MDK-ARM/STM32-BISHE.uvprojx`
- CubeMX 配置：`STM32-BISHE/STM32-BISHE.ioc`
- 源码根目录：`STM32-BISHE/Core`

这是一个 CubeMX 生成工程上持续叠加用户逻辑的项目。修改时要优先保护用户代码区和现有测试链路。

---

## 2. 必须遵守的修改原则

1. **优先改 `USER CODE BEGIN/END` 区域**
   - `main.c`
   - `freertos.c`
   - `usart.c`
   - `tim.c`
   - 其他 CubeMX 生成文件

2. **不要随意覆盖 CubeMX 生成代码**
   - 如果必须改生成参数，优先改 `.ioc`
   - 如果已经做了手工修改，要在提交说明里注明“CubeMX 重新生成可能覆盖”

3. **不要无故新增任务**
   - 当前工程已经有多个 FreeRTOS 任务
   - 简单外设测试优先复用 `defaultTask` 或现有中断回调

4. **不要假设串口 / OLED / 舵机一定有硬件问题或一定是软件问题**
   - 先区分“硬件链路正常”还是“任务/逻辑层异常”

5. **不要在未验证的情况下扩大 PWM / 舵机脉宽范围**
   - 当前测试值已经是经验值，不要盲目调很大

---

## 3. 当前代码结构重点

### 3.1 `main.c`

负责：

- HAL 初始化
- 时钟初始化
- GPIO / I2C1 / USART1 / USART2 / TIM3 初始化
- 启动 FreeRTOS 调度器

当前初始化顺序：

- `MX_GPIO_Init()`
- `MX_I2C1_Init()`
- `MX_USART1_UART_Init()`
- `MX_USART2_UART_Init()`
- `MX_TIM3_Init()`

注意：`main.c` 的 `USER CODE BEGIN 2` 里目前还有一条 USART2 启动测试发送：

```c
uint8_t uart2_direct_msg[] = "USART2 ready\r\n";
HAL_UART_Transmit(&huart2, uart2_direct_msg, sizeof(uart2_direct_msg) - 1, HAL_MAX_DELAY);
```

如果后续要做正式应用，可以删掉；做调试时保留也没问题。

### 3.2 `freertos.c`

当前主要任务：

- `defaultTask`
  - 按键扫描
  - 双舵机控制
  - 蜂鸣器短提示
- `ledBlinkTask`
  - LED 周期闪烁
- `oledTestTask`
  - OLED 显示舵机 PWM 状态

当前关键宏：

```c
#define KEY_SCAN_PERIOD_MS 10
#define SERVO_PWM_MIN      500
#define SERVO_PWM_MID      1500
#define SERVO_PWM_MAX      2500
#define SERVO_PWM_STEP     50
```

当前共享状态：

- `servo_pwm[2]`
- `selected_servo`

如果继续扩展舵机控制，优先复用这两个变量，不要重复造状态。

### 3.3 `usart.c`

当前已经有：

- USART1 初始化
- USART2 初始化
- `fputc()` 重定向到 USART1
- USART1 接收中断回显
- USART2 接收中断回显

如果 agent 修改串口逻辑，要注意：

- 不要同时引入新的串口测试任务和中断回显，容易互相干扰
- 如果要做命令行协议，建议在当前接收回调基础上改成缓冲/整行处理

### 3.4 `tim.c` / `steering_engine.c`

TIM3 当前用于双舵机 PWM：

- `PA6 -> TIM3_CH1`
- `PA7 -> TIM3_CH2`

代码里已手动调整为标准舵机周期：

```c
Prescaler = 72 - 1
Period    = 20000 - 1
```

也就是：

- 1 MHz 计数频率
- 20 ms 周期
- 50 Hz PWM

`steering_engine.c` 提供：

- `Steering_Engine_Init()`
- `Set_Pwm(index, pwm)`

当前 `Set_Pwm()` 是直接写 CCR，没有角度抽象层。

---

## 4. 当前功能行为

### 4.1 按键行为

- `KEY1`：当前选中舵机 `+50`
- `KEY2`：当前选中舵机 `-50`
- `KEY3`：切换当前控制舵机

切换时蜂鸣器是**短促提示**，不是持续翻转。

### 4.2 OLED 行为

当前 OLED 用于显示：

- `SERVO STATUS`
- `S1 PWM:xxxx`
- `S2 PWM:xxxx`
- `SEL:S1` / `SEL:S2`

如果后续更改 OLED 内容，请确认不会影响调试价值。

### 4.3 LED 行为

当前 LED 独立闪烁任务仍在运行，每秒闪烁一次。

---

## 5. 当前硬件映射

### 按键 / 指示器

- `PA4`：KEY1
- `PA5`：KEY2
- `PB10`：KEY3
- `PB3`：LED
- `PA15`：Buzzer

### OLED

- `PB6`：I2C1_SCL
- `PB7`：I2C1_SDA

### 串口

- USART1：`PA9 TX` / `PA10 RX`
- USART2：`PA2 TX` / `PA3 RX`

### 舵机

- `PA6`：TIM3_CH1
- `PA7`：TIM3_CH2

---

## 6. 编译与验证

### Keil 命令行编译

```bat
"D:\Keil_v5\UV4\uVision.com" -b "E:\STM32F103C8T6-bishe\STM32-BISHE\MDK-ARM\STM32-BISHE.uvprojx" -j0
```

### Agent 必做验证

只要改了代码，至少做以下一项：

1. **重新编译 Keil 工程**
2. 检查修改文件是否落盘

当前环境里 `clangd` 不可用，所以不要把 `lsp_diagnostics` 失败误判为代码错误。

---

## 7. 已知坑点

### 7.1 `.ioc` 与源码不一致

当前 `.ioc` 里 TIM3 仍然显示旧值：

- `TIM3.Period=10000-1`
- `TIM3.Pulse=5000`

但 `tim.c` 已经手改成：

- `Period = 20000-1`
- `Pulse = 1500`

**如果重新 CubeMX 生成代码，这个修改很可能被覆盖。**

### 7.2 舵机问题经常不是代码问题

历史排查已经验证过：

- 某个舵机抖动不一定是 PWM 通道问题
- 可能是舵机本体问题
- 也可能是供电/地线/接线问题

所以 agent 不应盲目反复改 PWM 逻辑。

### 7.3 `README.md` 是面向人类的说明

需要概览项目时先看：

- `README.md`

需要具体实现细节时再看：

- `main.c`
- `freertos.c`
- `usart.c`
- `tim.c`
- `steering_engine.c`

---

## 8. 推荐修改策略

如果要继续扩展功能，优先顺序建议：

1. 小改测试逻辑 → 先改 `freertos.c`
2. 串口功能 → 优先改 `usart.c`
3. 舵机功能 → 优先改 `steering_engine.c` + 少量 `freertos.c`
4. 重新配外设 → 优先同步 `.ioc` 与生成代码

不建议：

- 为简单测试继续堆更多 FreeRTOS 任务
- 在未验证供电前反复调舵机参数
- 直接覆盖 CubeMX 生成的整段初始化代码

---

## 9. Agent 工作建议

- 先读 `README.md`
- 再读实际相关源码
- 修改后优先用 Keil CLI 编译验证
- 如果行为问题与舵机/OLED/蜂鸣器相关，优先区分：
  - 软件逻辑
  - 引脚配置
  - 供电/硬件本体

这个仓库目前更像“持续演进的板级实验工程”，不是成熟应用项目。请以**小步修改、快速验证、避免破坏现有测试链路**为第一原则。
