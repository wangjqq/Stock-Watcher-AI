import { useEffect, useState } from 'react'
import { Button, Card, Form, Input, InputNumber, Popconfirm, Slider, Space, Switch, message } from 'antd'
import { api } from '../api/client'

interface FormValues {
  device_name: string
  ssid: string
  password: string
  brightness: number
  auto_brightness: boolean
  screen_sleep_enabled: boolean
  screen_sleep_s: number
  auto_rotate_enabled: boolean
  auto_rotate_s: number
  buzzer_enabled: boolean
  buzzer_volume: number
}

export default function DeviceSettings() {
  const [form] = Form.useForm()
  const [loading, setLoading] = useState(false)
  const [saving, setSaving] = useState(false)
  const autoBrightness = Form.useWatch('auto_brightness', form)
  const screenSleep = Form.useWatch('screen_sleep_enabled', form)
  const autoRotate = Form.useWatch('auto_rotate_enabled', form)
  const buzzerEnabled = Form.useWatch('buzzer_enabled', form)

  useEffect(() => {
    setLoading(true)
    api
      .getConfig()
      .then((cfg) =>
        form.setFieldsValue({
          device_name: cfg.device_name,
          ssid: cfg.ssid,
          password: cfg.password,
          brightness: cfg.brightness,
          auto_brightness: cfg.auto_brightness,
          screen_sleep_enabled: (cfg.screen_timeout_s ?? 0) > 0,
          screen_sleep_s: cfg.screen_timeout_s || 60,
          auto_rotate_enabled: (cfg.auto_rotate_s ?? 0) > 0,
          auto_rotate_s: cfg.auto_rotate_s || 10,
          buzzer_enabled: cfg.buzzer_enabled,
          buzzer_volume: cfg.buzzer_volume,
        }),
      )
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
        screen_timeout_s: values.screen_sleep_enabled ? Math.max(1, values.screen_sleep_s) : 0,
        auto_rotate_s: values.auto_rotate_enabled ? Math.max(1, values.auto_rotate_s) : 0,
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

  return (
    <Card title="设备设置" loading={loading}>
      <Form form={form} layout="vertical" onFinish={onSave} style={{ maxWidth: 520 }}>
        <Form.Item name="device_name" label="设备名称">
          <Input placeholder="StockWatcher" />
        </Form.Item>
        <Form.Item name="ssid" label="Wi-Fi 名称（留空则进入 AP 配网模式）">
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

        <Form.Item name="buzzer_enabled" label="蜂鸣器" valuePropName="checked">
          <Switch />
        </Form.Item>
        <Form.Item name="buzzer_volume" label="蜂鸣音量">
          <Slider min={0} max={100} disabled={!buzzerEnabled} />
        </Form.Item>
        <Button type="primary" htmlType="submit" loading={saving}>
          保存
        </Button>
        <Popconfirm
          title="确定重置所有配置吗？"
          description="设备将清空配置并重启，回到 AP 配网模式"
          onConfirm={onReset}
        >
          <Button danger style={{ marginLeft: 8 }}>
            重置配置
          </Button>
        </Popconfirm>
      </Form>
    </Card>
  )
}
