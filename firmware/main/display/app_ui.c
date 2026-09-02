#include "app_ui.h"

#include <stdio.h>
#include <string.h>

#include "display.h"
#include "qr_admin.h"

/* 配色 */
#define COL_TITLE  0xFFD700 /* 金色标题 */
#define COL_HL_BG  0x1A3A5C /* 选中项背景（深蓝） */
#define COL_FG     0xFFFFFF
#define COL_DIM    0x9AA0A6
#define COL_BAR_FG 0x2ECC71 /* 亮度条 */
#define COL_BAR_BG 0x333333
#define COL_OK     0x2ECC71
#define COL_ERR    0xE74C3C

/* 只清画布区（状态栏以下），不动顶部状态栏 */
static void clear_canvas(void)
{
    display_fill_rect(0, STATUS_BAR_HEIGHT, CANVAS_WIDTH, CANVAS_HEIGHT, 0x000000);
}

/* 画一条水平进度条：x..x+w，value 0-100 */
static void draw_bar(int x, int y, int w, int h, uint8_t value)
{
    if (value > 100) {
        value = 100;
    }
    display_fill_rect(x, y, w, h, COL_BAR_BG);
    int fw = (int)((uint32_t)w * value / 100);
    if (fw > 0) {
        display_fill_rect(x, y, fw, h, COL_BAR_FG);
    }
}

void app_ui_draw_menu(const char names[][CONFIG_NAME_MAX], int count, int cursor)
{
    if (count < 1) {
        count = 1;
    }
    if (cursor < 0) {
        cursor = 0;
    }
    if (cursor >= count) {
        cursor = count - 1;
    }

    clear_canvas();
    display_draw_text(4, STATUS_BAR_HEIGHT + 4, "APPS", 8, COL_TITLE);

    /* 应用名可为中文：用 16px 中文混排，行距 16px（画布 144px 最多 8 行，与 CONFIG_APP_MAX 一致） */
    const int row_top = STATUS_BAR_HEIGHT + 14;
    const int row_h = 16;
    for (int i = 0; i < count && row_top + i * row_h + 16 <= DISPLAY_HEIGHT; i++) {
        int ry = row_top + i * row_h;
        if (i == cursor) {
            display_fill_rect(2, ry, CANVAS_WIDTH - 4, 16, COL_HL_BG);
            display_draw_text_zh(8, ry, names[i], 1, COL_FG);
        } else {
            display_draw_text_zh(8, ry, names[i], 1, COL_DIM);
        }
    }
}

void app_ui_draw_system(int view, int cursor, uint8_t brightness, bool auto_brightness,
                        bool wifi_ok, int rssi, const char *ip,
                        const char *version, bool refreshing)
{
    clear_canvas();

    if (view == SYS_VIEW_MENU) {
        /* 手动刷新提示：短暂覆盖菜单 */
        if (refreshing) {
            display_draw_text(24, STATUS_BAR_HEIGHT + 60, "Refreshing...", 8, COL_FG);
            return;
        }

        display_draw_text(4, STATUS_BAR_HEIGHT + 4, "SYSTEM", 8, COL_TITLE);

        static const char *const labels[SYS_ITEM_COUNT] = { "Brightness", "Refresh", "Status", "WiFi", "QR", "TouchCal" };
        const int row_top = STATUS_BAR_HEIGHT + 12;
        const int row_h = 20;
        for (int i = 0; i < SYS_ITEM_COUNT; i++) {
            int ry = row_top + i * row_h;
            if (i == cursor) {
                display_fill_rect(2, ry, CANVAS_WIDTH - 4, 12, COL_HL_BG);
                display_draw_text(8, ry + 2, labels[i], 8, COL_FG);
            } else {
                display_draw_text(8, ry + 2, labels[i], 8, COL_DIM);
            }
            if (i == 0) {
                /* 亮度行下方附加进度条 + 百分比 */
                char pct[16];
                snprintf(pct, sizeof(pct), "%d%%", brightness);
                draw_bar(12, ry + 14, 76, 4, brightness);
                display_draw_text(92, ry + 12, pct, 8, COL_DIM);
            }
        }
        return;
    }

    if (view == SYS_VIEW_BRIGHT) {
        display_draw_text(4, STATUS_BAR_HEIGHT + 8, "Brightness", 8, COL_TITLE);

        char pct[16];
        snprintf(pct, sizeof(pct), "%d%%", brightness);
        draw_bar(8, STATUS_BAR_HEIGHT + 28, 112, 12, brightness);
        display_draw_text(92, STATUS_BAR_HEIGHT + 42, pct, 8, COL_FG);

        /* 自动亮度开启时由光敏接管，旋转调节不可用 */
        display_draw_text(8, STATUS_BAR_HEIGHT + 70,
                          auto_brightness ? "Auto: sensor" : "OK/BACK: back", 8, COL_DIM);
        return;
    }

    if (view == SYS_VIEW_STATUS) {
        display_draw_text(4, STATUS_BAR_HEIGHT + 4, "Status", 8, COL_TITLE);

        char line[64];
        int y = STATUS_BAR_HEIGHT + 20;
        display_draw_text(8, y, "IP", 8, COL_DIM);
        y += 16;
        display_draw_text(8, y, (ip && ip[0]) ? ip : "-", 8, COL_FG);
        y += 16;
        snprintf(line, sizeof(line), "RSSI: %d dBm", rssi);
        display_draw_text(8, y, line, 8, COL_FG);
        y += 16;
        snprintf(line, sizeof(line), "Ver: %s", version ? version : "-");
        display_draw_text(8, y, line, 8, COL_FG);
        y += 16;
        display_draw_text(8, y, wifi_ok ? "WiFi ON" : "WiFi OFF", 8,
                          wifi_ok ? COL_OK : COL_ERR);
        y += 16;
        display_draw_text(8, y, "OK/BACK: back", 8, COL_DIM);
        return;
    }
}

/* 触摸标定页：stage 1=等左上角 2=等右下角 0=完成 */
void app_ui_draw_touch_cal(int stage)
{
    clear_canvas();
    display_draw_text(4, STATUS_BAR_HEIGHT + 4, "Touch Cal", 8, COL_TITLE);

    if (stage == 1) {
        display_draw_text(8, STATUS_BAR_HEIGHT + 40, "Tap top-left", 8, COL_FG);
        display_draw_text(8, STATUS_BAR_HEIGHT + 56, "corner", 8, COL_DIM);
    } else if (stage == 2) {
        display_draw_text(8, STATUS_BAR_HEIGHT + 40, "Tap bottom-right", 8, COL_FG);
        display_draw_text(8, STATUS_BAR_HEIGHT + 56, "corner", 8, COL_DIM);
    } else {
        display_draw_text(8, STATUS_BAR_HEIGHT + 40, "Calibrated", 8, COL_OK);
        display_draw_text(8, STATUS_BAR_HEIGHT + 56, "OK/BACK: back", 8, COL_DIM);
    }
}

/* ---------------- 管理页面二维码 ---------------- */

#define QR_CONTENT "http://stockwatcher.local" /* mDNS 域名，固定不变 */
#define QR_SCALE   6                           /* 每模块像素：(29+8)*6=222px，适配 240 宽画布 */

void app_ui_draw_qr(void)
{
    clear_canvas();

    int total = QR_ADMIN_SIZE + 2 * QR_ADMIN_QUIET;
    int px = total * QR_SCALE;
    int x0 = (CANVAS_WIDTH - px) / 2;
    int y0 = STATUS_BAR_HEIGHT + 18;

    /* 白底（含静区），保证二维码可被扫码识别 */
    display_fill_rect(x0, y0, px, px, 0xFFFFFF);

    /* 黑色模块：位图来自构建期预生成的 qr_admin.h，按位读取（1=黑） */
    for (int my = 0; my < QR_ADMIN_SIZE; my++) {
        for (int mx = 0; mx < QR_ADMIN_SIZE; mx++) {
            int idx = my * QR_ADMIN_SIZE + mx;
            if (qr_admin_modules[idx >> 3] & (1 << (7 - (idx & 7)))) {
                display_fill_rect(x0 + (QR_ADMIN_QUIET + mx) * QR_SCALE,
                                  y0 + (QR_ADMIN_QUIET + my) * QR_SCALE,
                                  QR_SCALE, QR_SCALE, 0x000000);
            }
        }
    }

    /* 下方 URL 文字（居中）+ 返回提示 */
    int lw = (int)strlen(QR_CONTENT) * 8;
    display_draw_text((CANVAS_WIDTH - lw) / 2, y0 + px + 10, QR_CONTENT, 8, COL_FG);
    display_draw_text(4, y0 + px + 26, "OK/BACK: back", 8, COL_DIM);
}
