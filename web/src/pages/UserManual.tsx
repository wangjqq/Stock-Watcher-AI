import { Alert, Card, Collapse, Space, Tag, Typography } from 'antd'
import {
  BookOutlined,
  BulbOutlined,
  CheckCircleOutlined,
  QuestionCircleOutlined,
  RocketOutlined,
  SettingOutlined,
} from '@ant-design/icons'

const { Title, Paragraph, Text } = Typography

/* 功能变更时请同步更新：设备端 firmware/main/display/app_ui.c 的 s_manual_pages 分页内容。 */
const sections = [
  {
    key: 'intro',
    icon: <BookOutlined />,
    label: '项目简介',
    content: (
      <>
        <Paragraph>
          这是一款基于 <Text strong>ESP32 + 2.4 英寸彩屏（240×320，触摸）</Text>的股票盯盘设备。用户通过
          网页管理界面配置数据接口，设备自动获取、解析数据，并按照用户布局实时显示行情。
        </Paragraph>
        <Paragraph>
          <Text strong>核心理念：</Text>
          用户提供数据接口 → 系统自动解析数据 → 用户选择需要的数据 → 用户布局屏幕显示 → 设备自动获取并显示。
        </Paragraph>
        <Paragraph>
          设备管理网页直接运行在设备上（第一阶段无需独立服务器）。设备端支持触摸、旋钮、系统菜单、低功耗休眠、
          省电模式、深度睡眠与条件提醒等功能。
        </Paragraph>
      </>
    ),
  },
  {
    key: 'quickstart',
    icon: <RocketOutlined />,
    label: '快速上手',
    content: (
      <>
        <Paragraph>
          <Text strong>1. 首次开机联网：</Text>
          设备启动后自动尝试连接已保存的 Wi-Fi。未配置时，在设备端进入「系统 → WiFi」扫描并连接网络（触摸软键盘
          输入密码），连接成功自动保存。
        </Paragraph>
        <Paragraph>
          <Text strong>2. 打开管理页：</Text>
          手机/电脑连接同一 Wi-Fi 后，浏览器访问 <Text code>http://stockwatcher.local</Text>（或设备 IP）。也可在
          设备「系统 → QR」页扫码直达。
        </Paragraph>
        <Paragraph>
          <Text strong>无 Wi-Fi 时（AP 热点）：</Text>
          设备始终开放 <Text code>StockWatcher-XXXX</Text> 无密码热点（热点名见「系统状态」页）。手机/电脑连接
          该热点后，访问 <Text code>http://192.168.4.1</Text> 即可打开管理页。
        </Paragraph>
        <Paragraph>
          <Text strong>3. 配置流程：</Text>
          接口配置 → 测试接口 → 字段解析 → 选择字段 → 屏幕布局 → 保存。设备随即按配置自动刷新显示，重启后配置不丢失。
        </Paragraph>
      </>
    ),
  },
  {
    key: 'web',
    icon: <SettingOutlined />,
    label: '网页管理功能',
    content: (
      <>
        <Paragraph>
          <Text strong>设备设置：</Text>
          设备名称、Wi-Fi（SSID/密码）、屏幕亮度、自动亮度（光敏）、屏幕休眠、自动轮播、<Text strong>深度睡眠
          （固定时段）</Text>、蜂鸣器与音量；支持配置<Text strong> 导出 / 导入 / 重置</Text>。
        </Paragraph>
        <Paragraph>
          <Text strong>接口配置：</Text>
          添加股票数据接口（最多 8 个）：接口地址、请求方式（GET/POST）、请求头、请求体、刷新间隔。可「测试接口」
          查看返回数据。
        </Paragraph>
        <Paragraph>
          <Text strong>字段解析：</Text>
          系统自动递归解析接口返回的 JSON（对象/数组），生成字段树；选择需要显示的字段并配置显示格式（颜色、
          百分号、小数位、单位）。
        </Paragraph>
        <Paragraph>
          <Text strong>应用列表：</Text>
          创建多个显示应用，每个应用拥有独立的字段与布局；设备端可左右滑动/自动轮播切换应用。
        </Paragraph>
        <Paragraph>
          <Text strong>屏幕布局：</Text>
          将选中的字段以拖拽/网格方式自由摆放到屏幕任意位置（支持多只股票同屏）。
        </Paragraph>
        <Paragraph>
          <Text strong>提醒设置：</Text>
          配置条件告警（字段 大于/小于 阈值），触发时设备蜂鸣 + LED 闪烁 + 屏幕红色横幅。
        </Paragraph>
        <Paragraph>
          <Text strong>系统状态：</Text>
          查看设备 IP、信号强度、AP 热点名与访问地址、固件版本、网络状态。
        </Paragraph>
        <Paragraph>
          <Text strong>固件升级：</Text>
          通过 OTA 上传固件文件在线升级，升级失败自动回滚。
        </Paragraph>
      </>
    ),
  },
  {
    key: 'device',
    icon: <BulbOutlined />,
    label: '设备端操作',
    content: (
      <>
        <Paragraph>
          <Text strong>应用列表：</Text>
          旋钮旋转移动光标、按下打开；触摸点按打开应用，左右滑动移动光标。
        </Paragraph>
        <Paragraph>
          <Text strong>应用内：</Text>
          点击状态栏最左侧「‹」返回列表，点按其他区域切换下一个应用，左右滑动切换应用，边缘向内滑动返回。
        </Paragraph>
        <Paragraph>
          <Text strong>系统菜单：</Text>
          亮度（旋钮调节/自动亮度）、手动刷新、状态、WiFi（设备端配网）、二维码（扫码进管理页）、触摸标定、用户手册。
        </Paragraph>
        <Paragraph>
          <Text strong>触摸手势：</Text>
          点按=选择；左右滑=移动光标/切换应用/翻页；左缘右滑或右缘左滑=返回上一级。
        </Paragraph>
        <Paragraph>
          <Text strong>触摸标定：</Text>
          首次使用或触摸不准时，在「系统 → TouchCal」按提示依次点按左上角、右下角完成两点标定。
        </Paragraph>
      </>
    ),
  },
  {
    key: 'power',
    icon: <CheckCircleOutlined />,
    label: '低功耗与提醒',
    content: (
      <>
        <Paragraph>
          <Text strong>屏幕休眠：</Text>
          无操作超过设定时间自动熄灭背光（数据拉取、告警、状态栏继续运行），任何按键/触摸或告警触发可唤醒。
        </Paragraph>
        <Paragraph>
          <Text strong>深度睡眠（固定时段）：</Text>
          在「设备设置」中可配置「入睡 / 唤醒」时间（支持跨天，如 22:00 → 08:00）。到点且屏幕已休眠后，
          整机进入深度睡眠（<Text strong>耗电 &lt;1mA</Text>，数据与告警随之暂停）；RTC 定时器到唤醒时刻自动醒来，
          或随时按下<Text strong>旋钮（GPIO7）</Text>手动唤醒，唤醒后自动联网校时并恢复原应用继续盯盘。
          需联网校时成功且开启屏幕休眠（或至少无操作一段时间）才会进入。
        </Paragraph>
        <Paragraph>
          <Text strong>自动轮播：</Text>
          按设定间隔自动切换用户应用，仅作用于应用列表，休眠时暂停。
        </Paragraph>
        <Paragraph>
          <Text strong>条件提醒：</Text>
          每个提醒对应「接口 + 字段 + 大于/小于阈值」，满足条件时蜂鸣 + LED 告警 + 屏幕横幅。
        </Paragraph>
        <Paragraph>
          <Text strong>省电模式：</Text>
          在「设备设置」中可手动开启「省电模式」（背光降到最低 + CPU 降频），明显降低耗电；电量低于 20% 时设备也会
          <Text strong>自动进入</Text>省电模式，电量回升后自动退出。
        </Paragraph>
        <Paragraph>
          <Text strong>低电量保护：</Text>
          电量低于 5% 时，设备<Text strong>闪烁红光提示后进入深度睡眠保护</Text>（唤醒方式同深度睡眠，RTC 定时唤醒复测电量，
          充电后自动恢复运行）；<Text strong>未接电池（纯 USB 供电）</Text>时低电量保护不生效，避免误休眠。
        </Paragraph>
        <Paragraph>
          <Text strong>电量与背光：</Text>
          支持电池电压采集与 PWM 调光，自动亮度由光敏传感器接管。
        </Paragraph>
      </>
    ),
  },
  {
    key: 'faq',
    icon: <QuestionCircleOutlined />,
    label: '常见问题',
    content: (
      <>
        <Paragraph>
          <Text strong>无法打开管理页？</Text>
          确认手机/电脑与设备处于同一 Wi-Fi；访问 <Text code>http://stockwatcher.local</Text> 或设备状态页显示的 IP。
        </Paragraph>
        <Paragraph>
          <Text strong>屏幕触摸不准确/无响应？</Text>
          进入「系统 → TouchCal」重新进行两点标定。
        </Paragraph>
        <Paragraph>
          <Text strong>二维码扫不出来？</Text>
          保持手机与设备同一网络，且二维码在屏幕中完整显示。
        </Paragraph>
        <Paragraph>
          <Text strong>接口返回但屏幕无数据？</Text>
          检查网页「字段解析」中是否已选择字段，以及「屏幕布局」是否已摆放字段。
        </Paragraph>
      </>
    ),
  },
]

export default function UserManual() {
  return (
    <Card title="用户手册">
      <Alert
        type="info"
        showIcon
        message="本手册随固件内置"
        description="设备端也可在「系统 → Manual」查看精简版手册。功能更新时，网页与设备端手册会同步更新。"
        style={{ marginBottom: 16 }}
      />
      <Title level={5} style={{ marginTop: 0 }}>
        股票盯盘设备 · 功能说明
      </Title>
      <Collapse
        accordion
        defaultActiveKey="intro"
        items={sections.map((s) => ({
          key: s.key,
          label: (
            <Space>
              {s.icon}
              {s.label}
              {s.key === 'web' && <Tag color="blue">核心</Tag>}
            </Space>
          ),
          children: s.content,
        }))}
      />
    </Card>
  )
}
