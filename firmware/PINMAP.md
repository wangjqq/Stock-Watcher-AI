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

## 旋钮导航 + 返回键（第二阶段）

输入设备：EC11 / E8H6 等**带按键旋钮**（旋转 = 导航、按下 = 确认）+ 独立返回键。
旋钮引脚：`A  B  C(公共)  SW(按下开关)`，内部上拉；A/B 正交解码，SW 与返回键按下为低电平，软件消抖。
若旋转方向与预期相反，把 `knob.c` 顶部 `KNOB_REVERSE` 改为 1。

| 信号 | 功能 | ESP32 GPIO | 定义位置 |
| ---- | ---- | ---------- | -------- |
| A    | 旋钮相位 A | GPIO25 | `hardware/knob.c` → `KNOB_GPIO_A` |
| B    | 旋钮相位 B | GPIO26 | `KNOB_GPIO_B` |
| SW   | 旋钮按下 = 确认 | GPIO27 | `KNOB_GPIO_SW` |
| BACK | 返回键 | GPIO14 | `KNOB_GPIO_BACK` |

接线速查：

```text
旋钮 A  → GPIO25
旋钮 B  → GPIO26
旋钮 SW → GPIO27
旋钮 C  → GND
返回键   → GPIO14（另一脚 → GND）
```

## 无源蜂鸣器（第二阶段）

无源蜂鸣器由 LEDC PWM 驱动，可发不同音调。

| 引脚 | ESP32 GPIO | 定义位置 |
| ---- | ---------- | -------- |
| 蜂鸣器信号 | GPIO13 | `hardware/buzzer.c` → `PIN_BUZZER` |

```text
蜂鸣器信号 → GPIO13
```

## RGB LED（第二阶段，三引脚）

三路独立 PWM 调亮度，默认**共阴极**（高电平点亮）。

| 引脚 | ESP32 GPIO | 定义位置 |
| ---- | ---------- | -------- |
| R   | GPIO33 | `hardware/led.c` → `LED_GPIO_R` |
| G   | GPIO21 | `LED_GPIO_G` |
| B   | GPIO22 | `LED_GPIO_B` |

```text
R → GPIO33
G → GPIO21
B → GPIO22
```

> 若为共阳极 LED，把 `led.c` 顶部 `LED_ACTIVE_HIGH` 改为 `0` 即可（自动反相）。

## 修改接线

各外设的引脚定义都在自己模块的 `.c` 文件顶部宏里，改对应宏即可，不需要改其他文件：

```c
/* 屏幕 */
#define PIN_SCLK   GPIO_NUM_18
#define PIN_MOSI   GPIO_NUM_23
#define PIN_CS     GPIO_NUM_5
#define PIN_DC     GPIO_NUM_17
#define PIN_RST    GPIO_NUM_16
#define PIN_BL     GPIO_NUM_4

/* 旋钮 + 返回键 hardware/knob.c */
#define KNOB_GPIO_A    GPIO_NUM_25
#define KNOB_GPIO_B    GPIO_NUM_26
#define KNOB_GPIO_SW   GPIO_NUM_27
#define KNOB_GPIO_BACK GPIO_NUM_14

/* 蜂鸣器 hardware/buzzer.c */
#define PIN_BUZZER GPIO_NUM_13

/* RGB LED hardware/led.c */
#define LED_GPIO_R GPIO_NUM_33
#define LED_GPIO_G GPIO_NUM_21
#define LED_GPIO_B GPIO_NUM_22
```
