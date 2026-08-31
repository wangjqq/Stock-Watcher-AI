/** 字段显示格式（与固件 formatter.h 中的 format_type_t 对应） */
export type FormatType = 0 | 1 | 2

/** 一个数据接口（每个接口可独立配置刷新时间） */
export interface DataInterface {
  id: number
  name: string
  url: string
  refresh_interval_ms: number
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

/** 设备全局配置 */
export interface AppConfig {
  device_name: string
  ssid: string
  password: string
  brightness: number
  interfaces: DataInterface[]
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
