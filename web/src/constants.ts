/** 屏幕与画布参数（与固件 firmware/main/display/display.h 保持一致）
 *
 *  屏幕 128x160 (ST7735, 1.8")，顶部 16px 为状态栏（系统保留，不参与布局），
 *  状态栏以下 128x144 为可配置显示区域（canvas）。Widget 的 x/y/w/h 均为画布内像素坐标。
 */
export const DISPLAY_WIDTH = 128
export const DISPLAY_HEIGHT = 160
export const STATUS_BAR_HEIGHT = 16
export const CANVAS_WIDTH = 128
export const CANVAS_HEIGHT = DISPLAY_HEIGHT - STATUS_BAR_HEIGHT // 144

/** 前端画布预览放大倍数（按真实像素等比放大，便于查看与拖拽） */
export const CANVAS_SCALE = 2

/** 画布网格参考线间距（像素），仅用于预览辅助，非实际布局约束 */
export const GRID_SIZE = 16
