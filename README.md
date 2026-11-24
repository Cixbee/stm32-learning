# STM32F103 学习工程集合

STM32F103C8T6 最小系统板（Blue Pill）上的一组外设学习实验，全部用 **STM32CubeMX + HAL 库 + CMake** 组织，VS Code + `arm-none-eabi-gcc` 编译，ST-LINK 烧录。每个实验一个独立可编译的 CMake 工程，互不依赖。

## 工程一览

| 目录 | 实验内容 | 关键外设 / 引脚 | 现象 |
|---|---|---|---|
| [`pwm-light`](pwm-light) | TIM3 输出 PWM，占空比往复扫描 | TIM3_CH1 → `PA6` | LED 呼吸式渐亮渐暗 |
| [`tim-basic`](tim-basic) | TIM4 基础定时器，1 s 更新中断 | TIM4、USART2（`PA2`/`PA3`） | 串口每秒打印一串 emoji |
| [`tim-external-input`](tim-external-input) | TIM2 外部时钟模式 2，统计外部脉冲 | TIM2_ETR ← `PA0`、I2C1 OLED | OLED 实时显示脉冲计数值 |
| [`tim-slave-mode`](tim-slave-mode) | TIM2 从模式复位，外部触发清零计数器 | TIM2_CH1 ← `PA0`、USART2 | 串口打印「自动重装载」/「从模式触发」 |
| [`tim-input-capture`](tim-input-capture) | 输入捕获测脉宽，HC-SR04 超声波测距 | TIM1_CH3/CH4 ← `PA10`、TRIG → `PA11`、OLED | OLED 显示距离（cm） |
| [`tim-encoder`](tim-encoder) | 编码器接口模式计数 + PWM 联动 | TIM1_CH1/CH2 ← `PA8`/`PA9`、TIM3_CH1 → `PA6`、OLED | 旋钮转到哪，数字、进度条、LED 亮度就跟到哪 |
| [`tim-motor`](tim-motor) | DRV8833 电机驱动 + 编码器比例闭环 | TIM2_CH1/CH2 → `PA0`/`PA1`、编码器 `PA8`/`PA9` | 「力反馈旋钮」：编码器偏向哪边，电机就往哪边转 |
| [`tim-servo`](tim-servo) | 50 Hz 舵机 PWM + 编码器调角度 | TIM4_CH3 → `PB8`、编码器 `PA8`/`PA9` | 旋转编码器，舵机在 0–180° 之间跟随 |
| [`adc`](adc) | ADC 单通道轮询采样并换算电压 | ADC1_IN5 ← `PA5`、USART2 | 串口每 0.5 s 打印原始值与电压 |
| [`oled-ssd1306`](oled-ssd1306) | SSD1306 OLED 驱动库（帧缓冲 + 图形/字符） | I2C1：`PB6`(SCL) / `PB7`(SDA) | 提供画点/线/圆/字符串接口 |

## 硬件

- **MCU**：STM32F103C8T6，LQFP48，64 KB Flash / 20 KB RAM
- **时钟**：外部 8 MHz 晶振（HSE）→ PLL ×9 → **SYSCLK 72 MHz**；APB1 = 36 MHz（TIM2/3/4 定时器时钟经 ×2 后仍为 72 MHz），APB2 = 72 MHz
- **下载调试**：ST-LINK V2（SWD：`PA13` SWDIO / `PA14` SWCLK）
- **显示**：SSD1306 0.96" OLED，128×64，I2C 接口
- **测距**：HC-SR04 超声波模块（TRIG / ECHO）
- **电机**：DRV8833 双 H 桥驱动 + 带编码器的直流减速电机
- **舵机**：SG90 / MG996R 类 50 Hz PWM 舵机
- **输入**：增量式旋转编码器（EC11 等）、电位器、按键

## 工具链

- STM32CubeMX（各工程根目录的 `.ioc` 可直接打开重新生成代码）
- `arm-none-eabi-gcc`（见 `cmake/gcc-arm-none-eabi.cmake`）
- CMake ≥ 3.22 + Ninja
- VS Code（`.vscode/` 内有 C/C++ 配置；`.clangd` 指向 `build/Debug` 的编译数据库）

## 构建

每个工程独立构建：

```bash
cd pwm-light          # 换成任一工程目录
cmake --preset Debug
cmake --build --preset Debug
```

产物为 `build/Debug/<工程名>.elf`，同时可用 `arm-none-eabi-objcopy` 转出 `.hex` / `.bin`。

## 烧录

任选其一：

- STM32CubeProgrammer（连接 ST-LINK，选择 ELF/HEX 后下载）
- VS Code + Cortex-Debug，用 `st-util`/`stlink-gdb-server` 起 GDB Server 后按 `.vscode/launch.json` 调试
- `openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/Debug/pwm-light.elf verify reset exit"`

## 目录说明

```
.
├── pwm-light/             TIM3 PWM 呼吸灯
├── tim-basic/             TIM4 基础定时器 + UART 中断
├── tim-external-input/    TIM2 ETR 外部时钟计数 + OLED 显示
├── tim-slave-mode/        TIM2 从模式复位 + UART 打印
├── tim-input-capture/     TIM1 输入捕获 + HC-SR04 测距 + OLED 显示
├── tim-encoder/           TIM1 编码器接口 + TIM3 PWM 联动
├── tim-motor/             DRV8833 电机驱动 + 编码器比例闭环
├── tim-servo/             TIM4 50 Hz 舵机 PWM + 编码器调角
├── adc/                   ADC1 单通道采样 + 串口输出
└── oled-ssd1306/          SSD1306 OLED 驱动库
```

每个子目录都是一个完整工程，结构一致：

```
<工程>/
├── <工程>.ioc          CubeMX 工程文件
├── CMakeLists.txt      CMake 构建脚本
├── CMakePresets.json   Debug / Release 预设（Ninja）
├── Core/               main.c、外设初始化与用户代码
├── Drivers/            HAL 驱动与 CMSIS（CubeMX 生成）
├── cmake/              工具链与 CubeMX 生成的 CMake 片段
└── STM32F103xx_FLASH.ld
```

## 说明

- `@brief` 注释与 `/* USER CODE */` 分区保留 CubeMX 原始风格，`.ioc` 可继续用于二次生成（注意重新生成会覆盖 `USER CODE` 之外的改动）。
- 以下内容**未纳入版本管理**（见 `.gitignore`）：`build/`、`*.elf`/`*.bin`/`*.hex`/`*.map`、`Drivers/CMSIS/Lib` 下的预编译库（`*.lib`/`*.a`）。
- `Drivers/` 由 CubeMX 整包生成，各工程之间内容基本相同，仓库中由 Git 按内容去重，不会成倍占用体积。
