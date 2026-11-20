# pwm-light —— TIM3 PWM 呼吸灯

## 实验目的

用 TIM3 的通道 1 输出 PWM 波，通过动态修改比较值（CCR）改变占空比，驱动 LED 实现「呼吸」效果，理解 `PSC` / `ARR` / `CCR` 三者的关系。

## 硬件连接

| 信号 | 引脚 | 说明 |
|---|---|---|
| PWM 输出 | `PA6` (TIM3_CH1) | 串联限流电阻接 LED 阳极，LED 阴极接 GND |

## 关键配置

| 项目 | 值 | 说明 |
|---|---|---|
| 定时器时钟 | 72 MHz | TIM3 挂在 APB1，定时器时钟经 ×2 后为 72 MHz |
| `PSC` | 72 - 1 | 计数频率 72 MHz / 72 = **1 MHz** |
| `ARR` | 100 - 1 | PWM 频率 1 MHz / 100 = **10 kHz** |
| PWM 模式 | PWM1 | 向上计数，`CNT < CCR` 时输出有效电平 |
| 初始 `Pulse` | 50 | 上电时占空比 50% |

## 程序流程

```c
HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);   // 启动 PWM 输出

while (1) {
    for (int i = 0; i < 100; i++) {          // 占空比 0 → 99%
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, i);
        HAL_Delay(10);
    }
    for (int i = 99; i >= 0; i--) {          // 占空比 99 → 0%
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, i);
        HAL_Delay(10);
    }
}
```

## 现象

`PA6` 上的 LED 呈呼吸式渐变：约 1 s 渐亮、1 s 渐暗，循环往复。因 `ARR = 99`，占空比调节分辨率是 1%，共 100 级。

## 备注

- 若 LED 亮度变化不明显，可把 `HAL_Delay(10)` 调大观察慢速渐变，或用示波器直接看 `PA6` 的波形与频率（应为 10 kHz）。
- 改 `ARR` 会同时改变 PWM 频率与占空比分辨率；改 `PSC` 只改变频率。实际工程中常把 `ARR` 设大（如 1000）以获得更细的调光粒度。
