# ESP32-S3 NANO 开发板资料

> 本目录存放项目主控开发板 **芯璐电子 ESP32-S3 NANO**（兼容立创开源 ESP32S3）的官方资料，
> 供固件开发时核对引脚与硬件规格。图片与原理图来自官方资料页，引脚表由原理图核实。

## 一、基本信息

| 项目 | 说明 |
| ---- | ---- |
| 型号 | ESP32-S3 NANO 开发板 |
| 主控 | ESP32-S3 **N16R8**（16MB Flash + 8MB Octal PSRAM） |
| 供电 | Type-C 5V 输入，板上 800mA LDO 稳压 3.3V |
| 编程/调试 | Type-C → GPIO19/20（USB-Serial-JTAG） |
| 官方资料页 | https://www.xinlucity.com/?s=resourcedetail/index/id/114.html |
| 在线文档 | https://lceda002.feishu.cn/docx/KVMwd9aYWoSzyHxys1ScuUvmnrw |
| 兼容参考（立创开源） | https://lceda001.feishu.cn/wiki/PICgwjcDsiN7TjkWw5tc3OzbnHb |

> 型号说明：**N16** = 16MB Flash（程序存储），**R8** = 8MB PSRAM（运行内存，内置）。
> 板上预留 PSRAM 焊盘出厂不焊接（内置 8MB 已够用）。

## 二、板载外设（占用固定引脚）

| 外设 | 引脚 | 说明 |
| ---- | ---- | ---- |
| 板载 LED | **GPIO48** | 低电平点亮（`bsp_led.h`: `LED_PIN 48`） |
| 板载按键 | **GPIO0** | 按下为低电平（`bsp_key.h`: `KEY_PIN 0`） |
| USB | GPIO19 / GPIO20 | Type-C，D-/D+，不可复用为普通 IO |

> ⚠️ 本项目把 **GPIO48 用作 LCD MOSI**，因此板载 LED 无法再使用；
> 项目已用独立 RGB LED（GPIO38/21/42），无影响。

## 三、排针引脚（从原理图核实）

排针两排引出的 GPIO 全集（去重）：

```
GPIO 1   2   3   4   5   6   7   8   9   10  11  12  13  14  15
GPIO 16  17  18  19  20  21
GPIO 33  34  35  36  37
GPIO 38  39  40  41  42  45  46  47  48
```

电源/地：`3V3`、`GND`、`+5V`（多组）。`GPIO43/44`（U0TXD/RXD）未引出。

### 引脚禁区（不可使用 / 需避开）

| 引脚 | 原因 |
| ---- | ---- |
| GPIO26 ~ 32 | 内部 Flash + Octal PSRAM 的 SPI 总线（模组内部占用，未引出） |
| GPIO33 ~ 37 | **R8 内部 Octal PSRAM 使用**（原理图标注“R8型号则内部PSRAM使用”） |
| GPIO19 / 20 | USB Type-C（USB-Serial-JTAG） |
| GPIO45 / 46 | Strapping 引脚（上电决定启动模式，尽量避开） |
| GPIO0 / 3 | Strapping 引脚（GPIO0 已作板载按键） |
| GPIO48 | 与板载 LED 共用；本项目用作 LCD MOSI 时板载 LED 失效 |

> 实测可用排针引脚：GPIO1~18、GPIO21、GPIO38~42、GPIO47、GPIO48（避开上表）。

## 四、本项目引脚对照检查（2026-09-03 核对）

全部引脚均已在排针引出且**不落入禁区**，与开发板相符：

| 外设 | GPIO | 引出 | 可用 | 说明 |
| ---- | ---- | ---- | ---- | ---- |
| LCD CS | 5 | ✅ | ✅ | |
| LCD RST | 16 | ✅ | ✅ | |
| LCD DC | 17 | ✅ | ✅ | |
| LCD MOSI | 48 | ✅ | ✅ | 与板载 LED 共用（已说明） |
| LCD SCLK | 18 | ✅ | ✅ | |
| LCD BL | 4 | ✅ | ✅ | |
| 触摸 T_CS | 9 | ✅ | ✅ | |
| 触摸 T_DO | 11 | ✅ | ✅ | |
| 触摸 T_IRQ | 15 | ✅ | ✅ | |
| 旋钮 A | 47 | ✅ | ✅ | |
| 旋钮 B | 6 | ✅ | ✅ | |
| 旋钮 SW（深睡唤醒） | 7 | ✅ | ✅ | GPIO7 属 RTC 域，深睡可唤醒 |
| 返回键 | 14 | ✅ | ✅ | |
| 蜂鸣器 | 13 | ✅ | ✅ | |
| RGB R | 38 | ✅ | ✅ | |
| RGB G | 21 | ✅ | ✅ | |
| RGB B | 42 | ✅ | ✅ | |
| BH1750 SDA | 8 | ✅ | ✅ | |
| BH1750 SCL | 10 | ✅ | ✅ | |
| 电池 ADC | 1 | ✅ | ✅ | ADC1_CH0 |

**结论：项目所用 20 个 GPIO 全部符合开发板排针引出，无需改动。**

## 五、资料文件清单

| 文件 | 说明 |
| ---- | ---- |
| `pinout.jpg` | 引脚定义图（官方，5.9MB） |
| `dimension.png` | 尺寸图 |
| `components.jpg` | 元件分布图 |
| `schematic.pdf` | 原理图（单页，含芯片/排针/电源） |
| `esp-idf-example/` | 官方 ESP-IDF 例程源码（已清理编译产物，见下） |

## 六、ESP-IDF 例程索引（esp-idf-example/03.ESP-IDF/）

官方配套例程（编译环境：工程路径不能含中文）：

| 例程 | 内容 |
| ---- | ---- |
| 01_led | 板载 LED（GPIO48）点亮/熄灭 |
| 02_delay | 延时 |
| 03_key | 板载按键（GPIO0）扫描 |
| 04_exti | GPIO 外部中断（GPIO0 输入 / GPIO48 输出） |
| 05_uart | UART |
| 06_timer | 定时器 |
| 07_pwm | LEDC PWM |
| 08_adc | ADC1（GPIO1/2/3 采样） |
| 09_i2c | I2C 驱动 OLED（SDA=GPIO7 / SCL=GPIO8 为例程接线） |
| 10_spi | SPI2 驱动 LCD（例程接线：MISO4/MOSI8/SCLK2/CS5，仅供 SPI 用法参考） |
| 11_wifi_ap_tcp_server | WiFi AP + TCP Server |
| 12_wifi_ap_tcp_client | WiFi AP + TCP Client |
| 13_bluetooth | 蓝牙 |
| 14_spi_sd_card | SPI SD 卡 |

> 例程接线与本项目不同（例程只演示对应外设用法），引脚以本项目
> [firmware/PINMAP.md](../../../firmware/PINMAP.md) 为准。
