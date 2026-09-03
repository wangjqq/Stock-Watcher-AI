/** 字段显示格式（与固件 formatter.h 中的 format_type_t 对应） */
export type FormatType = 0 | 1 | 2

/** 请求方法：0 = GET，1 = POST（与固件 http_method_t 对应） */
export type HttpMethod = 0 | 1

/** 一个数据接口（每个接口可独立配置刷新时间） */
export interface DataInterface {
  id: number
  name: string
  url: string
  refresh_interval_ms: number
  method: HttpMethod
  headers: string[]
  post_body: string
}

/** 屏幕上的一个显示块（像素布局，数据源由 interface_id 指定） */
export interface Widget {
  interface_id: number
  label: string
  field_path: string
  format: FormatType
  decimal_places: number
  unit: string
  use_change_color: boolean
  x: number
  y: number
  w: number
  h: number
  font_size: number
}

/** 一个应用 = 一套屏幕布局（一个页面 = 一个应用） */
export interface App {
  name: string
  widgets: Widget[]
}

/** 提醒条件：0 = 大于 >，1 = 小于 <（与固件 alert_cond_t 对应） */
export type AlertCond = 0 | 1

/** 条件提醒规则（全局，监控某个接口的数值字段） */
export interface Alert {
  enabled: boolean
  interface_id: number
  field_path: string
  condition: AlertCond
  threshold: number
}

/** 设备全局配置 */
export interface AppConfig {
  device_name: string
  ssid: string
  password: string
  brightness: number
  auto_brightness: boolean
  power_save_enabled: boolean
  screen_timeout_s: number
  auto_rotate_s: number
  deep_sleep_enabled: boolean
  deep_sleep_start_hh: number
  deep_sleep_start_mm: number
  deep_sleep_end_hh: number
  deep_sleep_end_mm: number
  buzzer_enabled: boolean
  buzzer_volume: number
  interfaces: DataInterface[]
  apps: App[]
  alerts: Alert[]
}

/** 解析出的一个叶子字段 */
export interface FieldInfo {
  path: string
  type: string
  sample: string
}

/** 接口测试结果 */
export interface TestResult {
  ok: boolean
  field_count?: number
  fields?: FieldInfo[]
  error?: string
  raw?: string
}

/** 设备状态 */
export interface DeviceStatus {
  device_name: string
  wifi_connected: boolean
  ip: string
  rssi: number
  ap_ssid: string
  ap_ip: string
  firmware_version: string
  uptime_ms: number
  /** 累计异常复位（崩溃）次数 */
  crash_count: number
  /** 最后崩溃原因码（esp_reset_reason_t），0 = 从未异常复位 */
  last_crash_code: number
  /** 最后崩溃原因（ASCII 短描述） */
  last_crash_reason: string
}
