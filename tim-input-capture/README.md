# tim-input-capture —— 输入捕获 + HC-SR04 超声波测距

## 实验目的

用 TIM1 的输入捕获测量外部脉冲的**高电平宽度**，借此驱动 HC-SR04 超声波模块完成测距：模块把距离转换成回波脉宽，MCU 只负责把脉宽换算成厘米。同时练习「直接/间接捕获通道」配合测量脉宽的方法。

## 硬件连接

| 信号 | 引脚 | 说明 |
|---|---|---|
| TRIG | `PA11`（GPIO 输出） | 接 HC-SR04 的 Trig，高电平 ≥10 µs 触发一次测距 |
| ECHO | `PA10`（TIM1_CH3） | 接 HC-SR04 的 Echo，脉宽与距离成正比 |
| OLED SCL | `PB6` (I2C1_SCL) | SSD1306 |
| OLED SDA | `PB7` (I2C1_SDA) | SSD1306 |

## 关键配置

| 项目 | 值 | 说明 |
|---|---|---|
| 定时器时钟 | 72 MHz | TIM1 挂在 APB2 |
| `PSC` | 72 - 1 | 计数频率 **1 MHz → 1 个计数 = 1 µs** |
| `ARR` | 默认 16 位满量程 | 65535 µs 内不会溢出，足够覆盖 HC-SR04 的脉宽 |
| CH3 | 输入捕获，直接模式 `TI3`，**上升沿** | 记录回波起点 |
| CH4 | 输入捕获，间接模式 `TI3`，**下降沿** | 记录回波终点，同时触发中断 |

两个通道都映射到同一个引脚 `PA10`（TIM1 的 CH3/CH4 共用 `TI3`），也就是「一个信号、两个边沿」。CH3 捕上升沿、CH4 捕下降沿，两次捕获值之差就是脉宽。

## 程序流程

```c
HAL_TIM_Base_Start(&htim1);
HAL_TIM_IC_Start(&htim1, TIM_CHANNEL_3);        // 轮询方式，只记值
HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_4);     // 中断方式，下降沿时进回调

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim == &htim1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4) {
        upEdge   = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);
        downEdge = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_4);
        distance = (downEdge - upEdge) * 0.034 / 2;   // 声速 0.034 cm/µs，往返除以 2
    }
}

while (1) {
    HAL_GPIO_WritePin(Trig_GPIO_Port, Trig_Pin, GPIO_PIN_SET);
    HAL_Delay(1);                                   // Trig 保持高电平 1 ms
    HAL_GPIO_WritePin(Trig_GPIO_Port, Trig_Pin, GPIO_PIN_RESET);
    __HAL_TIM_SET_COUNTER(&htim1, 0);               // 清计数器
    HAL_Delay(20);                                  // 等回波捕获完成

    OLED_NewFrame();
    sprintf(message, "距离:%.2f cm", distance);
    OLED_PrintString(0, 0, message, &font16x16, OLED_COLOR_NORMAL);
    OLED_ShowFrame();
    HAL_Delay(500);
}
```

## 现象

OLED 第一行显示 `距离:xx.xx cm`，每 0.5 s 更新一次。手掌在模块前方移动，数值随之变化。

## 换算与量程

声速取 340 m/s = 0.034 cm/µs。回波脉宽 `Δt` 是声波「去 + 回」的总时间，所以：

```
距离(cm) = Δt(µs) × 0.034 / 2
```

例：测得 5800 µs → 5800 × 0.034 / 2 ≈ 98.6 cm。模块标称量程 2–400 cm，精度约 3 mm。

## 备注

- **ECHO 是 5 V 电平**，HC-SR04 用 5 V 供电时 Echo 输出接近 5 V，直接接 `PA10` 会超出 F103 的耐压。稳妥做法是分压（如 1 kΩ + 2 kΩ）或加电平转换；若模块能在 3.3 V 下工作则可直连。
- 必须先清计数器再发 Trig，否则 `downEdge - upEdge` 可能跨越溢出点导致结果错误。当前测距周期远小于 65 ms 的溢出周期，所以安全；若定时器周期设得很小，就要在溢出中断里累加高位。
- 没有回波（超出量程或没接模块）时捕获不会触发，`distance` 保持上次的值，可以加一个超时判断来提示「无回波」。
