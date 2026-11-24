# tim-motor —— DRV8833 电机驱动 + 编码器闭环

## 实验目的

用 TIM2 的两路 PWM 驱动 DRV8833 双 H 桥控制直流电机正反转与调速，同时用 TIM1 的编码器接口读取电机轴上的编码器位置，构成一个「力反馈旋钮」式的比例闭环：编码器偏离中心多少，电机就按比例往对应方向转多快。

## 硬件连接

| 信号 | 引脚 | 说明 |
|---|---|---|
| DRV8833 IN1 | `PA0` (TIM2_CH1) | H 桥 A 路输入 1 |
| DRV8833 IN2 | `PA1` (TIM2_CH2) | H 桥 A 路输入 2 |
| 编码器 A / B | `PA8` / `PA9` (TIM1_CH1/CH2) | 接电机尾部编码器 |
| 电源 | — | DRV8833 的 VM 接电机电源（2.7–10.8 V），VCC 接 3.3 V，**必须与 MCU 共地** |

## 关键配置

| 项目 | 值 | 说明 |
|---|---|---|
| TIM2 `PSC` / `ARR` | 72 - 1 / 100 - 1 | **10 kHz** PWM，CCR 0–100 对应 0–100% 占空比 |
| TIM2 通道 | CH1 / CH2 均为 PWM1 模式 | 双 PWM 控制，可做慢衰减/快衰减 |
| TIM1 模式 | 编码器接口 `TIM_ENCODERMODE_TI1` | 硬件判向计数 |
| TIM1 `ARR` | 65535 | 16 位 |
| `COUNT_MID` | 20 | 编码器「中心点」，有效行程 0–40 |

## 驱动接口（`drv8833.c`）

```c
void DRV8833_Init(void);
void DRV8833_SetDecayMode(DecayMode mode);   // SLOW_DECAY / FAST_DECAY
void DRV8833_Forward(uint8_t speed);         // speed: 0–100
void DRV8833_Backward(uint8_t speed);
void DRV8833_Brake(void);                    // 刹车（两端同时导通）
void DRV8833_Coast(void);                    // 惰行（两端同时关断）
```

两种衰减模式的区别：

| 模式 | IN1 | IN2 | 特点 |
|---|---|---|---|
| `SLOW_DECAY` | PWM | 0 | 电流在低边续流衰减慢，低速平稳、噪声小 |
| `FAST_DECAY` | 100% | 100% − speed | 电流经高边续流衰减快，响应快但纹波大 |

## 程序流程

```c
DRV8833_Init();
HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
__HAL_TIM_SET_COUNTER(&htim1, COUNT_MID);       // 编码器从中点开始

while (1) {
    count = __HAL_TIM_GET_COUNTER(&htim1);
    if (count > 6000)            { count = 0;        __HAL_TIM_SET_COUNTER(&htim1, 0); }
    else if (count > COUNT_MID*2) { count = COUNT_MID*2; __HAL_TIM_SET_COUNTER(&htim1, COUNT_MID*2); }

    if (count < COUNT_MID) {                     // 偏离中心的两侧
        speed = (COUNT_MID - count) * 100 / COUNT_MID;
        DRV8833_Backward(speed);
    } else {
        speed = (count - COUNT_MID) * 100 / COUNT_MID;
        DRV8833_Forward(speed);
    }
    HAL_Delay(100);
}
```

## 现象

上电时电机静止（计数器被置为 20）。用手转动编码器轴：偏向一侧电机就朝那个方向转，偏得越多转速越快；把轴拨回中心附近，电机停下。松手后因惯性和比例控制的作用，轴会来回摆几下才稳定，像在拧一个有阻尼的旋钮。

## 备注

- **小心正反馈**。代码是「编码器在左 → 反转，在右 → 正转」，实际方向由电机接线和编码器 A/B 相序共同决定。如果上电后电机自己越转越快、根本停不下来，就是接成了正反馈：把电机的两根线对调，或把编码器 A/B 对调，二者只改一个。
- 这里只有**比例（P）控制**，没有积分和微分，所以存在稳态误差且会有振荡。把它换成位置式 PID（目标 = `COUNT_MID`）就是标准的电机位置闭环，`HAL_Delay(100)` 的 100 ms 也要改小到 10 ms 以内。
- 电机电源与逻辑电源分开供电，否则电机启动瞬间的电流跌落会让 MCU 复位。DRV8833 的 nSLEEP 需拉高才工作（模块板上通常已处理）。
- 10 kHz PWM 是 DRV8833 的合适区间（手册建议 20 kHz 以上可避开音频噪声，但 10 kHz 在可听范围内，低速时可能听到轻微啸叫）。
- `DRV8833_Init()` 内部会启动 TIM2 的 PWM 输出，因此不需要在 `main()` 里再调 `HAL_TIM_PWM_Start()`。
