# tim-slave-mode —— TIM2 从模式复位

## 实验目的

把 TIM2 配置成**从模式（Slave）复位模式**：定时器仍用内部时钟计数，但一旦检测到外部触发信号（`TI1FP1`，即 `PA0` 上升沿），硬件立即把计数器清零。用串口打印区分「正常溢出」与「被外部触发复位」，直观看到从模式对计数器的影响。

## 硬件连接

| 信号 | 引脚 | 说明 |
|---|---|---|
| 外部触发输入 | `PA0` (TIM2_CH1 / TI1FP1) | 接信号发生器或按键，0–3.3 V |
| UART2_TX | `PA2` | 接 USB-TTL 的 RX |
| UART2_RX | `PA3` | 接 USB-TTL 的 TX |

串口参数：**115200, 8N1**。

## 关键配置

| 项目 | 值 | 说明 |
|---|---|---|
| 时钟源 | `TIM_CLOCKSOURCE_INTERNAL` | 内部 72 MHz 时钟，从模式不影响时钟源 |
| `PSC` | 8000 - 1 | 计数频率 72 MHz / 8000 = **9 kHz** |
| `ARR` | 5000 - 1 | 无触发时 5000 / 9 kHz ≈ **0.556 s** 溢出一次 |
| `SlaveMode` | `TIM_SLAVEMODE_RESET` | 触发到来 → 计数器清零并重新计数 |
| `InputTrigger` | `TIM_TS_TI1FP1` | 触发源选通道 1 的滤波后信号（`PA0`） |
| `TriggerPolarity` | `RISING` | 上升沿触发 |
| `TriggerFilter` | 15 | 最大滤波，抑制输入抖动 |

## 程序流程

```c
HAL_TIM_Base_Start_IT(&htim2);

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim == &htim2) {
        if (__HAL_TIM_GET_FLAG(htim, TIM_FLAG_TRIGGER) == SET) {
            __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_TRIGGER);
            HAL_UART_Transmit(&huart2, (uint8_t*)triggermessage, strlen(triggermessage), 100);
            // → "从模式触发"
        } else {
            HAL_UART_Transmit(&huart2, (uint8_t*)updated_message, strlen(updated_message), 100);
            // → "自动重装载"
        }
    }
}
```

主循环每 500 ms 读一次计数器并通过串口打印 `Counter: N`。

## 现象

- **`PA0` 悬空 / 无脉冲**：每 0.556 s 打印一次「自动重装载」，`Counter` 在 0–4999 之间循环。
- **`PA0` 上有脉冲**：脉冲到来时计数器被硬件清零，本次更新中断的 `TIM_FLAG_TRIGGER` 置位，于是打印「从模式触发」，`Counter` 从 0 重新开始。

对比两种打印的交替出现，就能理解「复位模式」与普通自动重装载的区别。

## 备注

- 从模式只在**触发瞬间**改写计数器，不影响预分频和周期；`PSC`/`ARR` 仍然决定没有触发时的溢出节奏。
- 若触发信号频率高于溢出频率，会一直看到「从模式触发」，正常溢出几乎不出现——这是因为每次触发都把计数清零，永远走不到 `ARR`。
- 常见从模式还有门控（Gated）、触发（Trigger）和外部时钟 1，区别在于触发信号分别控制「计数允许」「启动计数」和「提供时钟」。
