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

/** 崩溃原因码 → 中文描述（与固件 esp_reset_reason_t 对应，见 crashlog.c） */
const CRASH_REASON_TEXT: Record<number, string> = {
  4: '程序异常 Panic',
  5: '中断看门狗超时',
  6: '任务看门狗超时',
  7: '看门狗复位',
  9: '电压跌落 Brownout',
  14: '电源波动 Power Glitch',
  15: 'CPU 锁死 Lockup',
}

/** 最后崩溃原因展示（有崩溃记录时映射为中文，未知码回退固件返回的短描述） */
function crashReasonText(st: DeviceStatus | null) {
  if (!st) return '-'
  if (!st.crash_count || st.last_crash_code === 0) return '无异常复位记录'
  return CRASH_REASON_TEXT[st.last_crash_code] ?? (st.last_crash_reason || '未知原因')
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

      <Card title="稳定性 / 崩溃日志" style={{ marginTop: 16 }}>
        <Descriptions column={2} bordered size="small">
          <Descriptions.Item label="崩溃次数">
            {st ? (
              st.crash_count > 0 ? (
                <Tag color="red">{st.crash_count}</Tag>
              ) : (
                <Tag color="green">0</Tag>
              )
            ) : (
              '-'
            )}
          </Descriptions.Item>
          <Descriptions.Item label="最后崩溃原因">
            <Tag color={st && st.crash_count > 0 ? 'orange' : 'default'}>{crashReasonText(st)}</Tag>
          </Descriptions.Item>
          <Descriptions.Item label="自动重启">
            <Badge status="processing" text="看门狗已启用（程序异常自动重启）" />
          </Descriptions.Item>
          <Descriptions.Item label="统计口径">
            仅统计异常复位（程序异常 / 看门狗超时 / 电压跌落等），正常重启、深度睡眠与掉电不计
          </Descriptions.Item>
        </Descriptions>
      </Card>
    </div>
  )
}
