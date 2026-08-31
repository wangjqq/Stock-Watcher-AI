import { defineConfig, type Plugin } from 'vite'
import react from '@vitejs/plugin-react'
import { gzipSync } from 'node:zlib'
import { readdirSync, statSync, readFileSync, writeFileSync, unlinkSync } from 'node:fs'
import { join, resolve } from 'node:path'

// 构建后将产物 gzip 压缩并删除原文件，供固件内嵌（减小 Flash 占用）
function gzipAssetsPlugin(): Plugin {
  const THRESHOLD = 1024
  let outDir = ''
  return {
    name: 'gzip-assets',
    apply: 'build',
    enforce: 'post',
    configResolved(config) {
      outDir = resolve(config.root, config.build.outDir)
    },
    closeBundle() {
      const walk = (dir: string) => {
        for (const name of readdirSync(dir)) {
          const p = join(dir, name)
          const st = statSync(p)
          if (st.isDirectory()) {
            walk(p)
          } else if (st.size > THRESHOLD) {
            writeFileSync(`${p}.gz`, gzipSync(readFileSync(p), { level: 9 }))
            unlinkSync(p)
          }
        }
      }
      walk(outDir)
    },
  }
}

// 构建产物直接输出到固件目录 firmware/web_dist，由固件构建时内嵌进固件
export default defineConfig({
  plugins: [react(), gzipAssetsPlugin()],
  base: './',
  server: {
    host: true,
    port: 5173,
    // 本地开发时，把 /api 代理到设备（可改成设备 IP）
    proxy: {
      '/api': 'http://stockwatcher.local',
    },
  },
  build: {
    outDir: '../firmware/web_dist',
    emptyOutDir: true,
    assetsDir: 'assets',
  },
})
