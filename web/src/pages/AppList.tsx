import { useEffect, useState } from 'react'
import { Button, Card, Input, List, Modal, Popconfirm, Space, Typography, message } from 'antd'
import {
  ArrowDownOutlined,
  ArrowUpOutlined,
  DeleteOutlined,
  EditOutlined,
  LayoutOutlined,
  PartitionOutlined,
  PlusOutlined,
} from '@ant-design/icons'
import { api } from '../api/client'
import { appStore } from '../api/appStore'
import type { App, AppConfig } from '../types'

interface Props {
  /** 跳转到其他配置页（'fields' = 添加字段，'layout' = 屏幕布局） */
  onNavigate: (key: string) => void
}

export default function AppList({ onNavigate }: Props) {
  const [config, setConfig] = useState<AppConfig | null>(null)
  const [saving, setSaving] = useState(false)
  const [renaming, setRenaming] = useState<{ index: number; name: string } | null>(null)

  const apps: App[] = config?.apps ?? []

  useEffect(() => {
    api
      .getConfig()
      .then(setConfig)
      .catch((e: Error) => message.error(`读取失败: ${e.message}`))
  }, [])

  const save = async (next: App[]) => {
    if (!config) return
    setSaving(true)
    try {
      const cur = await api.getConfig()
      await api.saveConfig({ ...cur, apps: next })
      setConfig({ ...cur, apps: next })
    } catch (e) {
      message.error(`保存失败: ${(e as Error).message}`)
    } finally {
      setSaving(false)
    }
  }

  const addApp = async () => {
    if (!config) return
    if (apps.length >= 8) {
      message.warning('应用数量已达上限（8 个）')
      return
    }
    const name = `应用 ${apps.length + 1}`
    await save([...apps, { name, widgets: [] }])
    appStore.set(apps.length) // 指向新建的应用
    message.success(`已新建「${name}」，可去「字段解析 / 屏幕布局」配置它`)
  }

  const renameApp = async () => {
    if (!renaming) return
    const name = renaming.name.trim() || `应用 ${renaming.index + 1}`
    await save(apps.map((a, i) => (i === renaming.index ? { ...a, name } : a)))
    setRenaming(null)
    message.success('已重命名')
  }

  const removeApp = async (index: number) => {
    if (!config) return
    if (apps.length <= 1) {
      message.warning('至少保留一个应用')
      return
    }
    const next = apps.filter((_, i) => i !== index)
    await save(next)
    if (appStore.get() >= next.length) {
      appStore.set(next.length - 1)
    }
    message.success('已删除')
  }

  const moveApp = async (index: number, dir: -1 | 1) => {
    const to = index + dir
    if (to < 0 || to >= apps.length) return
    const next = [...apps]
    ;[next[index], next[to]] = [next[to], next[index]]
    await save(next)
    if (appStore.get() === index) {
      appStore.set(to)
    }
  }

  const open = (key: string, index: number) => {
    appStore.set(index)
    onNavigate(key)
  }

  return (
    <Card
      title="应用列表（一个页面 = 一个应用）"
      extra={
        <Button type="primary" icon={<PlusOutlined />} onClick={addApp} loading={saving} disabled={!config}>
          新建应用
        </Button>
      }
    >
      <Typography.Paragraph type="secondary">
        设备开机默认进入<strong>第一个</strong>应用。每个应用有独立的 widget 布局；
        先新建应用，再在「字段解析」为它添加字段、「屏幕布局」为它摆放位置。列表中第一个即开机默认页。
      </Typography.Paragraph>

      <List
        dataSource={apps}
        renderItem={(app, i) => (
          <List.Item
            actions={[
              <Button
                key="up"
                size="small"
                icon={<ArrowUpOutlined />}
                disabled={i === 0}
                onClick={() => moveApp(i, -1)}
              />,
              <Button
                key="down"
                size="small"
                icon={<ArrowDownOutlined />}
                disabled={i === apps.length - 1}
                onClick={() => moveApp(i, 1)}
              />,
              <Button key="rename" size="small" icon={<EditOutlined />} onClick={() => setRenaming({ index: i, name: app.name })}>
                重命名
              </Button>,
              <Button key="fields" size="small" icon={<PartitionOutlined />} onClick={() => open('fields', i)}>
                添加字段
              </Button>,
              <Button key="layout" size="small" type="primary" ghost icon={<LayoutOutlined />} onClick={() => open('layout', i)}>
                配置布局
              </Button>,
              <Popconfirm
                key="del"
                title="删除该应用？其布局将一并清除。"
                onConfirm={() => removeApp(i)}
                disabled={apps.length <= 1}
              >
                <Button size="small" danger icon={<DeleteOutlined />} disabled={apps.length <= 1}>
                  删除
                </Button>
              </Popconfirm>,
            ]}
          >
            <List.Item.Meta
              title={
                <Space>
                  <Typography.Text strong>{i === 0 ? '★ ' : ''}{app.name || `应用 ${i + 1}`}</Typography.Text>
                  <Typography.Text type="secondary">{app.widgets.length} 个显示块</Typography.Text>
                </Space>
              }
              description={i === 0 ? '开机默认进入' : `顺序：${i + 1} / ${apps.length}`}
            />
          </List.Item>
        )}
      />

      <Modal
        title="重命名应用"
        open={renaming !== null}
        onOk={renameApp}
        onCancel={() => setRenaming(null)}
        okText="保存"
        cancelText="取消"
      >
        <Input
          value={renaming?.name ?? ''}
          maxLength={32}
          onChange={(e) => setRenaming((r) => (r ? { ...r, name: e.target.value } : r))}
          onPressEnter={renameApp}
          placeholder="应用名称"
        />
      </Modal>
    </Card>
  )
}
