#include "app_ui.h"

#include <stdio.h>
#include <string.h>

#include "display.h"

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

    const int row_top = STATUS_BAR_HEIGHT + 14;
    const int row_h = 14;
    for (int i = 0; i < count && row_top + i * row_h + 12 <= DISPLAY_HEIGHT; i++) {
        int ry = row_top + i * row_h;
        if (i == cursor) {
            display_fill_rect(2, ry, CANVAS_WIDTH - 4, 12, COL_HL_BG);
            display_draw_text(8, ry + 2, names[i], 8, COL_FG);
        } else {
            display_draw_text(8, ry + 2, names[i], 8, COL_DIM);
        }
    }
}

void app_ui_draw_system(int view, int cursor, uint8_t brightness,
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

        static const char *const labels[SYS_ITEM_COUNT] = { "Brightness", "Refresh", "Status" };
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

        display_draw_text(8, STATUS_BAR_HEIGHT + 70, "OK/BACK: back", 8, COL_DIM);
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
