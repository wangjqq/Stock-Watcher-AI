import { lazy, Suspense, useState } from 'react'
import { Layout, Menu, Typography } from 'antd'
import {
  ApiOutlined,
  AppstoreOutlined,
  LayoutOutlined,
  PartitionOutlined,
  SettingOutlined,
} from '@ant-design/icons'

// 页面级代码分割，按需加载
const DeviceSettings = lazy(() => import('./pages/DeviceSettings'))
const ApiConfig = lazy(() => import('./pages/ApiConfig'))
const FieldSelect = lazy(() => import('./pages/FieldSelect'))
const AppList = lazy(() => import('./pages/AppList'))
const ScreenLayout = lazy(() => import('./pages/ScreenLayout'))

const { Sider, Header, Content } = Layout

export default function App() {
  const [current, setCurrent] = useState('device')

  const PAGES: Record<string, React.ReactNode> = {
    device: <DeviceSettings />,
    api: <ApiConfig />,
    fields: <FieldSelect />,
    apps: <AppList onNavigate={setCurrent} />,
    layout: <ScreenLayout />,
  }

  return (
    <Layout style={{ minHeight: '100vh' }}>
      <Sider theme="dark">
        <div style={{ color: '#fff', textAlign: 'center', padding: 16, fontWeight: 600 }}>
          Stock Watcher
        </div>
        <Menu
          theme="dark"
          mode="inline"
          selectedKeys={[current]}
          onClick={(e) => setCurrent(e.key)}
          items={[
            { key: 'device', icon: <SettingOutlined />, label: '设备设置' },
            { key: 'api', icon: <ApiOutlined />, label: '接口配置' },
            { key: 'fields', icon: <PartitionOutlined />, label: '字段解析' },
            { key: 'apps', icon: <AppstoreOutlined />, label: '应用列表' },
            { key: 'layout', icon: <LayoutOutlined />, label: '屏幕布局' },
          ]}
        />
      </Sider>
      <Layout>
        <Header style={{ background: '#fff', paddingInline: 24 }}>
          <Typography.Title level={4} style={{ margin: 0, lineHeight: '64px' }}>
            股票盯盘设备配置
          </Typography.Title>
        </Header>
        <Content style={{ margin: 24 }}>
          <Suspense fallback={<Typography.Text type="secondary">加载中...</Typography.Text>}>
            {PAGES[current]}
          </Suspense>
        </Content>
      </Layout>
    </Layout>
  )
}
