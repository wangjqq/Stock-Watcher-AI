import { useEffect, useMemo, useState } from 'react'
import { Button, Card, List, Select, Space, Tree, message } from 'antd'
import { api } from '../api/client'
import { fieldStore } from '../api/fieldStore'
import { appStore } from '../api/appStore'
import type { AppConfig, FieldInfo, Widget } from '../types'

export default function FieldSelect() {
  const [config, setConfig] = useState<AppConfig | null>(null)
  const [appIndex, setAppIndex] = useState(() => appStore.get())
  const [interfaceId, setInterfaceId] = useState<number | undefined>()
  const [fields, setFields] = useState<FieldInfo[]>([])
  const [selected, setSelected] = useState<string[]>([])
  const [testing, setTesting] = useState(false)

  const interfaces = config?.interfaces ?? []
  const apps = config?.apps ?? []
  const safeIndex = appIndex < apps.length ? appIndex : 0
  const app = apps[safeIndex]
  const current = interfaces.find((i) => i.id === interfaceId)

  useEffect(() => {
    api
      .getConfig()
      .then((cfg) => {
        setConfig(cfg)
        if (cfg.interfaces.length > 0) {
          setInterfaceId((id) => id ?? cfg.interfaces[0].id)
        }
      })
      .catch((e: Error) => message.error(`读取失败: ${e.message}`))
  }, [])

  const switchApp = (index: number) => {
    appStore.set(index)
    setAppIndex(index)
  }

  /* 切换数据源时加载其已解析字段 */
  useEffect(() => {
    if (interfaceId === undefined) return
    setFields(fieldStore.get(interfaceId))
    setSelected([])
  }, [interfaceId])

  const onTest = async () => {
    if (!current) return
    if (!current.url.trim()) {
      message.warning('请先在「接口配置」中填写该接口地址')
      return
    }
    setTesting(true)
    try {
      const res = await api.testInterface(current.url.trim())
      if (res.ok) {
        fieldStore.set(current.id, res.fields ?? [])
        setFields(res.fields ?? [])
        message.success(`解析出 ${res.field_count} 个字段`)
      } else {
        message.error(`测试失败: ${res.error}`)
      }
    } catch (e) {
      message.error(`请求失败: ${(e as Error).message}`)
    } finally {
      setTesting(false)
    }
  }

  const treeData = useMemo(
    () =>
      fields.map((f) => ({
        key: f.path,
        title: `${f.path} (${f.type}, 示例: ${f.sample})`,
      })),
    [fields],
  )

  const addWidgets = async () => {
    if (interfaceId === undefined) return
    try {
      const cur = await api.getConfig()
      const idx = safeIndex < cur.apps.length ? safeIndex : 0
      const widgets: Widget[] = [...(cur.apps[idx]?.widgets ?? [])]
      for (const path of selected) {
        const f = fields.find((x) => x.path === path)
        widgets.push({
          interface_id: interfaceId,
          label: f?.path ?? path,
          field_path: path,
          format: 0,
          decimal_places: 2,
          unit: '',
          use_change_color: false,
          x: 0,
          y: 0,
          w: 64,
          h: 16,
          font_size: 2,
        })
      }
      const apps = cur.apps.map((a, i) => (i === idx ? { ...a, widgets } : a))
      await api.saveConfig({ ...cur, apps })
      setConfig({ ...cur, apps })
      setSelected([])
      message.success(`已向「${app?.name || '应用'}」添加 ${selected.length} 个显示字段`)
    } catch (e) {
      message.error(`添加失败: ${(e as Error).message}`)
    }
  }

  return (
    <Card
      title={`数据字段解析（目标应用：「${app?.name || '应用'}」）`}
      extra={
        <Button type="primary" onClick={addWidgets} disabled={!selected.length}>
          添加显示字段
        </Button>
      }
    >
      <Space style={{ marginBottom: 16 }} wrap>
        <span>目标应用：</span>
        <Select
          style={{ width: 180 }}
          value={safeIndex}
          onChange={switchApp}
          options={apps.map((a, i) => ({ value: i, label: a.name || `应用 ${i + 1}` }))}
        />
        <span>数据源：</span>
        <Select
          style={{ width: 260 }}
          placeholder="选择接口"
          value={interfaceId}
          onChange={setInterfaceId}
          options={interfaces.map((i) => ({
            value: i.id,
            label: `${i.name || '接口'}${i.url ? `（${i.url}）` : ''}`,
          }))}
        />
        <Button onClick={onTest} loading={testing} disabled={!current}>
          测试/解析字段
        </Button>
      </Space>

      {!fields.length ? (
        <p>暂无字段，请先「测试/解析字段」。</p>
      ) : (
        <Tree
          checkable
          selectable={false}
          defaultExpandAll
          treeData={treeData}
          checkedKeys={selected}
          onCheck={(keys) => setSelected(keys as string[])}
        />
      )}
      <List
        size="small"
        header={`当前应用「${app?.name || '应用'}」已选显示字段`}
        dataSource={app?.widgets ?? []}
        renderItem={(w) => (
          <List.Item>
            {w.label} → {w.field_path}（
            {interfaces.find((i) => i.id === w.interface_id)?.name || `接口#${w.interface_id}`}）
          </List.Item>
        )}
      />
    </Card>
  )
}
