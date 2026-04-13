# STM32-BISHE

基于 **STM32F103C8T6** 的嵌入式实验/毕业设计工程，使用 **STM32CubeMX + HAL + FreeRTOS + Keil MDK-ARM** 开发。

当前工程已经接入：

- FreeRTOS 任务框架
- 3 个按键输入
- LED 指示灯
- 蜂鸣器
- I2C OLED 显示屏
- USART1 / USART2 串口测试
- TIM3 PWM 双路舵机控制

---

## 1. 开发环境

- MCU：`STM32F103C8T6`
- IDE / Toolchain：`Keil MDK-ARM V5`
- 工程文件：`STM32-BISHE/MDK-ARM/STM32-BISHE.uvprojx`
- CubeMX 配置：`STM32-BISHE/STM32-BISHE.ioc`
- HAL 固件包：`STM32Cube FW_F1 V1.8.7`

> 当前命令行编译可使用：
>
> ```bat
> "D:\Keil_v5\UV4\uVision.com" -b "E:\STM32F103C8T6-bishe\STM32-BISHE\MDK-ARM\STM32-BISHE.uvprojx" -j0
> ```

---

## 2. 工程目录结构

```text
STM32F103C8T6-bishe/
├─ README.md
├─ STM32-BISHE/
│  ├─ Core/
│  │  ├─ Inc/
│  │  └─ Src/
│  ├─ Drivers/
│  ├─ Middlewares/
│  ├─ MDK-ARM/
│  └─ STM32-BISHE.ioc
```

常用源文件：

- `Core/Src/main.c`：系统初始化、外设初始化、启动调度器
- `Core/Src/freertos.c`：FreeRTOS 任务逻辑
- `Core/Src/Key.c`：按键扫描
- `Core/Src/Led.c`：LED 控制
- `Core/Src/Buzzer.c`：蜂鸣器控制
- `Core/Src/oled.c`：OLED 驱动
- `Core/Src/usart.c`：USART1 / USART2 初始化与串口测试
- `Core/Src/steering_engine.c`：双舵机 PWM 控制
- `Core/Src/tim.c`：TIM3 PWM 初始化

---

## 3. 当前功能

### 3.1 FreeRTOS

工程使用 CMSIS-RTOS V2 接口创建任务，当前主要有：

- `defaultTask`
  - 按键扫描
  - 双舵机测试控制
- `ledBlinkTask`
  - LED 周期闪烁
- `oledTestTask`
  - OLED 显示舵机当前状态

### 3.2 按键

当前按键用于测试双舵机：

- `KEY1`：当前选中舵机 `PWM + 50`
- `KEY2`：当前选中舵机 `PWM - 50`
- `KEY3`：切换当前控制的舵机（S1 / S2）

切换舵机时会有一个很短的蜂鸣器提示音。

### 3.3 OLED

OLED 使用 I2C1 驱动，当前显示：

- `SERVO STATUS`
- `S1 PWM:xxxx`
- `S2 PWM:xxxx`
- `C:+x R:+y`

### 3.4 串口

- USART1：已配置 6 字节协议接收解析，用于控制双舵机
- USART2：已配置接收回显测试，并在启动后发送 `USART2 ready`

### 3.5 舵机

舵机使用 TIM3 双路 PWM：

- `PA6 -> TIM3_CH1`
- `PA7 -> TIM3_CH2`

当前代码中 TIM3 已手动调整为 **50Hz 舵机标准周期**：

- Prescaler：`72 - 1`
- Period：`20000 - 1`

PWM 取值范围当前测试使用：

- 最小：`500`
- 中位：`1500`
- 最大：`2500`
- 步进：`50`

---

## 4. 主要引脚

### 4.1 按键 / 指示器

- `PA4`：KEY1
- `PA5`：KEY2
- `PB10`：KEY3
- `PB3`：LED
- `PA15`：Buzzer

### 4.2 OLED

- `PB6`：I2C1_SCL
- `PB7`：I2C1_SDA

### 4.3 串口

#### USART1
- `PA9`：TX
- `PA10`：RX

#### USART2
- `PA2`：TX
- `PA3`：RX

### 4.4 舵机 PWM

- `PA6`：TIM3_CH1
- `PA7`：TIM3_CH2

---

## 5. 如何编译与下载

### 5.1 Keil 中编译

直接打开：

`STM32-BISHE/MDK-ARM/STM32-BISHE.uvprojx`

然后在 Keil 中 Build / Download。

### 5.2 命令行编译

```bat
"D:\Keil_v5\UV4\uVision.com" -b "E:\STM32F103C8T6-bishe\STM32-BISHE\MDK-ARM\STM32-BISHE.uvprojx" -j0
```

---

## 6. 当前测试说明

### 6.1 舵机测试

上电后：

- 两个舵机默认都到 `1500`
- 默认选中 `S1`
- OLED 会显示两个舵机的 PWM 值

按键测试：

- `KEY1`：当前舵机增大 PWM
- `KEY2`：当前舵机减小 PWM
- `KEY3`：切换控制对象

### 6.2 串口测试

#### USART1
USART1 当前不再做简单回显，而是解析 6 字节控制包：

```text
[0] 0xAA
[1] 0x01
[2] col
[3] row
[4] checksum = 0xAA + 0x01 + col + row
[5] 0x55
```

其中：

- `col`：`int8_t`，范围建议 `-8 ~ 8`
- `row`：`int8_t`，范围建议 `-10 ~ 10`
- ESP8266 发送负数时使用 **补码**

STM32 侧当前映射规则：

- `CH1 PWM = 1500 + col * 50`，限幅到 `1000 ~ 1800`
- `CH2 PWM = 1500 - row * 50`，限幅到 `1000 ~ 2000`

因此：

- `col = 0` / `row = 0` 时，两路都对应 `1500` 中位
- OLED 最后一行会显示接收到的原始 `C/R` 值

#### USART2
串口助手连接 `USART2 / 115200 / 8N1`：

- 上电会先收到：`USART2 ready`
- 发送字符后会收到原样回显

---

## 7. 已知注意事项

### 7.1 舵机供电

舵机抖动、复位、黑屏等问题，很多时候不是代码问题，而是：

- 舵机本体损坏
- 供电能力不足
- 电源线/地线接触不良
- 脉宽范围过宽

建议：

- 舵机尽量使用稳定的独立电源
- 与开发板 **共地**
- 先从中位 `1500` 和较小步进开始测试
- 如果走串口协议控制，先用小范围 `col/row` 测试，再逐步扩大

### 7.2 CubeMX 与手改代码的差异

当前 `tim.c` 中 TIM3 已手动改成 **20ms / 50Hz**，但 `.ioc` 中仍可能保留旧的参数显示。

如果后续重新用 CubeMX 生成代码，需要同步检查：

- `TIM3.Period`
- `TIM3.Pulse`

否则手动改动可能被覆盖。

### 7.3 头文件 warning

当前编译中仍有少量 warning，主要是：

- `Key.h`
- `Led.h`
- `Buzzer.h`

文件末尾缺少换行。

这不影响当前功能，但后续可以顺手清理。

---

## 8. 后续可扩展方向

- OLED 显示更多状态信息
- 舵机角度抽象封装
- 串口命令控制舵机 / LED / OLED
- 按键长按 / 连续调节舵机
- 将测试逻辑重构成独立模块

---

## 9. 备注

这是一个持续迭代中的实验工程，当前 README 主要记录：

- 工程结构
- 当前接线与功能
- 主要测试方式
- 已知注意事项

如果后续继续新增模块，建议同步更新本文档。
