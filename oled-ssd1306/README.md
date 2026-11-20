# oled-ssd1306 —— SSD1306 OLED 驱动

## 实验目的

为 0.96" SSD1306 OLED（128×64，I2C 接口）写一套可复用的驱动：内部维护一块显存（帧缓冲），所有绘图先写内存，调用 `OLED_ShowFrame()` 时一次性刷到屏幕。`tim-external-input` 和 `tim-input-capture` 两个实验都用它做显示。

## 硬件连接

| 信号 | 引脚 | 说明 |
|---|---|---|
| SCL | `PB6` (I2C1_SCL) | 需接 4.7 kΩ 上拉到 3.3 V（多数模块板载） |
| SDA | `PB7` (I2C1_SDA) | 同上 |

I2C1 配置为 **400 kHz、7 位地址**。

## 驱动地址

`oled.c` 中 `#define OLED_ADDRESS 0x7A`，这是**含读写位**的 8 位写法，对应 7 位地址 `0x3D`。SSD1306 常见地址还有 `0x78`（7 位 `0x3C`）——**点不亮时先确认模块实际地址**：

```c
#define OLED_ADDRESS 0x78   // 多数 0.96" 模块是这个
```

用逻辑分析仪或 I2C 扫描程序扫一下总线上出现的地址，比反复试更快。

## 文件构成

| 文件 | 内容 |
|---|---|
| `Core/Inc/oled.h` / `Core/Src/oled.c` | 驱动本体：初始化、显存、绘图、字符输出 |
| `Core/Inc/font.h` / `Core/Src/font.c` | ASCII 字库 `afont8x6` / `afont12x6` / `afont16x8` / `afont24x12`，以及 16×16 中文字库 `font16x16` |

## 主要接口

```c
void OLED_Init(void);
void OLED_DisPlay_On(void);
void OLED_DisPlay_Off(void);

void OLED_NewFrame(void);                                   // 清空帧缓冲
void OLED_ShowFrame(void);                                  // 刷到屏幕

void OLED_SetPixel(uint8_t x, uint8_t y, OLED_ColorMode color);
void OLED_DrawLine(...);
void OLED_DrawRectangle(...);  void OLED_DrawFilledRectangle(...);
void OLED_DrawTriangle(...);   void OLED_DrawFilledTriangle(...);
void OLED_DrawCircle(...);     void OLED_DrawFilledCircle(...);
void OLED_DrawEllipse(...);
void OLED_DrawImage(uint8_t x, uint8_t y, const Image *img, OLED_ColorMode color);

void OLED_PrintASCIIChar(...);      // 单个 ASCII 字符
void OLED_PrintASCIIString(...);    // ASCII 字符串
void OLED_PrintString(uint8_t x, uint8_t y, char *str, const Font *font, OLED_ColorMode color);  // 支持 UTF-8 中文
```

`OLED_ColorMode` 两个取值：`OLED_COLOR_NORMAL`（点亮）/ `OLED_COLOR_REVERSE`（反色）。

## 示例

本工程 `main.c` 是一个驱动自测程序：以屏幕中心 `(64, 32)` 为圆心，同时画半径 `i`、`2i`、`3i` 三个同心圆，`i` 从 0 递增到 63，效果是一组不断扩散的同心圆动画。

```c
OLED_Init();
while (1) {
    for (uint8_t i = 0; i < 64; i++) {
        OLED_NewFrame();
        OLED_DrawCircle(64, 32, i,   OLED_COLOR_NORMAL);
        OLED_DrawCircle(64, 32, 2*i, OLED_COLOR_NORMAL);
        OLED_DrawCircle(64, 32, 3*i, OLED_COLOR_NORMAL);
        OLED_ShowFrame();
    }
}
```

## 备注

- 显示中文要用 `OLED_PrintString` + `&font16x16`，源文件必须是 UTF-8 编码；用 `OLED_PrintASCIIString` 只会取出每个汉字的首字节，显示乱码。
- `OLED_NewFrame()` + 多次绘图 + `OLED_ShowFrame()` 的「双缓冲」用法能避免闪烁，但每次全屏刷新要传 1024 字节（8 页 × 128 列），400 kHz 下约 25 ms。刷新频繁时注意这个耗时。
- 初始化后 `HAL_Delay(20)` 是等 OLED 上电稳定，去掉偶发花屏。
