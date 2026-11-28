# calendar —— RTC 电子日历 + 旋钮设置

## 实验目的

用 STM32F103 内置的 **RTC（实时时钟）+ LSE 32.768 kHz 晶振**做一个走时准确的电子日历，OLED 显示年月日、时分秒和星期，用旋转编码器当「设置旋钮」调整时间。这个工程把这批实验里学的东西串起来了：RTC、I2C OLED、编码器、按键消抖、状态机。

## 硬件连接

| 信号 | 引脚 | 说明 |
|---|---|---|
| LSE 晶振 | `PC14` / `PC15` | 32.768 kHz 晶振（RTC 走时用，**必须焊**，否则退回内部 LSI 会一天差几分钟） |
| OLED SCL / SDA | `PB6` / `PB7` (I2C1) | SSD1306 128×64 |
| 编码器 A / B | `PA8` / `PA9` (TIM1) | 增量式编码器 |
| 旋钮按键 | `PB15` | 编码器自带的按钮，按下为低电平 |
| UART2 | `PA2` / `PA3` | 115200（本工程仅保留，未实际输出） |
| VBAT | — | 接纽扣电池才能掉电继续走时 |

## 目录分层

```
calendar/
├── Lib/                 可复用组件
│   ├── Src/oled.c       SSD1306 驱动（同 oled-ssd1306）
│   ├── Src/font.c       字库
│   ├── Src/lcc_rtc.c    RTC 封装：time_t ↔ struct tm
│   └── Src/lcc_knob.c   编码器 + 按键消抖 + 回调
└── App/
    └── Src/task_main.c  界面状态机与显示
```

## RTC 封装（`lcc_rtc.c`）

思路很直接：**RTC 只存一个 32 位秒计数（Unix 时间戳）**，读写时用标准库转换。

```c
HAL_StatusTypeDef LCC_RTC_SetTime(struct tm *time) {
    uint32_t unixTime = mktime(time);            // struct tm → 时间戳
    return RTC_WriteTimeCounter(&hrtc, unixTime);
}

struct tm *LCC_RTC_GetTime() {
    time_t unixTime = RTC_ReadTimeCounter(&hrtc); // 时间戳 → struct tm
    return gmtime(&unixTime);
}
```

`RTC_WriteTimeCounter` 里先进入 RTC 初始化模式（关写保护）、写 CNTH/CNTL、再退出，这是 F1 系列 RTC 的固定流程。

**首次上电才初始化**，靠后备寄存器 DR1 里的魔数 `0x2333` 判断：

```c
uint32_t initFlag = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1);
if (initFlag == RTC_INIT_FLAG) return;      // 已经初始化过，保留走时

/* 否则设为 2025-01-01 23:59:55，并写入魔数 */
```

后备域由 VBAT 供电，所以只要电池在，复位、断电都不会清掉时间。

## 旋钮封装（`lcc_knob.c`）

编码器用 TIM1 的硬件编码器接口，每次 `Knob_Loop()` 读一次计数值，和「中点」32767 比较来判断方向，然后立刻把计数器写回中点：

```c
void Knob_Loop() {
    uint32_t counter = getCounter();
    if (counter > COUNTER_INIT_VALUE)       onForwardCallback();
    else if (counter < COUNTER_INIT_VALUE)  onBackwardCallback();
    setCounter(COUNTER_INIT_VALUE);         // 每次归零，下次重新判断
}
```

好处是**不需要知道转了多少格**，只关心「这次是正转还是反转」，适合一格一步的菜单/数值调节。按键用 `HAL_GetTick()` 做 10 tick 消抖，只在按下瞬间回调一次。

对外只暴露三个回调注册接口，业务层不关心底层是编码器还是按键：

```c
Knob_SetForwardCallback(onKnobForward);
Knob_SetBackwardCallback(onKnobBackward);
Knob_SetPressedCallback(onKnobPressed);
```

## 状态机

两个状态、六个可调字段：

```
Normal ──按下──> Setting（光标停在"年"）
                  │ 旋转 → 修改当前字段
                  │ 按下 → 切到下一个字段（年→月→日→时→分→秒）
                  └ 在"秒"上按下 → 写回 RTC，回到 Normal
```

显示分三行：`2026-11-28`（16×8 字库）、`15:49:43`（24×12 字库）、`Friday` 居中。设置模式下，当前字段下方有 500 ms 闪烁的下划线，位置由 `cursorPosition[6]` 坐标表给出。

## 现象

上电显示 RTC 当前时间并逐秒走动。按一下旋钮进入设置模式，年份下出现闪烁下划线；旋转改数值，每按一次切换到下一个字段；在「秒」上按下把时间写回 RTC，退回正常显示。

## 备注

- **小时 23 设不出来**：`if (settingTime.tm_hour >= 23) tm_hour = 0;` 用的是 `>=`，加到 23 就被归零了，实际最大只能设到 22 点。应改成 `> 23`。
- **日期不做真实校验**：`tm_mday` 只判断 `> 31`，2 月能设到 30、31 号，写进 RTC 后会溢出成下个月。严谨做法是用 `mktime` 归一化（`mktime` 会自动把非法日期进位），或者手工按月份和闰年校验。
- `gmtime()` 返回的是 **UTC**，不是本地时间。裸机 newlib 的时区默认是 UTC，所以和 `mktime()` 能对上；一旦有人设了 `TZ` 环境变量，两者就不对称了，会差 8 小时。想显示北京时间，最稳的是在显示时手动 `+8` 小时并处理跨天。
- `LCC_RTC_GetTime()` 返回的是 newlib 内部静态缓冲区指针，**连续调用会互相覆盖**。本工程一次只用一次所以没事，但如果同时要比较两个时间就会踩坑。
- 修改时间后记得重新 `LCC_RTC_SetTime`，否则只改了内存里的 `settingTime`，RTC 还在按老时间走。
