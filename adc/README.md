# adc —— ADC 单通道采样 + 串口输出

## 实验目的

用 ADC1 的规则通道 5 对模拟电压做单次轮询采样，把 12 位转换结果换算成实际电压并通过串口打印，理解 ADC 的参考电压、分辨率与采样时间三个关键参数。

## 硬件连接

| 信号 | 引脚 | 说明 |
|---|---|---|
| 模拟输入 | `PA5` (ADC1_IN5) | 接电位器中间抽头（两端接 3.3 V / GND），或 0–3.3 V 电压源 |
| UART2_TX | `PA2` | 接 USB-TTL 的 RX |
| UART2_RX | `PA3` | 接 USB-TTL 的 TX |

串口参数：**115200, 8N1**。

## 关键配置

| 项目 | 值 | 说明 |
|---|---|---|
| 分辨率 | 12 位 | 0–4095，`LSB = 3.3 V / 4096 ≈ 0.806 mV` |
| 转换模式 | 单次（`HAL_ADC_Start` + 轮询） | 每次转换完取一次值 |
| 采样时间 | 1.5 个 ADC 周期 | 最短采样时间，要求信号源内阻足够低 |
| ADC 时钟 | 12 MHz | APB2 72 MHz / 6（`RCC_ADCPCLK2_DIV6`），不得超过 14 MHz |

## 程序流程

```c
HAL_ADCEx_Calibration_Start(&hadc1);        // 上电必须先校准

while (1) {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
    value   = HAL_ADC_GetValue(&hadc1);              // 0–4095
    voltage = (value / 4095.0) * 3.3;                // 换算电压

    sprintf(message, "ADC: %d    %.2fV", value, voltage);
    HAL_UART_Transmit(&huart2, (uint8_t*)message, strlen(message), HAL_MAX_DELAY);
    HAL_Delay(500);
}
```

## 现象

串口每 0.5 s 输出一行，如 `ADC: 2048    1.65V`。旋转电位器，数值在 `0 – 4095` / `0.00 – 3.30V` 之间连续变化。

## 备注

- 分母用 `4095.0` 还是 `4096.0`：满量程 4095 对应 VREF+，用 4095 能让读数在最大值时正好等于 3.3 V，这也是代码里的取法；用 4096 更符合「LSB 均匀分布」的理论。实测差异只有 1 个 LSB，不必纠结。
- **采样时间 1.5 周期对信号源内阻很敏感**。F103 的 ADC 采样电容需要被外部信号快速充满，源阻抗高（如几百 kΩ 以上）时读数会明显偏低。电位器用 10 kΩ 没问题；如果换成高阻信号，把 `SamplingTime` 改成 `ADC_SAMPLETIME_55CYCLES_5` 或更长。
- 输入电压绝对不能超过 3.3 V（VDDA），否则会击穿 ADC 引脚。测更高电压先分压。
- 校准（`HAL_ADCEx_Calibration_Start`）只能在 ADC 关闭时调用，去掉它会带来几个 LSB 的固定偏差。
- 读数末尾跳动是正常的（噪声 ±2~3 LSB），需要平滑可以做 8 次平均或滑动滤波。
