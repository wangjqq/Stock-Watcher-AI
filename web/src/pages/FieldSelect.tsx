import { useEffect, useMemo, useState } from 'react'
import { Button, Card, List, Select, Space, Tree, message } from 'antd'
import { api } from '../api/client'
import { fieldStore } from '../api/fieldStore'
import type { AppConfig, FieldInfo, Widget } from '../types'

export default function FieldSelect() {
  const [config, setConfig] = useState<AppConfig | null>(null)
  const [interfaceId, setInterfaceId] = useState<number | undefined>()
  const [fields, setFields] = useState<FieldInfo[]>([])
  const [selected, setSelected] = useState<string[]>([])
  const [testing, setTesting] = useState(false)

  const interfaces = config?.interfaces ?? []
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
      const widgets: Widget[] = [...(cur.widgets ?? [])]
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
      await api.saveConfig({ ...cur, widgets })
      setConfig({ ...cur, widgets })
      setSelected([])
      message.success(`已添加 ${selected.length} 个显示字段`)
    } catch (e) {
      message.error(`添加失败: ${(e as Error).message}`)
    }
  }

  return (
    <Card
      title="数据字段解析"
      extra={
        <Button type="primary" onClick={addWidgets} disabled={!selected.length}>
          添加显示字段
        </Button>
      }
    >
      <Space style={{ marginBottom: 16 }} wrap>
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
        header="当前已选显示字段"
        dataSource={config?.widgets ?? []}
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
