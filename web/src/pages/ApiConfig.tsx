import { useState } from 'react'
import { Button, Card, Form, Input, List, Tag, Typography, message } from 'antd'
import { api } from '../api/client'
import type { FieldInfo, TestResult } from '../types'

export default function ApiConfig() {
  const [url, setUrl] = useState('')
  const [testing, setTesting] = useState(false)
  const [result, setResult] = useState<TestResult | null>(null)

  const onTest = async () => {
    if (!url.trim()) {
      message.warning('请输入接口地址')
      return
    }
    setTesting(true)
    setResult(null)
    try {
      const res = await api.testInterface(url.trim())
      setResult(res)
      if (res.ok) message.success(`解析出 ${res.field_count} 个字段`)
      else message.error(`测试失败: ${res.error}`)
    } catch (e) {
      message.error(`请求失败: ${(e as Error).message}`)
    } finally {
      setTesting(false)
    }
  }

  const fields: FieldInfo[] = result?.fields ?? []

  return (
    <Card title="接口配置">
      <Form layout="inline" onFinish={onTest} style={{ marginBottom: 16 }}>
        <Form.Item style={{ flex: 1 }}>
          <Input
            placeholder="http://example.com/api/stock"
            value={url}
            onChange={(e) => setUrl(e.target.value)}
          />
        </Form.Item>
        <Form.Item>
          <Button type="primary" htmlType="submit" loading={testing}>
            测试接口
          </Button>
        </Form.Item>
      </Form>

      {result?.ok && (
        <List
          size="small"
          bordered
          dataSource={fields}
          renderItem={(f) => (
            <List.Item>
              <code>{f.path}</code>
              <Tag style={{ marginLeft: 8 }}>{f.type}</Tag>
              <Typography.Text type="secondary" style={{ marginLeft: 8 }}>
                示例: {f.sample}
              </Typography.Text>
            </List.Item>
          )}
        />
      )}
      {result && (
        <Typography.Paragraph type="secondary" style={{ marginTop: 16 }}>
          原始返回（截断）：{result.raw ?? '-'}
        </Typography.Paragraph>
      )}
    </Card>
  )
}
