# 股票盯盘 AI 助手

带 LCD/TFT 彩屏的股票盯盘设备，基于 **ESP32 + ESP-IDF**，内置网页管理界面。

> **用户提供数据接口 → 系统自动解析数据 → 用户选择需要的数据 → 用户布局屏幕显示 → 设备自动获取并显示**

详细需求见 [.trae/rules/project_rules.md](.trae/rules/project_rules.md)。

## 目录结构

```text
Stock-Watcher-AI/
├── firmware/          # ESP32 固件（ESP-IDF, C/C++）
│   ├── CMakeLists.txt
│   ├── partitions.csv # 分区表（NVS + factory，前端页面内嵌进固件，无需独立分区）
│   ├── sdkconfig.defaults
│   ├── tools/
│   │   └── gen_web_assets.py  # 把 web_dist 产物生成为 web_assets.c 内嵌进固件
│   ├── web_dist/      # 前端构建产物（npm run build 输出到这里，gzip 压缩）
│   └── main/
│       ├── main.c             # 入口：初始化 + 定时刷新主循环
│       ├── CMakeLists.txt
│       ├── common/            # 全局配置
│       │   └── app_config.c/.h    # 配置模型 + NVS 存取 + JSON 序列化
│       ├── network/           # 网络
│       │   ├── wifi_manager.c/.h  # AP 配网 / STA 连接 / mDNS
│       │   ├── http_server.c/.h   # 内置 HTTP 服务：REST API + 内嵌静态页面
│       │   └── data_fetcher.c/.h  # 请求外部股票数据接口
│       ├── data/              # 数据解析与格式化
│       │   ├── field_parser.c/.h  # 通用 JSON 自动解析（对象/数组 → 字段树）
│       │   └── formatter.c/.h     # 字段格式化（颜色/百分号/小数位/单位）
│       ├── display/           # 屏幕显示
│       │   ├── display.c/.h       # ST7735 1.8" 128x160 驱动（SPI + 帧缓冲 + 8x8 点阵字体）
│       │   ├── status_bar.c/.h    # 顶部状态栏（时间/信号/电量，SNTP 校时）
│       │   └── layout_renderer.c/.h # 按配置把数据渲染到画布
│       └── web/               # 内嵌前端资源
│           └── web_assets.h/.c    # 内嵌资源结构（.c 由生成器产出，构建期生成）
└── web/               # 网页管理界面（React + TS + Vite + Ant Design）
    └── src/
        ├── types.ts           # 前后端共享的类型定义
        ├── api/client.ts      # 设备 REST API 封装
        ├── api/fieldStore.ts  # 各接口测试字段缓存（跨页共享预览数据）
        ├── api/appStore.ts    # 当前选中的应用索引（跨页共享）
        ├── pages/DeviceSettings.tsx  # 设备设置
        ├── pages/ApiConfig.tsx       # 接口配置（多接口管理 + 测试）
        ├── pages/FieldSelect.tsx     # 数据字段解析/选择（按数据源，添加到指定应用）
        ├── pages/AppList.tsx         # 应用列表（新建/重命名/排序/删除，一个页面 = 一个应用）
        └── pages/ScreenLayout.tsx    # 屏幕显示配置（像素拖拽布局，按选中应用）
```

## 构建固件

前置：已安装 ESP-IDF（v5.x），并完成环境初始化。

```bash
# 1. 构建前端（产物输出到 firmware/web_dist，自动 gzip 压缩）
cd web
npm install
npm run build

# 2. 构建固件（配置阶段自动把 web_dist 内嵌进固件）
cd ../firmware
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

> 前端产物由 `firmware/tools/gen_web_assets.py` 在固件配置阶段生成 `web_assets.c`
> 以字节数组直接内嵌进固件二进制，ESP32 通过 HTTP 服务在内存中发送给浏览器，
> 不再依赖 SPIFFS 文件系统（体积更小、更可靠）。

## 使用流程

1. **配网**：设备未配置 Wi-Fi 时开启热点 `StockWatcher-xxxx`（密码 `12345678`），
   连上热点后访问 `http://192.168.4.1`，在「设备设置」中填写 Wi-Fi 并保存，
   设备会自动切换网络连接（无需重启）。
2. **访问**：设备接入局域网后，通过 `http://stockwatcher.local`（mDNS）或设备 IP 打开配置页。
3. **配置接口**：在「接口配置」新增数据接口，填写名称、地址和**该接口独立的刷新时间**，点击「测试」自动解析返回的 JSON 字段。支持多个接口。
4. **选择字段**：在「字段解析」选择数据源接口，测试/解析字段，勾选要显示的字段，添加到**当前目标应用**的显示列表（自动绑定当前数据源）。
5. **应用列表**：在「应用列表」新建 / 重命名 / 排序应用（一个页面 = 一个应用，**第一个为开机默认页**）；每个应用有独立的字段与布局。
6. **布局屏幕**：在「屏幕布局」选择某个应用，在其 128×144 像素画布上调整每个字段的位置、大小、字号、小数位、涨跌颜色；每个显示块可在「数据源」中切换读取的接口，保存。
7. **自动盯盘**：设备按各接口自己的刷新频率独立获取数据，并把当前显示的应用刷新到屏幕。

## 设备端 REST API

| 方法 | 路径                | 说明                                                 |
| ---- | ------------------- | ---------------------------------------------------- |
| GET  | /api/config         | 读取配置                                             |
| POST | /api/config         | 保存配置（JSON body，WiFi 变更后自动重连，无需重启） |
| POST | /api/reset          | 一键清空配置并重启（回到 AP 配网模式）               |
| POST | /api/interface/test | 测试接口并解析字段，body: `{"url": "..."}`           |
| GET  | /api/fields         | 最近一次测试解析出的字段                             |
| GET  | /api/status         | 设备状态（连接/IP/运行时间）                         |

## 屏幕与画布

- 屏幕：ST7735 1.8" 128×160（8pin，SPI）。接线见 [firmware/PINMAP.md](firmware/PINMAP.md)。
- 顶部 16px 为**状态栏**（系统保留，不参与布局）：左侧时间（SNTP 校时，未同步时显示运行时长），
  右侧信号 4 格 + 电池电量（当前无电池采样，固定满电）。
- 状态栏下方 **128×144 为可配置画布**（共 18432 像素）。前端「屏幕布局」与固件 `display.h` 共用同一组
  常量（`web/src/constants.ts` ⇄ `firmware/main/display/display.h`），Widget 的 x/y/w/h 均为画布内像素坐标，
  前端预览按 2 倍放大展示。

## 第一阶段范围

- 多接口、多股票（JSON 对象/数组自动解析），每个接口独立刷新频率
- 字段选择与格式化（颜色/百分号/小数位/单位）
- 像素画布屏幕布局（128×144，含顶部状态栏）
- AP 配网 + 局域网（mDNS）访问
- 配置本地保存（NVS）、重启复用

## 第二阶段：升级规划（P0 → P2 全量排期）

> 新增硬件：**带按键旋钮（EC11 / E8H6）+ 返回键** + 无源蜂鸣器 + RGB LED（三引脚）。接线最终同步到 [PINMAP.md](firmware/PINMAP.md)。
> **交互模型**：一个页面 = 一个应用，前端配置「应用列表」；**旋钮旋转 = 导航、旋钮按下 = 确认、独立返回键 = 返回**。
> 新模块统一放 `firmware/main/hardware/`（knob / buzzer / led）。

### 硬件（三件套）

- [x] 旋钮导航 + 返回键
  - [x] 接线：旋钮 A=GPIO25 / B=GPIO26 / SW=GPIO27（按下确认），返回键=GPIO14（内部上拉 + 软件消抖 + 正交解码）
  - [x] `knob` 模块：旋转解码（EC11 / E8H6 原理相同）/ 按下确认 / 返回键事件
  - [x] 更新 PINMAP.md 接线图
- [x] 无源蜂鸣器（PWM）
  - [x] 接线：GPIO 13（LEDC）
  - [x] `buzzer` 模块：`buzzer_play(pattern)` 音调/时长序列
  - [x] 声音事件表：按键 / 涨 / 跌 / 告警 / 断网
  - [x] 网页开关与音量配置
- [x] RGB LED（三引脚）
  - [x] 接线：GPIO 33 / 21 / 22
  - [x] `led` 模块：`led_set` / `led_blink` / 呼吸
  - [x] 状态色：联网绿 / 断网红 / 刷新闪 / 告警橙，随涨跌字段变色

### P0：脱机闭环（旋钮 + 应用列表一体做）

- [x] 应用列表（一个页面 = 一个应用）
  - [x] 配置改为 `apps[]`：每个应用 = 名称 + 独立 widget 布局
  - [x] 前端新增「应用列表」配置页：新建 / 重命名 / 排序应用，并在每个应用内配置自己的布局
  - [x] 设备开机默认进入第一个应用
- [ ] 旋钮交互（配合应用列表）
  - [x] 旋钮旋转切换应用（循环切换，按键音 + 用缓存数据立即重绘）
  - [ ] 应用列表菜单：旋转移动光标，按下（确认）打开所选应用，返回键回列表
  - [ ] 应用内：旋转交给应用自身功能；返回键回应用列表
  - [ ] 内置「系统」应用：亮度 / 手动刷新 / 状态（IP · 信号 · 版本）
- [ ] 条件提醒（alerts）
  - [ ] 配置新增 `alerts[]`：interface_id / field_path / 条件(> / <) / 阈值 / 启用
  - [ ] NVS 序列化与 config JSON 读写
  - [ ] 固件每轮数据检查，触发蜂鸣 + LED 闪烁
  - [ ] 网页「提醒设置」页（增删改规则）

### P1：接口增强 + 系统增强

- [ ] 接口增强
  - [ ] `interface_t` 加 `method`（GET / POST）与 `headers[]`
  - [ ] `data_fetcher` 支持 POST + 自定义头（Token 鉴权）
  - [ ] 网页「接口配置」对应表单
- [ ] 系统信息页
  - [ ] `/api/status` 加固件版本 / RSSI
  - [ ] 网页状态卡片 + 内置「系统」应用展示

### P2：进阶（可选）

- [ ] OTA 升级（分区表加 ota_0 / ota_1 / ota_data，网页上传固件）
- [ ] 配置导入导出（下载 / 上传 JSON 配置）
- [ ] 低功耗（屏幕休眠 / 空闲降频 / 按键唤醒）
- [ ] 自动轮播（应用列表定时自动切换）
