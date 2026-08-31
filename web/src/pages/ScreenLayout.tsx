import { useEffect, useRef, useState, type PointerEvent as ReactPointerEvent } from 'react'
import { Button, Card, InputNumber, List, Space, Switch, message } from 'antd'
import { api } from '../api/client'
import type { AppConfig, Widget } from '../types'
import {
  CANVAS_HEIGHT,
  CANVAS_SCALE,
  CANVAS_WIDTH,
  DISPLAY_WIDTH,
  GRID_SIZE,
  STATUS_BAR_HEIGHT,
} from '../constants'

const s = CANVAS_SCALE

function clamp(v: number, min: number, max: number) {
  return v < min ? min : v > max ? max : v
}

/** 拖拽中的状态（move=移动 / resize=右下角缩放） */
interface DragState {
  index: number
  mode: 'move' | 'resize'
  startClientX: number
  startClientY: number
  origX: number
  origY: number
  origW: number
  origH: number
}

export default function ScreenLayout() {
  const [config, setConfig] = useState<AppConfig | null>(null)
  const [saving, setSaving] = useState(false)
  const dragRef = useRef<DragState | null>(null)
  const canvasRef = useRef<HTMLDivElement | null>(null)

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

  const applyDrag = (clientX: number, clientY: number) => {
    const d = dragRef.current
    if (!d) return
    const dx = (clientX - d.startClientX) / s
    const dy = (clientY - d.startClientY) / s
    setConfig((c) => {
      if (!c) return c
      const widgets = [...c.widgets]
      const w = widgets[d.index]
      if (d.mode === 'move') {
        const nx = clamp(d.origX + Math.round(dx), 0, CANVAS_WIDTH - 1)
        const ny = clamp(d.origY + Math.round(dy), 0, CANVAS_HEIGHT - 1)
        widgets[d.index] = { ...w, x: nx, y: ny }
      } else {
        const nw = clamp(d.origW + Math.round(dx), 1, CANVAS_WIDTH - d.origX)
        const nh = clamp(d.origH + Math.round(dy), 1, CANVAS_HEIGHT - d.origY)
        widgets[d.index] = { ...w, w: nw, h: nh }
      }
      return { ...c, widgets }
    })
  }

  const startDrag = (index: number, mode: DragState['mode']) => (e: ReactPointerEvent) => {
    e.preventDefault()
    e.stopPropagation()
    const w = widgets[index]
    dragRef.current = {
      index,
      mode,
      startClientX: e.clientX,
      startClientY: e.clientY,
      origX: w.x,
      origY: w.y,
      origW: w.w,
      origH: w.h,
    }
    const move = (ev: PointerEvent) => applyDrag(ev.clientX, ev.clientY)
    const up = () => {
      window.removeEventListener('pointermove', move)
      window.removeEventListener('pointerup', up)
      dragRef.current = null
    }
    window.addEventListener('pointermove', move)
    window.addEventListener('pointerup', up)
  }

  const widgets: Widget[] = config?.widgets ?? []

  return (
    <Card
      title={`屏幕显示配置（像素画布 ${CANVAS_WIDTH}×${CANVAS_HEIGHT}）`}
      extra={
        <Button type="primary" loading={saving} onClick={save} disabled={!widgets.length}>
          保存布局
        </Button>
      }
    >
      {/* 屏幕预览：顶部状态栏（系统保留）+ 可配置画布 */}
      <div
        style={{
          width: DISPLAY_WIDTH * s,
          borderRadius: 8,
          overflow: 'hidden',
          border: '1px solid #444',
          marginBottom: 12,
        }}
      >
        {/* 状态栏 16px（不可配置，仅示意） */}
        <div
          style={{
            height: STATUS_BAR_HEIGHT * s,
            background: '#111',
            color: '#fff',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            padding: '0 4px',
            fontSize: Math.max(8 * s, 10),
          }}
        >
          <span>12:34</span>
          <div style={{ display: 'flex', alignItems: 'center', gap: 4 * s }}>
            {/* 信号 4 格 */}
            <div style={{ display: 'flex', alignItems: 'flex-end', gap: 1 * s, height: 8 * s }}>
              {[2, 4, 6, 8].map((h) => (
                <div key={h} style={{ width: 1.5 * s, height: h * s, background: '#fff' }} />
              ))}
            </div>
            {/* 电池 */}
            <div style={{ position: 'relative', width: 12 * s, height: 7 * s, border: `1px solid #fff`, borderRadius: 1 }}>
              <div style={{ position: 'absolute', left: 1 * s, top: 1 * s, width: 8 * s, height: 5 * s, background: '#00e676' }} />
              <div style={{ position: 'absolute', right: -2 * s, top: 1.5 * s, width: 1.5 * s, height: 4 * s, background: '#fff' }} />
            </div>
          </div>
        </div>

        {/* 可配置画布 128x144 */}
        <div
          ref={canvasRef}
          style={{
            position: 'relative',
            width: CANVAS_WIDTH * s,
            height: CANVAS_HEIGHT * s,
            background: '#000',
            touchAction: 'none',
            userSelect: 'none',
            cursor: 'default',
          }}
        >
          {/* 网格参考线 */}
          {Array.from({ length: Math.floor(CANVAS_WIDTH / GRID_SIZE) + 1 }).map((_, i) => (
            <div
              key={`v${i}`}
              style={{ position: 'absolute', left: i * GRID_SIZE * s, top: 0, width: 1, height: '100%', background: 'rgba(255,255,255,0.06)' }}
            />
          ))}
          {Array.from({ length: Math.floor(CANVAS_HEIGHT / GRID_SIZE) + 1 }).map((_, i) => (
            <div
              key={`h${i}`}
              style={{ position: 'absolute', top: i * GRID_SIZE * s, left: 0, height: 1, width: '100%', background: 'rgba(255,255,255,0.06)' }}
            />
          ))}
          {widgets.map((w, i) => (
            <div
              key={i}
              onPointerDown={startDrag(i, 'move')}
              title={`拖动移动 (${w.x},${w.y}) ${w.w}×${w.h}`}
              style={{
                position: 'absolute',
                left: w.x * s,
                top: w.y * s,
                width: Math.max(1, w.w) * s,
                height: Math.max(1, w.h) * s,
                background: 'rgba(42,42,42,0.92)',
                border: '1px solid #4a4a4a',
                color: '#fff',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontSize: Math.min(w.font_size || 2, 4) * s * 2,
                padding: 2,
                overflow: 'hidden',
                textAlign: 'center',
                cursor: 'move',
              }}
            >
              {w.label} {w.field_path}
              {/* 右下角缩放把手 */}
              <div
                onPointerDown={startDrag(i, 'resize')}
                style={{
                  position: 'absolute',
                  right: 0,
                  bottom: 0,
                  width: 10,
                  height: 10,
                  cursor: 'nwse-resize',
                  background: 'linear-gradient(135deg, transparent 50%, #00e676 50%)',
                }}
              />
            </div>
          ))}
        </div>
      </div>

      <p style={{ color: '#888', fontSize: 12, marginBottom: 16 }}>
        在画布上<b>拖动</b>移动位置、拖动<b>右下角</b>调整大小（单位像素）。
        可配置区域 {CANVAS_WIDTH}×{CANVAS_HEIGHT}（顶部状态栏 {STATUS_BAR_HEIGHT}px 为系统保留）；也可用下方输入框精调。
      </p>

      <List
        dataSource={widgets}
        renderItem={(w, i) => (
          <List.Item
            actions={[
              <Space key="pos" size={4}>
                位置 x:
                <InputNumber size="small" min={0} max={CANVAS_WIDTH - 1} value={w.x} onChange={(v) => updateWidget(i, { x: v ?? 0 })} />
                y:
                <InputNumber size="small" min={0} max={CANVAS_HEIGHT - 1} value={w.y} onChange={(v) => updateWidget(i, { y: v ?? 0 })} />
                宽:
                <InputNumber size="small" min={1} max={CANVAS_WIDTH} value={w.w} onChange={(v) => updateWidget(i, { w: v ?? 1 })} />
                高:
                <InputNumber size="small" min={1} max={CANVAS_HEIGHT} value={w.h} onChange={(v) => updateWidget(i, { h: v ?? 1 })} />
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
