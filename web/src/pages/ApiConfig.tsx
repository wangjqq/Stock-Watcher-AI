import { useEffect, useState } from 'react'
import { Button, Card, Input, InputNumber, List, Popconfirm, Select, Space, Tag, message } from 'antd'
import { DeleteOutlined, ExperimentOutlined, PlusOutlined } from '@ant-design/icons'
import { api } from '../api/client'
import { fieldStore } from '../api/fieldStore'
import type { DataInterface, HttpMethod } from '../types'

const HEADER_MAX = 4

/** 编辑中的一行头 */
interface HeaderRow {
  key: number
  name: string
  value: string
}

/** 编辑中的一行接口 */
interface Row {
  key: number // 本地编辑键
  id: number // 最终写入的接口 id
  name: string
  url: string
  refresh_s: number
  method: HttpMethod
  headers: HeaderRow[]
  post_body: string
}

/** "Key: Value" 拆成 {name, value} */
const parseHeader = (line: string) => {
  const i = line.indexOf(':')
  if (i < 0) return { name: line.trim(), value: '' }
  return { name: line.slice(0, i).trim(), value: line.slice(i + 1).trim() }
}

export default function ApiConfig() {
  const [rows, setRows] = useState<Row[]>([])
  const [loaded, setLoaded] = useState(false)
  const [saving, setSaving] = useState(false)
  const [testing, setTesting] = useState<number | null>(null) // 正在测试的行 key
  const [testMsg, setTestMsg] = useState<Record<number, string>>({})

  useEffect(() => {
    api
      .getConfig()
      .then((cfg) => {
        setRows(
          cfg.interfaces.map((it) => ({
            key: it.id,
            id: it.id,
            name: it.name,
            url: it.url,
            refresh_s: Math.round((it.refresh_interval_ms || 5000) / 1000),
            method: it.method ?? 0,
            headers: (it.headers ?? []).slice(0, HEADER_MAX).map((line, idx) => ({ ...parseHeader(line), key: idx })),
            post_body: it.post_body ?? '',
          })),
        )
        setLoaded(true)
      })
      .catch((e: Error) => message.error(`读取失败: ${e.message}`))
  }, [])

  const updateRow = (key: number, patch: Partial<Row>) => {
    setRows((rs) => rs.map((r) => (r.key === key ? { ...r, ...patch } : r)))
  }

  const updateHeader = (key: number, hkey: number, patch: Partial<HeaderRow>) => {
    setRows((rs) =>
      rs.map((r) =>
        r.key === key
          ? { ...r, headers: r.headers.map((h) => (h.key === hkey ? { ...h, ...patch } : h)) }
          : r,
      ),
    )
  }

  const addHeader = (key: number) => {
    const r = rows.find((x) => x.key === key)
    if (!r || r.headers.length >= HEADER_MAX) return
    setRows((rs) =>
      rs.map((x) =>
        x.key === key ? { ...x, headers: [...x.headers, { key: Date.now() + Math.random(), name: '', value: '' }] } : x,
      ),
    )
  }

  const removeHeader = (key: number, hkey: number) => {
    setRows((rs) =>
      rs.map((r) => (r.key === key ? { ...r, headers: r.headers.filter((h) => h.key !== hkey) } : r)),
    )
  }

  const addRow = () => {
    const newId = Date.now()
    setRows((rs) => [
      ...rs,
      { key: newId, id: newId, name: `接口${rs.length + 1}`, url: '', refresh_s: 5, method: 0, headers: [], post_body: '' },
    ])
  }

  const removeRow = (key: number) => setRows((rs) => rs.filter((r) => r.key !== key))

  const onTest = async (row: Row) => {
    if (!row.url.trim()) {
      message.warning('请输入接口地址')
      return
    }
    setTesting(row.key)
    try {
      const headers = row.headers.filter((h) => h.name.trim()).map((h) => `${h.name.trim()}: ${h.value}`)
      const res = await api.testInterface(
        row.url.trim(),
        row.method,
        headers,
        row.method === 1 ? row.post_body : undefined,
      )
      if (res.ok) {
        fieldStore.set(row.id, res.fields ?? [])
        setTestMsg((m) => ({ ...m, [row.key]: `解析出 ${res.field_count} 个字段` }))
        message.success(`解析出 ${res.field_count} 个字段`)
      } else {
        setTestMsg((m) => ({ ...m, [row.key]: `失败: ${res.error}` }))
        message.error(`测试失败: ${res.error}`)
      }
    } catch (e) {
      setTestMsg((m) => ({ ...m, [row.key]: `请求失败: ${(e as Error).message}` }))
      message.error(`请求失败: ${(e as Error).message}`)
    } finally {
      setTesting(null)
    }
  }

  const save = async () => {
    setSaving(true)
    try {
      const cur = await api.getConfig()
      const interfaces: DataInterface[] = rows.map((r) => ({
        id: r.id,
        name: r.name,
        url: r.url.trim(),
        refresh_interval_ms: Math.max(1, r.refresh_s || 5) * 1000,
        method: r.method,
        headers: r.headers
          .filter((h) => h.name.trim())
          .slice(0, HEADER_MAX)
          .map((h) => `${h.name.trim()}: ${h.value}`),
        post_body: r.post_body,
      }))
      await api.saveConfig({ ...cur, interfaces })
      message.success('接口配置已保存')
    } catch (e) {
      message.error(`保存失败: ${(e as Error).message}`)
    } finally {
      setSaving(false)
    }
  }

  return (
    <Card
      title="接口配置（多接口，各自独立刷新，支持 POST + 自定义头）"
      extra={
        <Space>
          <Button icon={<PlusOutlined />} onClick={addRow}>
            新增接口
          </Button>
          <Button type="primary" loading={saving} onClick={save}>
            保存接口
          </Button>
        </Space>
      }
    >
      {!loaded && <p>加载中...</p>}
      {loaded && !rows.length && (
        <p>还没有数据接口，点击右上角「新增接口」添加（每个接口可设置独立的刷新时间）。</p>
      )}
      <List
        dataSource={rows}
        renderItem={(r) => (
          <List.Item
            actions={[
              <Button
                key="test"
                icon={<ExperimentOutlined />}
                loading={testing === r.key}
                onClick={() => onTest(r)}
              >
                测试
              </Button>,
              <Popconfirm key="del" title="删除该接口？" onConfirm={() => removeRow(r.key)}>
                <Button danger icon={<DeleteOutlined />} />
              </Popconfirm>,
            ]}
          >
            <div style={{ width: '100%' }}>
              <Space wrap>
                <Input
                  style={{ width: 110 }}
                  placeholder="名称"
                  value={r.name}
                  onChange={(e) => updateRow(r.key, { name: e.target.value })}
                />
                <Select
                  style={{ width: 90 }}
                  value={r.method}
                  onChange={(v) => updateRow(r.key, { method: v })}
                  options={[
                    { value: 0, label: 'GET' },
                    { value: 1, label: 'POST' },
                  ]}
                />
                <Input
                  style={{ width: 260 }}
                  placeholder="http://example.com/api/stock"
                  value={r.url}
                  onChange={(e) => updateRow(r.key, { url: e.target.value })}
                />
                刷新(秒):
                <InputNumber
                  min={1}
                  max={3600}
                  value={r.refresh_s}
                  onChange={(v) => updateRow(r.key, { refresh_s: v ?? 5 })}
                />
              </Space>

              {/* 自定义头（Token 鉴权等） */}
              <div style={{ marginTop: 6 }}>
                {r.headers.map((h) => (
                  <Space key={h.key} style={{ marginBottom: 4 }}>
                    <Input
                      style={{ width: 160 }}
                      placeholder="Header 名（如 Authorization）"
                      value={h.name}
                      onChange={(e) => updateHeader(r.key, h.key, { name: e.target.value })}
                    />
                    <Input
                      style={{ width: 200 }}
                      placeholder="值（如 Bearer xxxxx）"
                      value={h.value}
                      onChange={(e) => updateHeader(r.key, h.key, { value: e.target.value })}
                    />
                    <Button
                      danger
                      size="small"
                      icon={<DeleteOutlined />}
                      onClick={() => removeHeader(r.key, h.key)}
                    />
                  </Space>
                ))}
                <Button
                  size="small"
                  icon={<PlusOutlined />}
                  disabled={r.headers.length >= HEADER_MAX}
                  onClick={() => addHeader(r.key)}
                >
                  添加请求头
                </Button>
              </div>

              {/* POST body */}
              {r.method === 1 && (
                <div style={{ marginTop: 6 }}>
                  <Input.TextArea
                    style={{ maxWidth: 480 }}
                    rows={2}
                    placeholder='POST 请求体（可空），如 {"symbol":"AAPL"}'
                    value={r.post_body}
                    onChange={(e) => updateRow(r.key, { post_body: e.target.value })}
                  />
                </div>
              )}

              {testMsg[r.key] && (
                <div style={{ marginTop: 4 }}>
                  <Tag>{testMsg[r.key]}</Tag>
                </div>
              )}
            </div>
          </List.Item>
        )}
      />
    </Card>
  )
}
