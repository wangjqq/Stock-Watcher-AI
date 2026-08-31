import { useEffect, useRef, useState, type PointerEvent as ReactPointerEvent } from 'react'
import { Button, Card, Input, InputNumber, List, Select, Space, Switch, message } from 'antd'
import { api } from '../api/client'
import { fieldStore } from '../api/fieldStore'
import type { AppConfig, FormatType, Widget } from '../types'
import {
  CANVAS_HEIGHT,
  CANVAS_SCALE,
  CANVAS_WIDTH,
  DISPLAY_WIDTH,
  GRID_SIZE,
  STATUS_BAR_HEIGHT,
} from '../constants'

const s = CANVAS_SCALE

/** 取某 widget 数据源里对应字段的示例值（用于实时预览） */
function sampleOf(w: Widget): string | undefined {
  return fieldStore.get(w.interface_id).find((f) => f.path === w.field_path)?.sample
}

/** 格式化类型选项（与固件 formatter.h 的 format_type_t 对应） */
const FORMAT_OPTIONS = [
  { value: 0, label: '原样' },
  { value: 1, label: '百分比 %' },
  { value: 2, label: '小数位' },
]

/** 按 widget 规则格式化字段示例值（与固件 formatter.c 规则一致，仅用于预览） */
function formatPreview(w: Widget, sample: string | undefined): string {
  const trimmed = String(sample ?? '').trim()
  const num = Number(trimmed)
  if (trimmed === '' || !Number.isFinite(num)) {
    return `${trimmed}${w.unit}`
  }
  switch (w.format) {
    case 1:
      return `${num >= 0 ? '+' : ''}${num.toFixed(w.decimal_places)}%`
    case 2:
      return num.toFixed(w.decimal_places) + w.unit
    default:
      return (Number.isInteger(num) ? String(num) : String(num)) + w.unit
  }
}

/** 涨跌颜色预览：正红负绿，与固件一致 */
function previewColor(w: Widget, sample: string | undefined): string | undefined {
  if (!w.use_change_color || sample === undefined) return undefined
  const num = Number(String(sample).trim())
  if (!Number.isFinite(num)) return undefined
  if (num > 0) return '#ff4d4f'
  if (num < 0) return '#52c41a'
  return '#888'
}

function clamp(v: number, min: number, max: number) {
  return v < min ? min : v > max ? max : v
}

/** 取整到 GRID_SIZE 的倍数（网格吸附） */
function snapToGrid(v: number) {
  return Math.round(v / GRID_SIZE) * GRID_SIZE
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
  const [snapOn, setSnapOn] = useState(true)
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
        let nx = d.origX + Math.round(dx)
        let ny = d.origY + Math.round(dy)
        if (snapOn) {
          nx = snapToGrid(nx)
          ny = snapToGrid(ny)
        }
        nx = clamp(nx, 0, CANVAS_WIDTH - 1)
        ny = clamp(ny, 0, CANVAS_HEIGHT - 1)
        widgets[d.index] = { ...w, x: nx, y: ny }
      } else {
        let nw = d.origW + Math.round(dx)
        let nh = d.origH + Math.round(dy)
        if (snapOn) {
          nw = snapToGrid(nw)
          nh = snapToGrid(nh)
        }
        nw = clamp(nw, 1, CANVAS_WIDTH - d.origX)
        nh = clamp(nh, 1, CANVAS_HEIGHT - d.origY)
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
      {/* 吸附开关 */}
      <div style={{ marginBottom: 8 }}>
        <Space>
          网格吸附（{GRID_SIZE}px）
          <Switch size="small" checked={snapOn} onChange={setSnapOn} />
        </Space>
      </div>

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
                  flexDirection: 'column',
                  alignItems: 'center',
                  justifyContent: 'center',
                  fontSize: Math.min(w.font_size || 2, 4) * s,
                  padding: 2,
                  overflow: 'hidden',
                  textAlign: 'center',
                  cursor: 'move',
                  lineHeight: 1.2,
                }}
              >
                <div style={{ fontSize: Math.max(Math.min(w.font_size || 2, 4) * s * 0.7, 7), color: '#ccc', width: '100%' }}>
                  {w.label || w.field_path}
                </div>
                <div style={{ color: previewColor(w, sampleOf(w)) ?? '#fff', width: '100%', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                  {formatPreview(w, sampleOf(w))}
                </div>
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
        在画布上<b>拖动</b>移动位置、拖动<b>右下角</b>调整大小（单位像素）。开启吸附时按 {GRID_SIZE}px 网格对齐；
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
                数据源:
                <Select
                  size="small"
                  style={{ width: 120 }}
                  value={w.interface_id}
                  options={(config?.interfaces ?? []).map((it) => ({
                    value: it.id,
                    label: it.name || `接口#${it.id}`,
                  }))}
                  onChange={(v) => updateWidget(i, { interface_id: v as number })}
                />
                标签:
                <Input
                  size="small"
                  style={{ width: 90 }}
                  value={w.label}
                  placeholder={w.field_path}
                  onChange={(e) => updateWidget(i, { label: e.target.value })}
                />
                格式:
                <Select
                  size="small"
                  style={{ width: 100 }}
                  value={w.format}
                  options={FORMAT_OPTIONS}
                  onChange={(v) => updateWidget(i, { format: v as FormatType })}
                />
                单位:
                <Input
                  size="small"
                  style={{ width: 56 }}
                  value={w.unit}
                  placeholder="无"
                  onChange={(e) => updateWidget(i, { unit: e.target.value })}
                />
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
