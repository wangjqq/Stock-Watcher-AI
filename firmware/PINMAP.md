# 硬件接线说明（PIN 映射）

> 目标芯片：**ESP32-S3 N16R8**（16MB Flash + 8MB Octal PSRAM）。
> 本文件是硬件接线的唯一说明，固件中的引脚定义见各模块 `.c` 文件顶部 `PIN_*` 宏，两者保持一致。
> 构建目标：`idf.py set-target esp32s3`。
>
> ⚠️ **引脚调整说明（2026-09-02）**：原固件把 LCD MOSI、旋钮 A、LED B 分别定在
> GPIO23 / GPIO25 / GPIO22，这三个引脚在 ESP32-S3 上并不存在（S3 仅有 GPIO0~21、26~48），
> 已按 S3 安全引脚重新分配为 **GPIO48 / GPIO47 / GPIO42**。若实物接线与上述不符，请以实际
> 接线为准，同步修改对应 `.c` 文件宏与本表。

## ⚠️ 引脚禁区（N16R8 必须避开）

N16R8 的 **8MB Octal PSRAM 与 Flash 通过 SPI 总线连接，占用 GPIO26~37**，这些引脚在模组内部已被占用，
**绝对不可用作 GPIO**（强行使用会导致系统不稳定 / 反复重启）：

- `GPIO26 ~ GPIO32`：SPI0/1 数据/时钟（Flash + PSRAM）
- `GPIO33 ~ GPIO37`：Octal 模式额外数据线（SPIIO4~7 / SPIDQS），其中 35/36/37 硬连接 PSRAM

其余注意项：

- Strapping 引脚：`GPIO0 / GPIO3 / GPIO45 / GPIO46`（上电决定启动模式，尽量避免做普通 IO）
- USB 引脚：`GPIO19 / GPIO20`（USB D-/D+，用作普通 IO 会禁用 USB-JTAG）
- ADC1 只在 `GPIO1 ~ GPIO10`；ADC2（GPIO11~20）在 WiFi 开启时不可用

---

## 屏幕：ILI9341 2.4" 240x320 + 触摸（XPT2046）

模块引脚：`VCC  GND  CS  RESET  DC/RS  SDI(MOSI)  SCK  LED  SDO(MISO)`
触摸引脚：`T_CLK  T_CS  T_DIN  T_DO  T_IRQ`

LCD 与触摸共用一条 SPI 总线（SPI2）：**T_CLK 与 SCK 连到同一个 GPIO，T_DIN 与 SDI(MOSI) 连到同一个 GPIO**。
`T_DO` 接共享 MISO（**LCD 的 SDO 可以不接**，写屏无需读回）；触摸 T_CS 独立片选。

| 模块引脚 | 功能         | ESP32 GPIO | 定义位置                                                   |
| -------- | ------------ | ---------- | ---------------------------------------------------------- |
| VCC      | 电源 3.3V    | 3V3        | 接开发板 3.3V                                              |
| GND      | 地           | GND        | 共地                                                       |
| CS       | LCD 片选     | GPIO5      | `display/display.c` → `PIN_CS`                             |
| RESET    | 复位         | GPIO16     | `PIN_RST`                                                  |
| DC       | 数据/命令    | GPIO17     | `PIN_DC`（也叫 RS/A0）                                     |
| SDI      | LCD 数据输入 | GPIO48     | `PIN_MOSI`                                                 |
| SCK      | 时钟         | GPIO18     | `PIN_SCLK`                                                 |
| LED      | 背光         | GPIO4      | `PIN_BL`，高电平点亮                                       |
| SDO      | LCD 数据输出 | 悬空       | 写屏无需读回，可不接                                       |
| T_CLK    | 触摸时钟     | GPIO18     | 与 SCK 共用 `PIN_SCLK`                                     |
| T_CS     | 触摸片选     | GPIO9      | `hardware/touch.c` → `TOUCH_PIN_CS`                        |
| T_DIN    | 触摸数据输入 | GPIO48     | 与 MOSI 共用 `PIN_MOSI`                                    |
| T_DO     | 触摸数据输出 | GPIO11     | 共享 MISO，`display.c` → `PIN_MISO`                        |
| T_IRQ    | 触摸中断     | GPIO15     | `hardware/touch.c` → `TOUCH_PIN_IRQ`，按下为低（内部上拉） |

接线速查（模块引脚 → ESP32）：

```text
VCC    → 3V3
GND    → GND
CS     → GPIO5
RESET  → GPIO16
DC     → GPIO17
SDI    → GPIO48
SCK    → GPIO18
LED    → GPIO4
SDO    → 悬空（可不接）
T_CLK  → GPIO18   （与 SCK 共用）
T_CS   → GPIO9
T_DIN  → GPIO48   （与 SDI 共用）
T_DO   → GPIO11   （共享 MISO）
T_IRQ  → GPIO15
```

> 注：不同批次 ILI9341 面板颜色/方向可能不同，若偏色或反色，参考
> `display.c` 中 `ili9341_init()` 末尾的注释（MADCTL / INVOFF 调整）。
>
> 触控为电阻式，首次使用需在「系统 → TouchCal」里做两角标定（点左上角、右下角），
> 标定数据保存在 NVS。若触摸方向左右颠倒，重复标定即可；若 X/Y 轴互换（少见），
> 需在 `touch.c` 的 `raw_to_screen()` 中交换两个通道。
>
> 触控手势（`touch.c` + `main.c` 的 `on_touch_ev`）：
>
> - 点按：菜单/系统菜单按行选中打开；应用内点状态栏最左返回按钮（‹）回列表，其余区域切下一个应用
> - 中部左右滑：菜单/系统菜单移动光标；应用内切换上一个/下一个应用
> - 边缘向内滑（从屏幕最左缘向右滑，或从最右缘向左滑）= 返回上一级
>
> 「系统 → WiFi」页整页为触摸操作：点列表行选择网络 / 点软键盘键输入密码，边缘向内滑返回
> （配网无 AP 模式，需在设备上手动选择网络并输密码）。

## 旋钮导航 + 返回键

输入设备：EC11 / E8H6 等**带按键旋钮**（旋转 = 导航、按下 = 确认）+ 独立返回键。
旋钮引脚：`A  B  C(公共)  SW(按下开关)`，内部上拉；A/B 正交解码，SW 与返回键按下为低电平，软件消抖。
若旋转方向与预期相反，把 `knob.c` 顶部 `KNOB_REVERSE` 改为 1。

> **深度睡眠唤醒**：SW（GPIO7）处于 RTC 域（GPIO0~21），同时作为深度睡眠唤醒键——
> 整机深度睡眠后按下旋钮即可随时唤醒（同键兼作确认，见 main.c 深度睡眠逻辑，无需额外接线）。

| 信号 | 功能            | ESP32 GPIO | 定义位置                          |
| ---- | --------------- | ---------- | --------------------------------- |
| A    | 旋钮相位 A      | GPIO47     | `hardware/knob.c` → `KNOB_GPIO_A` |
| B    | 旋钮相位 B      | GPIO6      | `KNOB_GPIO_B`（避开 26~37）       |
| SW   | 旋钮按下 = 确认 | GPIO7      | `KNOB_GPIO_SW`（避开 26~37）      |
| BACK | 返回键          | GPIO14     | `KNOB_GPIO_BACK`                  |

接线速查：

```text
旋钮 A  → GPIO47
旋钮 B  → GPIO6
旋钮 SW → GPIO7
旋钮 C  → GND
返回键   → GPIO14（另一脚 → GND）
```

## 无源蜂鸣器

无源蜂鸣器由 LEDC PWM 驱动，可发不同音调。

| 引脚       | ESP32 GPIO | 定义位置                           |
| ---------- | ---------- | ---------------------------------- |
| 蜂鸣器信号 | GPIO13     | `hardware/buzzer.c` → `PIN_BUZZER` |

```text
蜂鸣器信号 → GPIO13
```

## RGB LED（三引脚）

三路独立 PWM 调亮度，默认**共阴极**（高电平点亮）。

| 引脚 | ESP32 GPIO | 定义位置                                      |
| ---- | ---------- | --------------------------------------------- |
| R    | GPIO38     | `hardware/led.c` → `LED_GPIO_R`（避开 26~37） |
| G    | GPIO21     | `LED_GPIO_G`                                  |
| B    | GPIO42     | `LED_GPIO_B`                                  |

```text
R → GPIO38
G → GPIO21
B → GPIO42
```

> 若为共阳极 LED，把 `led.c` 顶部 `LED_ACTIVE_HIGH` 改为 `0` 即可（自动反相）。

## 光敏传感器 BH1750（I2C）

| 引脚 | ESP32 GPIO | 定义位置                                            |
| ---- | ---------- | --------------------------------------------------- |
| VCC  | 3V3        | -                                                   |
| GND  | GND        | -                                                   |
| SDA  | GPIO8      | `hardware/light_sensor.c` → `PIN_SDA`（避开 26~37） |
| SCL  | GPIO10     | `PIN_SCL`（避开 26~37）                             |
| ADDR | 悬空       | 地址 0x23                                           |

```text
VCC → 3V3
GND → GND
SDA → GPIO8
SCL → GPIO10
```

## 电池电量（ADC 分压采样）

| 引脚  | ESP32 GPIO              | 说明                            |
| ----- | ----------------------- | ------------------------------- |
| 电池+ | ── R1(100K) ──┬── GPIO1 | GPIO1 = ADC1_CH0                |
| 电池- | ── R2(100K) ──┘         | 分压比 2:1，3.0~4.2V → 1.5~2.1V |

```text
电池+ ──R1(100K)──┬── GPIO1
                  │
电池- ──R2(100K)──┘
```

> S3 的 ADC1 通道仅在 GPIO1~10；GPIO1 = ADC1_CH0。分压后电压须落在
> ADC_ATTEN_DB_12 量程（0~3.1V）内。

## 修改接线

各外设的引脚定义都在自己模块的 `.c` 文件顶部宏里，改对应宏即可，不需要改其他文件：

```c
/* 屏幕 + 触摸 display/display.c + hardware/touch.c */
#define PIN_SCLK   GPIO_NUM_18
#define PIN_MOSI   GPIO_NUM_48
#define PIN_MISO   GPIO_NUM_11   /* 触摸 T_DO */
#define PIN_CS     GPIO_NUM_5
#define PIN_DC     GPIO_NUM_17
#define PIN_RST    GPIO_NUM_16
#define PIN_BL     GPIO_NUM_4
#define TOUCH_PIN_CS   GPIO_NUM_9
#define TOUCH_PIN_IRQ  GPIO_NUM_15

/* 旋钮 + 返回键 hardware/knob.c */
#define KNOB_GPIO_A    GPIO_NUM_47
#define KNOB_GPIO_B    GPIO_NUM_6
#define KNOB_GPIO_SW   GPIO_NUM_7
#define KNOB_GPIO_BACK GPIO_NUM_14

/* 蜂鸣器 hardware/buzzer.c */
#define PIN_BUZZER GPIO_NUM_13

/* RGB LED hardware/led.c */
#define LED_GPIO_R GPIO_NUM_38
#define LED_GPIO_G GPIO_NUM_21
#define LED_GPIO_B GPIO_NUM_42

/* 光敏 BH1750 hardware/light_sensor.c */
#define PIN_SDA GPIO_NUM_8
#define PIN_SCL GPIO_NUM_10

/* 电池电量 hardware/battery.c */
#define PIN_BATT ADC1_CHANNEL_0  /* GPIO1 */
```
