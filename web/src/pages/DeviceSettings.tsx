import { useEffect, useRef, useState } from 'react'
import { Button, Card, Form, Input, InputNumber, Popconfirm, Slider, Space, Switch, message } from 'antd'
import { api } from '../api/client'
import type { AppConfig } from '../types'

interface FormValues {
  device_name: string
  ssid: string
  password: string
  brightness: number
  auto_brightness: boolean
  power_save_enabled: boolean
  screen_sleep_enabled: boolean
  screen_sleep_s: number
  auto_rotate_enabled: boolean
  auto_rotate_s: number
  deep_sleep_enabled: boolean
  deep_sleep_start_hh: number
  deep_sleep_start_mm: number
  deep_sleep_end_hh: number
  deep_sleep_end_mm: number
  buzzer_enabled: boolean
  buzzer_volume: number
}

export default function DeviceSettings() {
  const [form] = Form.useForm()
  const [loading, setLoading] = useState(false)
  const [saving, setSaving] = useState(false)
  const [importing, setImporting] = useState(false)
  const fileRef = useRef<HTMLInputElement>(null)
  const autoBrightness = Form.useWatch('auto_brightness', form)
  const screenSleep = Form.useWatch('screen_sleep_enabled', form)
  const autoRotate = Form.useWatch('auto_rotate_enabled', form)
  const deepSleep = Form.useWatch('deep_sleep_enabled', form)
  const buzzerEnabled = Form.useWatch('buzzer_enabled', form)

  const fillForm = (cfg: AppConfig) =>
    form.setFieldsValue({
      device_name: cfg.device_name,
      ssid: cfg.ssid,
      password: cfg.password,
      brightness: cfg.brightness,
      auto_brightness: cfg.auto_brightness,
      power_save_enabled: cfg.power_save_enabled,
      screen_sleep_enabled: (cfg.screen_timeout_s ?? 0) > 0,
      screen_sleep_s: cfg.screen_timeout_s || 60,
      auto_rotate_enabled: (cfg.auto_rotate_s ?? 0) > 0,
      auto_rotate_s: cfg.auto_rotate_s || 10,
      deep_sleep_enabled: cfg.deep_sleep_enabled,
      deep_sleep_start_hh: cfg.deep_sleep_start_hh ?? 22,
      deep_sleep_start_mm: cfg.deep_sleep_start_mm ?? 0,
      deep_sleep_end_hh: cfg.deep_sleep_end_hh ?? 8,
      deep_sleep_end_mm: cfg.deep_sleep_end_mm ?? 0,
      buzzer_enabled: cfg.buzzer_enabled,
      buzzer_volume: cfg.buzzer_volume,
    })

  useEffect(() => {
    setLoading(true)
    api
      .getConfig()
      .then((cfg) => fillForm(cfg))
      .catch((e: Error) => message.error(`读取配置失败: ${e.message}`))
      .finally(() => setLoading(false))
  }, [form])

  const onSave = async (values: FormValues) => {
    setSaving(true)
    try {
      const cur = await api.getConfig()
      await api.saveConfig({
        ...cur,
        device_name: values.device_name,
        ssid: values.ssid,
        password: values.password,
        brightness: values.brightness,
        auto_brightness: values.auto_brightness,
        power_save_enabled: values.power_save_enabled,
        screen_timeout_s: values.screen_sleep_enabled ? Math.max(1, values.screen_sleep_s) : 0,
        auto_rotate_s: values.auto_rotate_enabled ? Math.max(1, values.auto_rotate_s) : 0,
        deep_sleep_enabled: values.deep_sleep_enabled,
        deep_sleep_start_hh: values.deep_sleep_start_hh,
        deep_sleep_start_mm: values.deep_sleep_start_mm,
        deep_sleep_end_hh: values.deep_sleep_end_hh,
        deep_sleep_end_mm: values.deep_sleep_end_mm,
        buzzer_enabled: values.buzzer_enabled,
        buzzer_volume: values.buzzer_volume,
      })
      message.success('已保存')
      if (values.ssid !== cur.ssid) {
        message.info('Wi-Fi 已变更，设备正在切换网络，请稍候重连')
      }
    } catch (e) {
      message.error(`保存失败: ${(e as Error).message}`)
    } finally {
      setSaving(false)
    }
  }

  const onReset = async () => {
    try {
      await api.reset()
      message.success('配置已清空，设备即将重启')
    } catch (e) {
      message.error(`重置失败: ${(e as Error).message}`)
    }
  }

  /* 导出配置为 JSON 文件下载（含接口/应用/提醒等全部配置，含 Wi-Fi 密码，注意保管） */
  const onExport = async () => {
    try {
      const cfg = await api.getConfig()
      const blob = new Blob([JSON.stringify(cfg, null, 2)], { type: 'application/json' })
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = `stock-watcher-config-${cfg.device_name || 'device'}.json`
      a.click()
      URL.revokeObjectURL(url)
      message.success('配置已导出')
    } catch (e) {
      message.error(`导出失败: ${(e as Error).message}`)
    }
  }

  /* 导入配置文件：JSON 解析后整体覆盖保存到设备 */
  const onImportFile = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0]
    e.target.value = '' // 允许重复选择同一文件
    if (!file) return
    try {
      const parsed = JSON.parse(await file.text())
      if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
        message.error('无效的配置文件（需要 JSON 对象）')
        return
      }
      setImporting(true)
      const before = await api.getConfig()
      await api.saveConfig(parsed as AppConfig)
      fillForm(parsed as AppConfig)
      message.success('配置已导入')
      if (parsed.ssid !== before.ssid) {
        message.info('Wi-Fi 已变更，设备正在切换网络，请稍候重连')
      }
    } catch (e) {
      message.error(`导入失败: ${(e as Error).message}`)
    } finally {
      setImporting(false)
    }
  }

  return (
    <Card title="设备设置" loading={loading}>
      <Form form={form} layout="vertical" onFinish={onSave} style={{ maxWidth: 520 }}>
        <Form.Item name="device_name" label="设备名称">
          <Input placeholder="StockWatcher" />
        </Form.Item>
        <Form.Item name="ssid" label="Wi-Fi 名称（留空表示未配置，可在设备端「系统 → WiFi」页连接）">
          <Input placeholder="请输入 Wi-Fi SSID" />
        </Form.Item>
        <Form.Item name="password" label="Wi-Fi 密码">
          <Input.Password placeholder="请输入 Wi-Fi 密码" />
        </Form.Item>
        <Form.Item name="brightness" label="屏幕亮度">
          <Slider min={0} max={100} disabled={!!autoBrightness} />
        </Form.Item>
        <Form.Item name="auto_brightness" label="自动亮度（光敏传感器）" valuePropName="checked">
          <Switch />
        </Form.Item>
        <Form.Item
          name="power_save_enabled"
          label="省电模式（背光最低 + CPU 降频）"
          extra="电量低于 20% 时自动进入省电模式；低于 5% 进入深度睡眠并闪烁红光提示"
          valuePropName="checked"
        >
          <Switch />
        </Form.Item>

        <Form.Item label="屏幕休眠（无操作自动熄屏，按键唤醒）">
          <Space>
            <Form.Item name="screen_sleep_enabled" valuePropName="checked" noStyle>
              <Switch />
            </Form.Item>
            <Form.Item name="screen_sleep_s" noStyle>
              <InputNumber min={1} max={3600} addonAfter="秒" disabled={!screenSleep} />
            </Form.Item>
          </Space>
        </Form.Item>

        <Form.Item label="自动轮播（应用列表定时自动切换）">
          <Space>
            <Form.Item name="auto_rotate_enabled" valuePropName="checked" noStyle>
              <Switch />
            </Form.Item>
            <Form.Item name="auto_rotate_s" noStyle>
              <InputNumber min={1} max={3600} addonAfter="秒" disabled={!autoRotate} />
            </Form.Item>
          </Space>
        </Form.Item>

        <Form.Item label="深度睡眠（固定时段整机休眠 <1mA，RTC 定时 + 旋钮按下唤醒）">
          <Space direction="vertical" style={{ width: '100%' }}>
            <Form.Item name="deep_sleep_enabled" valuePropName="checked" noStyle>
              <Switch />
            </Form.Item>
            {deepSleep && (
              <Space wrap>
                <span>入睡</span>
                <Form.Item name="deep_sleep_start_hh" noStyle>
                  <InputNumber min={0} max={23} style={{ width: 64 }} placeholder="时" />
                </Form.Item>
                <span>:</span>
                <Form.Item name="deep_sleep_start_mm" noStyle>
                  <InputNumber min={0} max={59} style={{ width: 64 }} placeholder="分" />
                </Form.Item>
                <span>唤醒</span>
                <Form.Item name="deep_sleep_end_hh" noStyle>
                  <InputNumber min={0} max={23} style={{ width: 64 }} placeholder="时" />
                </Form.Item>
                <span>:</span>
                <Form.Item name="deep_sleep_end_mm" noStyle>
                  <InputNumber min={0} max={59} style={{ width: 64 }} placeholder="分" />
                </Form.Item>
              </Space>
            )}
          </Space>
        </Form.Item>

        <Form.Item name="buzzer_enabled" label="蜂鸣器" valuePropName="checked">
          <Switch />
        </Form.Item>
        <Form.Item name="buzzer_volume" label="蜂鸣音量">
          <Slider min={0} max={100} disabled={!buzzerEnabled} />
        </Form.Item>
        <Space wrap>
          <Button type="primary" htmlType="submit" loading={saving}>
            保存
          </Button>
          <Popconfirm
            title="确定重置所有配置吗？"
            description="设备将清空配置并重启，需在设备端「系统 → WiFi」页重新连接 Wi-Fi"
            onConfirm={onReset}
          >
            <Button danger>重置配置</Button>
          </Popconfirm>
          <Button onClick={onExport}>导出配置</Button>
          <Popconfirm
            title="导入将覆盖当前全部配置"
            description="包含接口 / 应用布局 / 提醒等，导入后自动保存生效"
            onConfirm={() => fileRef.current?.click()}
          >
            <Button loading={importing}>导入配置</Button>
          </Popconfirm>
        </Space>
        <input
          ref={fileRef}
          type="file"
          accept=".json,application/json"
          style={{ display: 'none' }}
          onChange={onImportFile}
        />
      </Form>
    </Card>
  )
}
