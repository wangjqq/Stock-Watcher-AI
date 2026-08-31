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
        ├── pages/DeviceSettings.tsx  # 设备设置
        ├── pages/ApiConfig.tsx       # 接口配置（多接口管理 + 测试）
        ├── pages/FieldSelect.tsx     # 数据字段解析/选择（按数据源）
        └── pages/ScreenLayout.tsx    # 屏幕显示配置（像素拖拽布局）
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
4. **选择字段**：在「字段解析」选择数据源接口，测试/解析字段，勾选要显示的字段，添加到显示列表（自动绑定当前数据源）。
5. **布局屏幕**：在「屏幕布局」的 128×144 像素画布上调整每个字段的位置、大小、字号、小数位、涨跌颜色；每个显示块可在「数据源」中切换读取的接口，保存。
6. **自动盯盘**：设备按各接口自己的刷新频率独立获取数据并刷新屏幕。

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

## 待办 / 后续

- [ ] 按需引入 antd（当前整包引入，gzip 后约 300KB，仍有优化空间）
