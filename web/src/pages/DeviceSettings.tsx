import { useEffect, useState } from 'react'
import { Button, Card, Form, Input, Popconfirm, Slider, Switch, message } from 'antd'
import { api } from '../api/client'

interface FormValues {
  device_name: string
  ssid: string
  password: string
  brightness: number
  auto_brightness: boolean
  buzzer_enabled: boolean
  buzzer_volume: number
}

export default function DeviceSettings() {
  const [form] = Form.useForm()
  const [loading, setLoading] = useState(false)
  const [saving, setSaving] = useState(false)
  const buzzerEnabled = Form.useWatch('buzzer_enabled', form)
  const autoBrightness = Form.useWatch('auto_brightness', form)

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
        <Form.Item name="brightness" label="屏幕亮度">
          <Slider min={0} max={100} disabled={autoBrightness} />
        </Form.Item>
        <Form.Item
          name="auto_brightness"
          label="自动亮度（光敏传感器）"
          extra="开启后由 BH1750 光敏传感器按环境光照度自动调节屏幕亮度"
          valuePropName="checked"
        >
          <Switch />
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
