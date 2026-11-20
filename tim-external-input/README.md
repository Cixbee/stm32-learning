# tim-external-input —— TIM2 外部时钟计数

## 实验目的

把 TIM2 配置成**外部时钟模式 2（ETR）**，用外部引脚上的脉冲作为计数器时钟源，实现对外部脉冲的硬件计数，并把计数值实时显示在 OLED 上。

## 硬件连接

| 信号 | 引脚 | 说明 |
|---|---|---|
| 外部脉冲输入 | `PA0` (TIM2_ETR) | 接信号发生器 / 按键 / 编码器 A 相，0–3.3 V |
| OLED SCL | `PB6` (I2C1_SCL) | SSD1306 |
| OLED SDA | `PB7` (I2C1_SDA) | SSD1306 |

## 关键配置

| 项目 | 值 | 说明 |
|---|---|---|
| 时钟源 | `TIM_CLOCKSOURCE_ETRMODE2` | 外部时钟模式 2，ETR 引脚直接作为计数时钟 |
| `PSC` | 0 | 不分频，来一个有效边沿计 1 |
| `ARR` | 65535 | 16 位满量程，计数满后回到 0 |
| 极性 | `NONINVERTED` | 上升沿计数 |
| 滤波 | 0 | 输入无滤波，信号抖动会被当成多个脉冲 |

## 程序流程

```c
MX_TIM2_Init();
OLED_Init();
HAL_TIM_Base_Start(&htim2);       // 只启动时基，不开中断

while (1) {
    OLED_NewFrame();
    counter = __HAL_TIM_GET_COUNTER(&htim2);
    sprintf(message, "counter: %d", counter);
    OLED_PrintString(0, 0, message, &font16x16, OLED_COLOR_NORMAL);
    OLED_ShowFrame();
    HAL_Delay(100);
}
```

## 现象

OLED 第一行显示 `counter: N`。每给 `PA0` 一个上升沿，N 加 1；计到 65535 后回到 0 重新计数。没有输入信号时数值保持不变。

## 备注

- 这是纯硬件计数，不占用 CPU，也不怕主循环慢——脉冲频率高到几百 kHz 也能准确计。
- **信号抖动会被重复计数**（滤波为 0）。用机械按键当信号源时会看到一次按下跳好几个数，需要把 `ClockFilter` 设为非 0（如 `TIM_CLOCKFILTER_CORRECT_N_8`）或在外部加 RC 消抖。
- 输入电压不要超过 3.3 V。若信号源是 5 V 器件，需分压或加电平转换。
- 需要测量超过 65535 个脉冲时，可以在更新中断里累加溢出次数，组成 32 位计数器。
