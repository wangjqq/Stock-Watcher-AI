import { useEffect, useRef, useState } from 'react'
import { Alert, Button, Card, Progress, Space, Typography, message } from 'antd'
import { UploadOutlined } from '@ant-design/icons'
import { api } from '../api/client'

type Phase = 'idle' | 'uploading' | 'rebooting' | 'done'

export default function OtaUpgrade() {
  const [version, setVersion] = useState('')
  const [file, setFile] = useState<File | null>(null)
  const [pct, setPct] = useState(0)
  const [phase, setPhase] = useState<Phase>('idle')
  const pollRef = useRef<ReturnType<typeof setInterval> | null>(null)

  useEffect(() => {
    api
      .getStatus()
      .then((s) => setVersion(s.firmware_version))
      .catch(() => {})
    return () => {
      if (pollRef.current) clearInterval(pollRef.current)
    }
  }, [])

  const onPick = (e: React.ChangeEvent<HTMLInputElement>) => {
    setFile(e.target.files?.[0] ?? null)
    setPhase('idle')
    setPct(0)
  }

  const onUpgrade = () => {
    if (!file) return
    setPct(0)
    setPhase('uploading')
    const xhr = new XMLHttpRequest()
    xhr.open('POST', '/api/ota')
    xhr.setRequestHeader('Content-Type', 'application/octet-stream')
    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable) setPct(Math.round((e.loaded / e.total) * 100))
    }
    xhr.onload = () => {
      setPhase('rebooting')
      message.success('升级数据已接收，设备重启中...')
      // 轮询 /api/status，等设备升级完成后重新上线
      let tries = 0
      pollRef.current = setInterval(() => {
        tries++
        api
          .getStatus()
          .then((s) => {
            if (pollRef.current) {
              clearInterval(pollRef.current)
              pollRef.current = null
            }
            setVersion(s.firmware_version)
            setPct(100)
            setPhase('done')
            message.success(`升级完成，当前版本 ${s.firmware_version}`)
          })
          .catch(() => {
            if (tries > 60) {
              // 2 分钟仍没恢复
              if (pollRef.current) {
                clearInterval(pollRef.current)
                pollRef.current = null
              }
              setPhase('idle')
              message.warning('设备暂未恢复，请稍后刷新页面重试')
            }
          })
      }, 2000)
    }
    xhr.onerror = () => {
      setPhase('idle')
      message.error('上传失败，请重试')
    }
    xhr.send(file)
  }

  return (
    <div style={{ maxWidth: 560 }}>
      <Card title="固件升级">
        <Space direction="vertical" size="middle" style={{ width: '100%' }}>
          <div>
            当前固件版本：
            <Typography.Text strong>{version || '未知'}</Typography.Text>
          </div>

          <input type="file" accept=".bin" onChange={onPick} />

          <Space>
            <Button
              type="primary"
              icon={<UploadOutlined />}
              disabled={!file || phase === 'uploading' || phase === 'rebooting'}
              loading={phase === 'uploading'}
              onClick={onUpgrade}
            >
              开始升级
            </Button>
            <Typography.Text type="secondary">
              {phase === 'idle' && (file ? `已选择：${file.name}（${(file.size / 1024).toFixed(0)} KB）` : '请选择 .bin 固件')}
            </Typography.Text>
          </Space>

          {(phase === 'uploading' || phase === 'rebooting') && (
            <Progress percent={pct} status={phase === 'rebooting' ? 'active' : 'normal'} />
          )}
          {phase === 'done' && <Progress percent={100} status="success" />}

          <Alert
            type="info"
            showIcon
            message="升级说明"
            description="选择 idf.py build 生成的 app 固件（.bin）上传。设备会边收边写，校验通过后自动重启进入新固件，全程无需插线刷机。上传中断不影响当前固件。"
          />
        </Space>
      </Card>
    </div>
  )
}
