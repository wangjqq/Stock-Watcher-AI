import type { FieldInfo } from '../types'

/** 各接口最近一次测试解析出的字段缓存（按接口 id），
 * 供「字段解析」和「屏幕布局」两页共享，避免跨页丢失预览数据。 */
const store = new Map<number, FieldInfo[]>()

export const fieldStore = {
  set(id: number, fields: FieldInfo[]) {
    store.set(id, fields)
  },
  get(id: number): FieldInfo[] {
    return store.get(id) ?? []
  },
}
