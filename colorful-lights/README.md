# colorful-lights —— 10 颗 WS2812 + 串口指令控制

## 实验目的

在 [`ws2812`](../ws2812) 的基础上做工程化改造：把「单帧硬编码」变成「颜色数组 + 可重复刷新」，并加上 **UART 指令协议 + 环形缓冲区**，让你可以从电脑串口发一帧数据控制 10 颗灯的颜色。工程按 `Lib`（可复用组件）/ `App`（业务逻辑）分层，是这批实验里第一个有多文件架构的项目。

## 硬件连接

| 信号 | 引脚 | 说明 |
|---|---|---|
| 数据 | `PB4` (TIM3_CH1) | 接 10 颗 WS2812B 灯带的 DIN |
| UART2_TX / RX | `PA2` / `PA3` | 接 USB-TTL，**115200, 8N1** |
| 电源 | — | 灯带 5 V 独立供电，与 MCU 共地 |

## 目录分层

```
colorful-lights/
├── Lib/                可复用组件（与业务无关）
│   ├── Inc/ws2812.h    WS2812 驱动接口
│   ├── Src/ws2812.c    颜色数组 → 位流 → PWM+DMA
│   ├── Inc/command.h   指令缓冲区接口
│   └── Src/command.c   128 字节环形缓冲 + 帧解析
├── App/                业务逻辑
│   ├── Inc/task_light.h
│   └── Src/task_light.c  解析指令 → 点亮灯带
└── Core/               CubeMX 生成的 HAL 初始化
```

## WS2812 驱动改进

相比 `ws2812` 的单帧数组，这里把颜色放在 `color[LED_COUNT][3]`，`Update()` 时才展开成位流：

```c
#define LED_COUNT 10
uint8_t color[LED_COUNT][3];      // [i][0]=R, [1]=G, [2]=B

void WS2812_Set(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void WS2812_SetAll(uint8_t r, uint8_t g, uint8_t b);
void WS2812_Update();             // 展开 240 bit + 1 个复位位 → 241 个码元
```

`Update()` 里先 `HAL_TIM_PWM_Stop_DMA()` 并清零计数器，再 `Start_DMA()`，因此可以反复刷新，不会累积相位误差。注意写入顺序是 **G 在前**（`data[24*i + j]` 放 G 的位），符合 WS2812B 的 GRB 顺序。

## 串口指令协议

| 字节 | 含义 |
|---|---|
| 0 | 包头 `0xAA` |
| 1 | **总长度**（含包头与校验和） |
| 2 | 模式，`0x01` = Normal（按 RGB 三元组设置） |
| 3 … n-2 | 每颗灯 3 字节，顺序 **R, G, B** |
| n-1 | 校验和 = 前面所有字节求和取低 8 位 |

`task_light.c` 的处理逻辑：`colorCount = (length - 4) / 3`，帧里给了几颗就设几颗，不够的用第 1 颗的颜色兜底，然后 `WS2812_Update()`。

**示例帧**（10 颗全绿）：

```
AA 22 01  00 FF 00  00 FF 00  ... (共 10 组)  C3
```

长度 `0x22` = 1+1+1+30+1 = 34；校验和 = (0xAA + 0x22 + 0x01 + 10×0xFF) & 0xFF = 0xC3。

## 接收与解析流程

```c
HAL_UARTEx_ReceiveToIdle_IT(&huart2, readBuffer, sizeof(readBuffer));   // 空闲中断接收

void HAL_UARTEx_RxEventCallback(...) {
    Command_Write(readBuffer, Size);                  // 收多少写多少进环形缓冲
    HAL_UARTEx_ReceiveToIdle_IT(...);                 // 重新武装
}

while (1) { LightTaskLoop(); }                        // 主循环里取帧并刷新灯带
```

环形缓冲的核心是一个取模的读写指针：`Command_GetLength() = (writeIndex + 128 - readIndex) % 128`。取帧时先找 `0xAA`，再校验长度和校验和，任何一个不对就**只前移 1 字节**继续找——这样即使收到半帧或噪声，也能自动重新同步。

## 现象

上电时前 3 颗灯暗红闪 500 ms 后熄灭（自检，看到闪一下说明 TIM3+DMA+DIO 通路正常）。之后用串口助手按上面的帧格式发送，10 颗灯立即变成指定颜色。

## 备注

- **帧里至少带一组 RGB**。如果发 `AA 04 01 xx`（length=4），`colorCount = 0`，代码会走到 `offset = 3` 去读第 4 个字节——那是校验和，会被当成颜色用。
- 校验和用的是「累加和」而非 CRC，抗干扰能力弱，但实现简单、单片机上够用。工业协议一般用 CRC16。
- `readBuffer[128]` 配合 `ReceiveToIdle` 是好的做法：一次最多收 128 字节，空闲即回调。但 `Command_Write` 在剩余空间不足时会**整帧丢弃并返回 0**，主循环又没检查返回值，所以高频发帧时可能静默丢数据——加个返回值判断或扩大缓冲即可。
- 刷新 10 颗灯约 240 × 1.25 µs ≈ 300 µs，`HAL_TIM_PWM_Start_DMA` 是异步的，函数立刻返回，实际传输在后台进行。若紧接着又调一次刷新，必须先 Stop，否则数据会打架——`Update()` 里已经处理了。
