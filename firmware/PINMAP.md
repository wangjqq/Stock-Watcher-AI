# 硬件接线说明（PIN 映射）

> 本文件是硬件接线的唯一说明，固件中的引脚定义见
> `firmware/main/display/display.c` 顶部 `PIN_*` 宏，两者保持一致。

## 屏幕：ST7735 1.8" 128x160（8pin）

模块 8 个引脚：`VCC  GND  CS  RESET  DC  SDI(MOSI)  SCK  LED`

| 模块引脚 | 功能 | ESP32 GPIO | 说明 |
| ------- | ---- | ---------- | ---- |
| VCC     | 电源 3.3V | 3V3      | 接开发板 3.3V |
| GND     | 地 | GND       | 共地 |
| CS      | 片选 | GPIO5    | `PIN_CS` |
| RESET   | 复位 | GPIO16   | `PIN_RST` |
| DC      | 数据/命令 | GPIO17 | `PIN_DC`（也叫 A0） |
| SDI     | 数据输入 | GPIO23  | `PIN_MOSI` |
| SCK     | 时钟 | GPIO18    | `PIN_SCLK` |
| LED     | 背光 | GPIO4     | `PIN_BL`，高电平点亮 |

接线速查（模块引脚 → ESP32）：

```text
VCC   → 3V3
GND   → GND
CS    → GPIO5
RESET → GPIO16
DC    → GPIO17
SDI   → GPIO23
SCK   → GPIO18
LED   → GPIO4
```

> 注：不同批次 ST7735 面板颜色/方向可能不同，若偏色或反色，参考
> `display.c` 中 `st7735_init()` 末尾的注释（MADCTL / INVON 调整）。

## 修改接线

直接改 `firmware/main/display/display.c` 顶部宏即可，不需要改其他文件：

```c
#define PIN_SCLK   GPIO_NUM_18
#define PIN_MOSI   GPIO_NUM_23
#define PIN_CS     GPIO_NUM_5
#define PIN_DC     GPIO_NUM_17
#define PIN_RST    GPIO_NUM_16
#define PIN_BL     GPIO_NUM_4
```
