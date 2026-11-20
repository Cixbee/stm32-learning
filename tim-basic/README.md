# tim-basic —— TIM4 基础定时器 + 串口中断

## 实验目的

用基本定时器 TIM4 产生 1 s 周期的更新中断，在中断回调里通过 USART2 发送数据，验证定时器时基计算（`PSC` / `ARR`）与「中断 + 串口」的基本配合方式。

## 硬件连接

| 信号 | 引脚 | 说明 |
|---|---|---|
| UART2_TX | `PA2` | 接 USB-TTL 的 RX |
| UART2_RX | `PA3` | 接 USB-TTL 的 TX |

串口参数：**115200, 8N1**。

## 关键配置

| 项目 | 值 | 说明 |
|---|---|---|
| 定时器时钟 | 72 MHz | TIM4 挂在 APB1，定时器时钟 72 MHz |
| `PSC` | 7200 - 1 | 计数频率 72 MHz / 7200 = **10 kHz** |
| `ARR` | 10000 - 1 | 更新周期 10000 / 10 kHz = **1 s** |
| 中断 | NVIC 使能 TIM4 全局中断 | 每次溢出进入 `HAL_TIM_PeriodElapsedCallback` |

TIM4 是**基本定时器**：只有时基单元，没有输入捕获 / 输出比较通道，也没有外部引脚，典型用途就是定时中断和触发 DAC。

## 程序流程

```c
HAL_TIM_Base_Start_IT(&htim4);              // 以中断方式启动

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim == &htim4) {
        HAL_UART_Transmit_IT(&huart2, (uint8_t*)date, strlen(date));
    }
}
```

`date` 是一串 UTF-8 编码的 emoji（`😊😂🤣❤️😍😒`），每个字符占多个字节，`strlen` 得到的是字节数，正好是串口需要发送的长度。

## 现象

串口助手每秒收到一行 emoji。**串口助手需按 UTF-8 显示**，否则会是乱码（这是 emoji 多字节编码导致的，不是串口配置错）。

## 备注

- 主循环里的 `counter` / `message` 是调试残留，读到的计数值并未输出；定时完全靠中断驱动，与主循环无关。
- 中断里用 `_IT` 版本发送是为了不阻塞中断过久，但连续两次中断之间若上一笔还没发完，`HAL_UART_Transmit_IT` 会返回 `HAL_BUSY` 而丢数据。1 s 间隔下不会触发，改成几十毫秒级时要注意这个坑。
