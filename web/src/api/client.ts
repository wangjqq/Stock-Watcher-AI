import type { AppConfig, DeviceStatus, FieldInfo, TestResult } from '../types'

async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
  const resp = await fetch(path, {
    headers: { 'Content-Type': 'application/json' },
    ...options,
  })
  if (!resp.ok) {
    const text = await resp.text().catch(() => '')
    throw new Error(`HTTP ${resp.status}: ${text}`)
  }
  return resp.json() as Promise<T>
}

export const api = {
  // 配置
  getConfig: () => request<AppConfig>('/api/config'),
  saveConfig: (cfg: AppConfig) =>
    request<{ ok: boolean }>('/api/config', { method: 'POST', body: JSON.stringify(cfg) }),
  // 接口测试与字段解析
  testInterface: (url: string) =>
    request<TestResult>('/api/interface/test', { method: 'POST', body: JSON.stringify({ url }) }),
  getFields: () => request<{ raw: string; fields: FieldInfo[] }>('/api/fields'),
  // 设备状态
  getStatus: () => request<DeviceStatus>('/api/status'),
}
