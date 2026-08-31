import { useEffect, useState } from 'react'
import { Button, Card, InputNumber, List, Space, Switch, message } from 'antd'
import { api } from '../api/client'
import type { AppConfig, Widget } from '../types'

export default function ScreenLayout() {
  const [config, setConfig] = useState<AppConfig | null>(null)
  const [saving, setSaving] = useState(false)

  useEffect(() => {
    api
      .getConfig()
      .then(setConfig)
      .catch((e: Error) => message.error(`读取失败: ${e.message}`))
  }, [])

  const updateWidget = (index: number, patch: Partial<Widget>) => {
    setConfig((c) => {
      if (!c) return c
      const widgets = [...c.widgets]
      widgets[index] = { ...widgets[index], ...patch }
      return { ...c, widgets }
    })
  }

  const save = async () => {
    if (!config) return
    setSaving(true)
    try {
      const cur = await api.getConfig()
      await api.saveConfig({ ...cur, widgets: config.widgets })
      message.success('布局已保存')
    } catch (e) {
      message.error(`保存失败: ${(e as Error).message}`)
    } finally {
      setSaving(false)
    }
  }

  const widgets: Widget[] = config?.widgets ?? []

  return (
    <Card
      title="屏幕显示配置（网格布局）"
      extra={
        <Button type="primary" loading={saving} onClick={save} disabled={!widgets.length}>
          保存布局
        </Button>
      }
    >
      <div
        style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(8, 1fr)',
          gap: 4,
          background: '#111',
          padding: 8,
          minHeight: 200,
          borderRadius: 8,
          marginBottom: 16,
        }}
      >
        {widgets.map((w, i) => (
          <div
            key={i}
            style={{
              gridColumn: `${w.x + 1} / span ${Math.max(1, w.w)}`,
              gridRow: `${w.y + 1} / span ${Math.max(1, w.h)}`,
              background: '#2a2a2a',
              color: '#fff',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              fontSize: 12 + (w.font_size || 1) * 2,
              padding: 4,
              overflow: 'hidden',
              textAlign: 'center',
            }}
          >
            {w.label} {w.field_path}
          </div>
        ))}
      </div>

      <List
        dataSource={widgets}
        renderItem={(w, i) => (
          <List.Item
            actions={[
              <Space key="pos" size={4}>
                位置 x:
                <InputNumber size="small" min={0} value={w.x} onChange={(v) => updateWidget(i, { x: v ?? 0 })} />
                y:
                <InputNumber size="small" min={0} value={w.y} onChange={(v) => updateWidget(i, { y: v ?? 0 })} />
                宽:
                <InputNumber size="small" min={1} value={w.w} onChange={(v) => updateWidget(i, { w: v ?? 1 })} />
                高:
                <InputNumber size="small" min={1} value={w.h} onChange={(v) => updateWidget(i, { h: v ?? 1 })} />
              </Space>,
              <Space key="fmt" size={4}>
                字号:
                <InputNumber size="small" min={1} max={6} value={w.font_size} onChange={(v) => updateWidget(i, { font_size: v ?? 2 })} />
                小数位:
                <InputNumber size="small" min={0} max={6} value={w.decimal_places} onChange={(v) => updateWidget(i, { decimal_places: v ?? 2 })} />
                涨跌色:
                <Switch size="small" checked={w.use_change_color} onChange={(v) => updateWidget(i, { use_change_color: v })} />
              </Space>,
            ]}
          >
            <List.Item.Meta title={w.label} description={w.field_path} />
          </List.Item>
        )}
      />
    </Card>
  )
}
