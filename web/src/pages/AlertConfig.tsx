import { useEffect, useState } from 'react'
import { AutoComplete, Button, Card, InputNumber, List, Popconfirm, Select, Space, Switch, Tag, message } from 'antd'
import { DeleteOutlined, PlusOutlined } from '@ant-design/icons'
import { api } from '../api/client'
import { fieldStore } from '../api/fieldStore'
import type { Alert, AlertCond } from '../types'

/** 编辑中的一条提醒规则 */
interface Row {
  key: number
  enabled: boolean
  interface_id: number
  field_path: string
  condition: AlertCond
  threshold: number
}

export default function AlertConfig() {
  const [rows, setRows] = useState<Row[]>([])
  const [loaded, setLoaded] = useState(false)
  const [saving, setSaving] = useState(false)
  const [interfaces, setInterfaces] = useState<{ id: number; name: string }[]>([])

  useEffect(() => {
    api
      .getConfig()
      .then((cfg) => {
        setInterfaces(cfg.interfaces.map((it) => ({ id: it.id, name: it.name })))
        setRows(
          cfg.alerts.map((al) => ({
            key: al.interface_id * 1000 + Math.floor(Math.random() * 1000),
            enabled: al.enabled,
            interface_id: al.interface_id,
            field_path: al.field_path,
            condition: al.condition,
            threshold: al.threshold,
          })),
        )
        setLoaded(true)
      })
      .catch((e: Error) => message.error(`读取失败: ${e.message}`))
  }, [])

  const updateRow = (key: number, patch: Partial<Row>) => {
    setRows((rs) => rs.map((r) => (r.key === key ? { ...r, ...patch } : r)))
  }

  const addRow = () => {
    const firstId = interfaces[0]?.id ?? 0
    setRows((rs) => [
      ...rs,
      {
        key: Date.now() + Math.random(),
        enabled: true,
        interface_id: firstId,
        field_path: '',
        condition: 0,
        threshold: 0,
      },
    ])
  }

  const removeRow = (key: number) => setRows((rs) => rs.filter((r) => r.key !== key))

  /** 字段路径下拉：优先取该接口已解析的字段，也允许手输 */
  const fieldOptions = (interfaceId: number) =>
    fieldStore.get(interfaceId).map((f) => ({ value: f.path, label: f.path }))

  const save = async () => {
    setSaving(true)
    try {
      const cur = await api.getConfig()
      const alerts: Alert[] = rows
        .filter((r) => r.field_path.trim())
        .map((r) => ({
          enabled: r.enabled,
          interface_id: r.interface_id,
          field_path: r.field_path.trim(),
          condition: r.condition,
          threshold: r.threshold,
        }))
      await api.saveConfig({ ...cur, alerts })
      message.success('提醒配置已保存')
    } catch (e) {
      message.error(`保存失败: ${(e as Error).message}`)
    } finally {
      setSaving(false)
    }
  }

  return (
    <Card
      title="提醒设置（条件触发蜂鸣 + LED 告警）"
      extra={
        <Space>
          <Button icon={<PlusOutlined />} onClick={addRow} disabled={!interfaces.length}>
            新增提醒
          </Button>
          <Button type="primary" loading={saving} onClick={save}>
            保存提醒
          </Button>
        </Space>
      }
    >
      {!loaded && <p>加载中...</p>}
      {loaded && !rows.length && (
        <p>
          还没有提醒规则，点击右上角「新增提醒」添加。规则触发时设备会蜂鸣提示并让状态灯橙色闪烁。
        </p>
      )}
      <List
        dataSource={rows}
        renderItem={(r) => (
          <List.Item
            actions={[
              <Popconfirm key="del" title="删除该提醒？" onConfirm={() => removeRow(r.key)}>
                <Button danger icon={<DeleteOutlined />} />
              </Popconfirm>,
            ]}
          >
            <div style={{ width: '100%' }}>
              <Space wrap>
                <Switch checked={r.enabled} onChange={(v) => updateRow(r.key, { enabled: v })} />
                <span>当</span>
                <Select
                  style={{ width: 180 }}
                  placeholder="数据源"
                  value={r.interface_id}
                  onChange={(v) => updateRow(r.key, { interface_id: v })}
                  options={interfaces.map((it) => ({
                    value: it.id,
                    label: it.name || `接口#${it.id}`,
                  }))}
                />
                <AutoComplete
                  style={{ width: 240 }}
                  placeholder="字段路径（如 stock.price）"
                  value={r.field_path}
                  options={fieldOptions(r.interface_id)}
                  onChange={(v) => updateRow(r.key, { field_path: v })}
                  allowClear
                />
                <Select
                  style={{ width: 70 }}
                  value={r.condition}
                  onChange={(v) => updateRow(r.key, { condition: v })}
                  options={[
                    { value: 0, label: '>' },
                    { value: 1, label: '<' },
                  ]}
                />
                <InputNumber
                  style={{ width: 120 }}
                  value={r.threshold}
                  onChange={(v) => updateRow(r.key, { threshold: v ?? 0 })}
                />
              </Space>
              {!fieldOptions(r.interface_id).length && (
                <div style={{ marginTop: 4 }}>
                  <Tag>该接口暂无已解析字段，可在「字段解析」页先测试接口，或直接手动输入路径</Tag>
                </div>
              )}
            </div>
          </List.Item>
        )}
      />
    </Card>
  )
}
