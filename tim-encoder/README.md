# tim-encoder —— 编码器接口 + PWM 联动

## 实验目的

用 TIM1 的**编码器接口模式**读取增量式旋转编码器的正交 A/B 信号（硬件自动判向 + 计数，不占 CPU、不丢步），把计数值显示在 OLED 上，并用同一个计数值去调节 TIM3 的 PWM 占空比，体会「输入捕获类定时器功能」与「输出比较」的配合。

## 硬件连接

| 信号 | 引脚 | 说明 |
|---|---|---|
| 编码器 A 相 | `PA8` (TIM1_CH1) | 增量式编码器（如 EC11），需 3.3 V 供电 |
| 编码器 B 相 | `PA9` (TIM1_CH2) | 同上 |
| PWM 输出 | `PA6` (TIM3_CH1) | 接 LED 或示波器，观察占空比随旋钮变化 |
| OLED SCL / SDA | `PB6` / `PB7` (I2C1) | SSD1306 |

## 关键配置

| 项目 | 值 | 说明 |
|---|---|---|
| TIM1 模式 | 编码器接口，`TIM_ENCODERMODE_TI1` | 只在 TI1 边沿计数，方向由 TI2 电平判定 |
| TIM1 `PSC` | 2 - 1 | 2 分频，每两个脉冲计一次数，降低灵敏度 |
| TIM1 `ARR` | 65535 | 16 位满量程，正转加、反转减 |
| TIM3 `PSC` / `ARR` | 72 - 1 / 100 - 1 | 1 MHz 计数，**10 kHz** PWM，0–99 对应 0–100% 占空比 |

## 程序流程

```c
OLED_Init();
HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);   // 编码器接口启动

while (1) {
    if (count > 6000) { count = 0;        __HAL_TIM_SET_COUNTER(&htim1, 0); }   // 溢出兜底
    else if (count > 100) { count = 100;  __HAL_TIM_SET_COUNTER(&htim1, 100); } // 上限钳位

    count = __HAL_TIM_GET_COUNTER(&htim1);
    sprintf(message, "count: %d", count);
    OLED_PrintString(13, 0, message, &font16x16, OLED_COLOR_NORMAL);   // 数字读数
    OLED_DrawRectangle(13, 25, 101, 12, OLED_COLOR_NORMAL);            // 进度条外框
    OLED_DrawFilledRectangle(13, 26, count, 11, OLED_COLOR_NORMAL);    // 进度条填充
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, count);               // 占空比跟随

    OLED_ShowFrame();
    HAL_Delay(100);
}
```

## 现象

- 正转编码器：OLED 上数字从 0 递增，进度条变长，`PA6` 上 LED 逐渐变亮；
- 反转编码器：数字递减、进度条缩短、LED 变暗；
- 旋到 100 后继续转，读数保持 100 不再上涨（被钳位）。

## 备注

- 编码器模式三种选择：`TI1`（只在 TI1 边沿计数）、`TI2`、`TI12`（两个通道的所有边沿都计数，即 4 倍频，分辨率最高）。这里用 `TI1`，所以一圈的计数值约为编码器标称脉冲数的一半。**换成 `TIM_ENCODERMODE_TI12` 可以让旋转更跟手**，代价是抖动也被放大。
- 代码里 `count > 6000` 那条分支实际不会执行——因为 `count > 100` 会先把它钳到 100 并写回计数器，之后永远到不了 6000。留着当兜底没问题，但要注意这个顺序。
- 钳位后调用了 `__HAL_TIM_SET_COUNTER()` 把计数器也写回，这样「转回来」时能立刻响应，而不是先要把多转的那些步数退完。这是个实用技巧。
- 机械编码器（EC11）的触点抖动较重，如果读数跳变明显，可以在 TIM1 的 `IC1Filter` / `IC2Filter` 上加滤波（如 6–10），或在 A/B 上并 100 nF 电容。
- `PA7`(TIM3_CH2)、`PB0`(TIM3_CH3) 在 `.ioc` 里也配成了 PWM 输出，本示例只用了 CH1，留着方便接双路/三路负载（如 RGB 灯、双电机）。
