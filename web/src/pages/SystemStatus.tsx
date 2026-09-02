import { useEffect, useState } from 'react'
import { Badge, Card, Descriptions, Statistic, Row, Col, Tag } from 'antd'
import { api } from '../api/client'
import type { DeviceStatus } from '../types'

/** 信号强度分级标签 */
function rssiTag(rssi: number) {
  if (rssi >= -60) return <Tag color="green">强</Tag>
  if (rssi >= -75) return <Tag color="orange">中</Tag>
  return <Tag color="red">弱</Tag>
}

function fmtUptime(ms: number) {
  const s = Math.floor(ms / 1000)
  const h = Math.floor(s / 3600)
  const m = Math.floor((s % 3600) / 60)
  const sec = s % 60
  if (h > 0) return `${h} 小时 ${m} 分`
  if (m > 0) return `${m} 分 ${sec} 秒`
  return `${sec} 秒`
}

export default function SystemStatus() {
  const [st, setSt] = useState<DeviceStatus | null>(null)

  useEffect(() => {
    let alive = true
    const load = () =>
      api
        .getStatus()
        .then((s) => alive && setSt(s))
        .catch(() => {})
    load()
    const timer = setInterval(load, 5000) // 5s 轮询
    return () => {
      alive = false
      clearInterval(timer)
    }
  }, [])

  return (
    <div>
      <Row gutter={16}>
        <Col xs={24} sm={12} lg={6}>
          <Card>
            <Statistic title="设备名称" value={st?.device_name ?? '-'} />
          </Card>
        </Col>
        <Col xs={24} sm={12} lg={6}>
          <Card>
            <Statistic title="运行时长" value={st ? fmtUptime(st.uptime_ms) : '-'} />
          </Card>
        </Col>
        <Col xs={24} sm={12} lg={6}>
          <Card>
            <Statistic title="固件版本" value={st?.firmware_version ?? '-'} />
          </Card>
        </Col>
        <Col xs={24} sm={12} lg={6}>
          <Card>
            <Statistic
              title="信号强度"
              value={st ? `${st.rssi} dBm` : '-'}
              prefix={st ? rssiTag(st.rssi) : undefined}
            />
          </Card>
        </Col>
      </Row>

      <Card title="网络状态" style={{ marginTop: 16 }}>
        <Descriptions column={2} bordered size="small">
          <Descriptions.Item label="Wi-Fi">
            {st ? (
              <Badge status={st.wifi_connected ? 'success' : 'error'} text={st.wifi_connected ? '已连接' : '未连接'} />
            ) : (
              '-'
            )}
          </Descriptions.Item>
          <Descriptions.Item label="IP 地址">{st?.ip || '-'}</Descriptions.Item>
          <Descriptions.Item label="信号强度">{st ? `${st.rssi} dBm` : '-'}</Descriptions.Item>
          <Descriptions.Item label="刷新">{st ? '每 5 秒自动更新' : '-'}</Descriptions.Item>
        </Descriptions>
      </Card>

      <Card title="AP 热点（SoftAP）" style={{ marginTop: 16 }}>
        <Descriptions column={2} bordered size="small">
          <Descriptions.Item label="热点名">{st?.ap_ssid || '-'}</Descriptions.Item>
          <Descriptions.Item label="访问地址">
            {st?.ap_ip ? `http://${st.ap_ip}` : '-'}
          </Descriptions.Item>
          <Descriptions.Item label="密码">无（开放网络）</Descriptions.Item>
          <Descriptions.Item label="说明">手机/电脑连接该热点后，即可访问配置页</Descriptions.Item>
        </Descriptions>
      </Card>
    </div>
  )
}
