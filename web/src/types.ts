/** 字段显示格式（与固件 formatter.h 中的 format_type_t 对应） */
export type FormatType = 0 | 1 | 2

/** 屏幕上的一个显示块（网格布局） */
export interface Widget {
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

/** 设备全局配置 */
export interface AppConfig {
  device_name: string
  ssid: string
  password: string
  api_url: string
  refresh_interval_ms: number
  brightness: number
  widgets: Widget[]
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
  uptime_ms: number
}
