import { useEffect, useState } from 'react'
import { Button, Card, Form, Input, InputNumber, Popconfirm, Slider, message } from 'antd'
import { api } from '../api/client'

interface FormValues {
  device_name: string
  ssid: string
  password: string
  brightness: number
  refresh_interval_s: number
}

export default function DeviceSettings() {
  const [form] = Form.useForm()
  const [loading, setLoading] = useState(false)
  const [saving, setSaving] = useState(false)

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
          refresh_interval_s: Math.round((cfg.refresh_interval_ms || 5000) / 1000),
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
        refresh_interval_ms: values.refresh_interval_s * 1000,
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
      <Form form={form} layout="vertical" onFinish={onSave} style={{ maxWidth: 480 }}>
        <Form.Item name="device_name" label="设备名称">
          <Input placeholder="StockWatcher" />
        </Form.Item>
        <Form.Item name="ssid" label="Wi-Fi 名称（留空则进入 AP 配网模式）">
          <Input placeholder="请输入 Wi-Fi SSID" />
        </Form.Item>
        <Form.Item name="password" label="Wi-Fi 密码">
          <Input.Password placeholder="请输入 Wi-Fi 密码" />
        </Form.Item>
        <Form.Item name="refresh_interval_s" label="刷新频率（秒）">
          <InputNumber min={1} max={3600} style={{ width: '100%' }} />
        </Form.Item>
        <Form.Item name="brightness" label="屏幕亮度">
          <Slider min={0} max={100} />
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
