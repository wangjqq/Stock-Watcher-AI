import { useEffect, useMemo, useState } from 'react'
import { Button, Card, List, Tree, message } from 'antd'
import { api } from '../api/client'
import type { AppConfig, FieldInfo, Widget } from '../types'

export default function FieldSelect() {
  const [fields, setFields] = useState<FieldInfo[]>([])
  const [config, setConfig] = useState<AppConfig | null>(null)
  const [selected, setSelected] = useState<string[]>([])

  const load = async () => {
    try {
      const [f, cfg] = await Promise.all([api.getFields(), api.getConfig()])
      setFields(f.fields ?? [])
      setConfig(cfg)
    } catch (e) {
      message.error(`读取失败: ${(e as Error).message}`)
    }
  }

  useEffect(() => {
    load()
  }, [])

  const treeData = useMemo(
    () =>
      fields.map((f) => ({
        key: f.path,
        title: `${f.path} (${f.type}, 示例: ${f.sample})`,
      })),
    [fields],
  )

  const addWidgets = async () => {
    try {
      const cur = await api.getConfig()
      const widgets: Widget[] = [...(cur.widgets ?? [])]
      for (const path of selected) {
        const f = fields.find((x) => x.path === path)
        widgets.push({
          label: f?.path ?? path,
          field_path: path,
          format: 0,
          decimal_places: 2,
          unit: '',
          use_change_color: false,
          x: 0,
          y: 0,
          w: 4,
          h: 1,
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
      {!fields.length ? (
        <p>暂无字段，请先在「接口配置」中测试接口。</p>
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
        renderItem={(w) => <List.Item>{w.label} → {w.field_path}</List.Item>}
      />
    </Card>
  )
}
