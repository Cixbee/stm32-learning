# cyclic-buffer —— 串口指令环形缓冲区

## 实验目的

串口是**逐字节、异步**到达的，而业务逻辑要的是**一整帧**。如果每来一个字节就中断里解析，主循环会被撕裂；如果在主循环里阻塞等一帧，又会丢数据。标准解法是中间加一层**环形缓冲区（ring / circular buffer）**：中断只管往里塞，主循环只管从里取。

本实验就是单独把这层抽出来做透——[`colorful-lights`](../colorful-lights) 的 `Lib/command.c` 就是从这里演化过去的。

## 硬件连接

只有串口，无其他外设：

| 信号 | 引脚 | 说明 |
|---|---|---|
| UART2_TX / RX | `PA2` / `PA3` | 接 USB-TTL，**115200, 8N1** |

## 环形缓冲的实现要点

```c
#define BUFFER_SIZE 128
uint8_t buffer[BUFFER_SIZE];
uint8_t readIndex, writeIndex;

/* 读取第 i 个字节，越界自动绕回 */
uint8_t Command_Read(uint8_t i)              { return buffer[i % BUFFER_SIZE]; }

/* 未处理数据长度：用取模差值，天然处理绕回 */
uint8_t Command_GetLength()                  { return (writeIndex + BUFFER_SIZE - readIndex) % BUFFER_SIZE; }

/* 剩余空间 */
uint8_t Command_GetRemain()                  { return BUFFER_SIZE - Command_GetLength(); }
```

写入时如果数据会跨越缓冲区尾部，就**分成两段 `memcpy`**，第二段写回开头：

```c
uint8_t firstLength = BUFFER_SIZE - writeIndex;
memcpy(buffer + writeIndex, data, firstLength);
memcpy(buffer, data + firstLength, length - firstLength);
writeIndex = length - firstLength;
```

`Command_GetLength` 用 `(w + N - r) % N` 而不是 `w - r`，是为了让反转后仍是正数（`uint8_t` 相减会下溢）。这是环形缓冲最容易写错的一行。

## 帧解析

`Command_GetCommand()` 一直循环到缓冲区里凑不出一帧为止：

1. 剩余长度 < 4 → 返回 0（最小帧长）
2. 当前字节不是包头 `0xAA` → **只前移 1 字节**，继续找
3. 读长度字段，缓冲区里的数据不够一整帧 → 返回 0，等下次
4. 累加前 `length-1` 个字节，与最后一个字节比对；不符 → 前移 1 字节，继续找
5. 全部通过 → 拷贝出这一帧，读指针前移 `length`，返回帧长

第 2、4 步「失败只前移 1 字节」是关键：这保证了即使收到半帧、噪声或错位数据，解析器也能自己重新对齐，不需要额外的状态机变量。

## 中断与主循环的分工

```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    Command_Write(readBuffer, Size);                   // 中断里只做拷贝
    HAL_UARTEx_ReceiveToIdle_IT(&huart2, readBuffer, sizeof(readBuffer));
}

while (1) {
    commandLength = Command_GetCommand(command);       // 主循环里解析
    if (commandLength != 0) {
        HAL_UART_Transmit(&huart2, command, commandLength, HAL_MAX_DELAY);
    }
    HAL_Delay(1000);
}
```

中断里只把字节塞进缓冲区（快进快出），解析和业务都放主循环，这就是「生产者—消费者」模型。

## 现象

用串口助手发送一帧合法数据，设备会把**同样的字节原样回显**。

合法的 5 字节帧（长度字段必须等于总字节数）：

```
AA 05 01 02 B2
```

校验和 = (0xAA + 0x05 + 0x01 + 0x02) & 0xFF = 0xB2。

## 备注

- **`HAL_Delay(1000)` 会让回显延迟整整 1 秒**。发完帧 1 秒内看不到回复不是没收到，是主循环还在睡。调试时改成 10–50 ms 体验完全不同（这行应该是早期测试残留）。
- `readBuffer[10]` 限制了单次 Idle 接收最多 10 字节。帧更长时会触发多次回调，分片后仍会被拼进环形缓冲正常解析；但如果一帧 30 字节，中间有停顿被判定为 Idle，就会分成多段进缓冲——对本协议无害，可对依赖「一次收全」的协议就有问题。
- 当前实现**没有区分「满」和「空」**：`writeIndex == readIndex` 既可能是空也可能是满。代码靠「剩余空间不足就不写、返回 0」来规避，代价是会静默丢帧。更严谨的写法是留一个空位（容量 127）或单独一个 `count` 变量。
- `command.c` 里注释掉的旧版 `Command_GetLength`（用 if/else 逐种情况判断）是很多人第一次写环形缓冲时的版本，分支多且容易漏判，和现在的一行取模对比一下很清楚为什么要改。
