/** 当前选中的应用索引（跨「应用列表 / 字段解析 / 屏幕布局」共享，
 * 设备开机默认进入第一个应用，故默认值为 0）。 */
let current = 0

export const appStore = {
  set(index: number) {
    current = index
  },
  get(): number {
    return current
  },
}
