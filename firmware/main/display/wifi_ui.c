#include "wifi_ui.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "app_config.h"
#include "display.h"
#include "wifi_manager.h"

static const char *TAG = "wifi_ui";

/* ---------------- 常量 ---------------- */
#define MAX_SCAN        20                  /* 最多展示的扫描结果数 */
#define CONN_TIMEOUT_MS 15000               /* 连接总超时 */
#define AUTH_FAIL_MS    4000                /* 认证失败（密码错误）快速失败时间 */
#define LIST_ROW_H      18                  /* 列表行高 */
#define LIST_TOP        (STATUS_BAR_HEIGHT + 30) /* 列表第一行屏幕 y */
#define KB_ROW_Y0       56                  /* 键盘第一行（画布相对 y） */
#define KB_ROW_H        44                  /* 键盘行高 */
#define KB_ROWS         4

/* 颜色 */
#define COL_TITLE 0xFFD700
#define COL_HL_BG 0x1A3A5C
#define COL_FG    0xFFFFFF
#define COL_DIM   0x9AA0A6
#define COL_OK    0x2ECC71
#define COL_ERR   0xE74C3C
#define COL_OK_BG 0x1A4A2A /* 确认键底色 */
#define COL_WARN_BG 0x3A1A1A /* 退格/返回键底色 */

/* 画布相对 y → 屏幕 y（画布在状态栏以下） */
#define CY(y) (STATUS_BAR_HEIGHT + (y))

/* ---------------- 视图与连接状态 ---------------- */
typedef enum {
    CONN_NONE = 0,
    CONN_WAIT,   /* 连接中 */
    CONN_OK,     /* 连接成功 */
    CONN_FAIL,   /* 连接失败 */
} conn_state_t;

/* ---------------- 软键盘模型 ---------------- */
typedef enum {
    K_ACT_NONE = 0,
    K_ACT_CHAR,      /* 输入字符 */
    K_ACT_SHIFT,     /* 大小写切换 */
    K_ACT_BACKSPACE, /* 退格 */
    K_ACT_PAGE,      /* 字母/符号页切换 */
    K_ACT_SPACE,     /* 空格 */
    K_ACT_OK,        /* 确认（连接 / 下一步） */
    K_ACT_BACK,      /* 返回列表 */
} kb_action_t;

typedef struct {
    kb_action_t act;
    char ch; /* K_ACT_CHAR 时的字符；K_ACT_PAGE 时 0=字母页 1=符号页 */
    int  w;  /* 键宽 */
} kb_key_t;

typedef struct {
    int  n;
    int  offset;   /* 行左偏移（居中用） */
    kb_key_t keys[10];
} kb_row_t;

/* 字母页 */
static const kb_row_t s_kb_lets[KB_ROWS] = {
    { 10, 0, { {K_ACT_CHAR,'q',24},{K_ACT_CHAR,'w',24},{K_ACT_CHAR,'e',24},{K_ACT_CHAR,'r',24},
               {K_ACT_CHAR,'t',24},{K_ACT_CHAR,'y',24},{K_ACT_CHAR,'u',24},{K_ACT_CHAR,'i',24},
               {K_ACT_CHAR,'o',24},{K_ACT_CHAR,'p',24} } },
    { 9, 12, { {K_ACT_CHAR,'a',24},{K_ACT_CHAR,'s',24},{K_ACT_CHAR,'d',24},{K_ACT_CHAR,'f',24},
               {K_ACT_CHAR,'g',24},{K_ACT_CHAR,'h',24},{K_ACT_CHAR,'j',24},{K_ACT_CHAR,'k',24},
               {K_ACT_CHAR,'l',24} } },
    { 9, 12, { {K_ACT_SHIFT,0,24},{K_ACT_CHAR,'z',24},{K_ACT_CHAR,'x',24},{K_ACT_CHAR,'c',24},
               {K_ACT_CHAR,'v',24},{K_ACT_CHAR,'b',24},{K_ACT_CHAR,'n',24},{K_ACT_CHAR,'m',24},
               {K_ACT_BACKSPACE,0,24} } },
    { 4, 0,  { {K_ACT_PAGE,0,36},{K_ACT_BACK,0,36},{K_ACT_SPACE,0,84},{K_ACT_OK,0,84} } },
};

/* 数字/符号页 */
static const kb_row_t s_kb_syms[KB_ROWS] = {
    { 10, 0, { {K_ACT_CHAR,'1',24},{K_ACT_CHAR,'2',24},{K_ACT_CHAR,'3',24},{K_ACT_CHAR,'4',24},
               {K_ACT_CHAR,'5',24},{K_ACT_CHAR,'6',24},{K_ACT_CHAR,'7',24},{K_ACT_CHAR,'8',24},
               {K_ACT_CHAR,'9',24},{K_ACT_CHAR,'0',24} } },
    { 8, 0,  { {K_ACT_CHAR,'@',30},{K_ACT_CHAR,'.',30},{K_ACT_CHAR,'_',30},{K_ACT_CHAR,'-',30},
               {K_ACT_CHAR,'?',30},{K_ACT_CHAR,'!',30},{K_ACT_CHAR,'/',30},{K_ACT_CHAR,'=',30} } },
    { 9, 12, { {K_ACT_CHAR,'(',24},{K_ACT_CHAR,')',24},{K_ACT_CHAR,'[',24},{K_ACT_CHAR,']',24},
               {K_ACT_CHAR,'{',24},{K_ACT_CHAR,'}',24},{K_ACT_CHAR,'+',24},{K_ACT_CHAR,'*',24},
               {K_ACT_BACKSPACE,0,24} } },
    { 5, 0,  { {K_ACT_PAGE,1,36},{K_ACT_CHAR,',',24},{K_ACT_CHAR,':',24},{K_ACT_SPACE,0,84},
               {K_ACT_OK,0,72} } },
};

typedef struct {
    kb_action_t act;
    char ch;
    int  x, y, w, h; /* 屏幕坐标（含状态栏偏移） */
} kb_cell_t;

/* ---------------- 内部状态 ---------------- */
static bool    s_active = false;
static bool    s_exit_requested = false;
static bool    s_dirty = false;

static int     s_view = WIFI_VIEW_LIST;

/* 扫描结果 */
static wifi_ap_record_t s_aps[MAX_SCAN];
static int      s_ap_count = 0;
static bool     s_scanning = false;
static int      s_list_cursor = 0; /* 列表光标：0=Add SSID, 1..ap=AP, 末位=Rescan */
static int      s_list_top = 0;    /* 列表滚动窗口首项 */

/* 目标网络与密码 */
static char     s_target_ssid[CONFIG_SSID_MAX];
static char     s_pass_buf[CONFIG_PASS_MAX];
static bool     s_manual = false;    /* 手动输入 SSID 模式 */
static bool     s_edit_ssid = false; /* 密码页当前编辑 SSID 字段 */

/* 连接前快照（失败恢复用） */
static char     s_prev_ssid[CONFIG_SSID_MAX];
static char     s_prev_pass[CONFIG_PASS_MAX];

/* 连接状态 */
static conn_state_t s_conn_state = CONN_NONE;
static uint32_t s_conn_start_ms = 0;
static uint32_t s_result_until_ms = 0; /* 结果页显示截止，到点回列表 */
static int      s_conn_reason = 0;

/* 软键盘 */
static kb_cell_t s_cells[40];
static int      s_cell_count = 0;
static bool     s_kb_symbols = false;
static bool     s_kb_shift = false;

/* ---------------- 工具 ---------------- */

static void clear_canvas(void)
{
    display_fill_rect(0, STATUS_BAR_HEIGHT, CANVAS_WIDTH, CANVAS_HEIGHT, 0x000000);
}

/* 列表条目数 = 手动项 + AP + 重扫项 */
static int list_count(void)
{
    return s_ap_count + 2;
}

static int list_visible_rows(void)
{
    return (DISPLAY_HEIGHT - LIST_TOP) / LIST_ROW_H;
}

/* 截断字符串到 max_len 字符（ASCII 8px/字符） */
static void truncate_str(const char *src, char *dst, size_t max_len)
{
    size_t l = strlen(src);
    if (l > max_len) {
        l = max_len;
    }
    memcpy(dst, src, l);
    dst[l] = '\0';
}

/* 把密码尾部保留 keep 个字符显示（字段宽度有限） */
static void tail_str(const char *src, char *dst, size_t keep)
{
    size_t l = strlen(src);
    if (l > keep) {
        memcpy(dst, src + l - keep, keep);
        dst[keep] = '\0';
    } else {
        memcpy(dst, src, l);
        dst[l] = '\0';
    }
}

static void mark_dirty(void)
{
    s_dirty = true;
}

/* ---------------- 扫描 ---------------- */

static void start_scan(void)
{
    s_scanning = true;
    s_ap_count = 0;
    if (s_list_cursor >= list_count()) {
        s_list_cursor = 0;
    }
    if (s_list_top > s_list_cursor) {
        s_list_top = s_list_cursor;
    }
    wifi_manager_scan_start();
    mark_dirty();
}

/* ---------------- 连接 ---------------- */

static const char *reason_str(int reason)
{
    switch (reason) {
    case WIFI_REASON_AUTH_FAIL:             return "Auth failed (check pw)";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:return "Handshake timeout";
    case WIFI_REASON_NO_AP_FOUND:           return "No AP found";
    case WIFI_REASON_MIC_FAILURE:           return "MIC failure";
    default:                                return "Connect failed";
    }
}

static void start_connect(void)
{
    s_view = WIFI_VIEW_CONN;
    s_conn_state = CONN_WAIT;
    s_conn_reason = 0;
    s_result_until_ms = 0;
    s_conn_start_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if (s_scanning) {
        s_scanning = false;
        wifi_manager_scan_stop(); /* 释放扫描结果内存 */
    }
    ESP_LOGI(TAG, "connect to %s", s_target_ssid);
    wifi_manager_connect(s_target_ssid, s_pass_buf);
    mark_dirty();
}

/* ---------------- 软键盘 ---------------- */

static void build_cells(void)
{
    const kb_row_t *rows = s_kb_symbols ? s_kb_syms : s_kb_lets;
    int idx = 0;
    for (int r = 0; r < KB_ROWS; r++) {
        const kb_row_t *row = &rows[r];
        int x = row->offset;
        int y = CY(KB_ROW_Y0 + r * KB_ROW_H);
        for (int k = 0; k < row->n; k++) {
            s_cells[idx].act = row->keys[k].act;
            s_cells[idx].ch = row->keys[k].ch;
            s_cells[idx].w = row->keys[k].w;
            s_cells[idx].h = KB_ROW_H;
            s_cells[idx].x = x;
            s_cells[idx].y = y;
            x += row->keys[k].w;
            idx++;
        }
    }
    s_cell_count = idx;
}

static kb_cell_t *cell_hit(int x, int y)
{
    for (int i = 0; i < s_cell_count; i++) {
        kb_cell_t *c = &s_cells[i];
        if (x >= c->x && x < c->x + c->w && y >= c->y && y < c->y + c->h) {
            return c;
        }
    }
    return NULL;
}

/* 当前正在编辑的字段缓冲 */
static char *edit_buf(void)
{
    return s_edit_ssid ? s_target_ssid : s_pass_buf;
}

static size_t edit_buf_size(void)
{
    return s_edit_ssid ? (size_t)CONFIG_SSID_MAX : (size_t)CONFIG_PASS_MAX;
}

static void type_char(char c)
{
    char *buf = edit_buf();
    size_t len = strlen(buf);
    if (len < edit_buf_size() - 1) {
        buf[len] = c;
        buf[len + 1] = '\0';
        mark_dirty();
        if (!s_kb_symbols && s_kb_shift) {
            s_kb_shift = false; /* 手机键盘习惯：输入后复位大写 */
            mark_dirty();
        }
    }
}

static void del_char(void)
{
    char *buf = edit_buf();
    size_t len = strlen(buf);
    if (len > 0) {
        buf[len - 1] = '\0';
        mark_dirty();
    }
}

static void kb_press(kb_action_t act, char ch)
{
    switch (act) {
    case K_ACT_CHAR:
        type_char(ch);
        break;
    case K_ACT_SHIFT:
        s_kb_shift = !s_kb_shift;
        mark_dirty();
        break;
    case K_ACT_BACKSPACE:
        del_char();
        break;
    case K_ACT_PAGE:
        s_kb_symbols = !s_kb_symbols;
        s_kb_shift = false;
        build_cells();
        mark_dirty();
        break;
    case K_ACT_SPACE:
        type_char(' ');
        break;
    case K_ACT_OK:
        if (s_manual && s_edit_ssid) {
            if (s_target_ssid[0]) {
                s_edit_ssid = false; /* 手动模式：SSID 输入完进入密码 */
                mark_dirty();
            }
        } else {
            start_connect();
        }
        break;
    case K_ACT_BACK:
        s_view = WIFI_VIEW_LIST;
        mark_dirty();
        break;
    default:
        break;
    }
}

/* ---------------- 列表操作 ---------------- */

static void move_cursor(int d)
{
    int n = list_count();
    int nc = s_list_cursor + d;
    if (nc < 0) {
        nc = 0;
    }
    if (nc >= n) {
        nc = n - 1;
    }
    if (nc != s_list_cursor) {
        s_list_cursor = nc;
        mark_dirty();
    }
    /* 滚动窗口跟随光标 */
    int vis = list_visible_rows();
    if (s_list_cursor < s_list_top) {
        s_list_top = s_list_cursor;
    } else if (s_list_cursor >= s_list_top + vis) {
        s_list_top = s_list_cursor - vis + 1;
    }
}

static void pass_enter(bool manual)
{
    s_view = WIFI_VIEW_PASS;
    s_manual = manual;
    s_edit_ssid = manual;
    s_kb_symbols = false;
    s_kb_shift = false;
    if (manual) {
        s_target_ssid[0] = '\0';
    }
    s_pass_buf[0] = '\0';
    build_cells();
    mark_dirty();
}

static void select_current(void)
{
    int n = list_count();
    if (s_list_cursor == 0) {
        pass_enter(true); /* 手动输入 SSID */
    } else if (s_list_cursor == n - 1) {
        start_scan();     /* 重新扫描 */
    } else {
        const wifi_ap_record_t *ap = &s_aps[s_list_cursor - 1];
        strlcpy(s_target_ssid, (const char *)ap->ssid, sizeof(s_target_ssid));
        if (ap->authmode == WIFI_AUTH_OPEN) {
            s_pass_buf[0] = '\0';
            start_connect(); /* 开放网络无需密码 */
        } else {
            pass_enter(false);
        }
    }
}

/* ---------------- 对外接口 ---------------- */

void wifi_ui_enter(void)
{
    s_active = true;
    s_exit_requested = false;
    s_view = WIFI_VIEW_LIST;
    s_conn_state = CONN_NONE;
    s_result_until_ms = 0;
    s_manual = false;
    s_edit_ssid = false;

    /* 快照当前已保存的 WiFi 配置（连接失败时恢复） */
    /* cfg 约 60KB，不能放栈（main_task 栈仅 3.5KB）也不能放 .bss（会挤爆内部 RAM），须堆分配 */
    app_config_t *cfg = malloc(sizeof(app_config_t));
    if (cfg) {
        config_load(cfg);
        strlcpy(s_prev_ssid, cfg->ssid, sizeof(s_prev_ssid));
        strlcpy(s_prev_pass, cfg->password, sizeof(s_prev_pass));
        free(cfg);
    }

    s_list_cursor = 0;
    s_list_top = 0;
    start_scan();
    ESP_LOGI(TAG, "wifi page enter, scan start");
}

void wifi_ui_exit(void)
{
    s_active = false;
    s_exit_requested = false;
    s_view = WIFI_VIEW_LIST;
    if (s_scanning) {
        s_scanning = false;
        wifi_manager_scan_stop(); /* 释放扫描结果内存 */
    }
    s_ap_count = 0;
    s_cell_count = 0;
    mark_dirty();
}

void wifi_ui_tick(void)
{
    if (!s_active) {
        return;
    }
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);

    /* 扫描轮询 */
    if (s_scanning && wifi_manager_scan_done()) {
        s_scanning = false;
        s_ap_count = wifi_manager_scan_get_results(s_aps, MAX_SCAN);
        ESP_LOGI(TAG, "scan done, %d aps", s_ap_count);
        if (s_list_cursor >= list_count()) {
            s_list_cursor = 0;
        }
        if (s_list_top > s_list_cursor) {
            s_list_top = s_list_cursor;
        }
        mark_dirty();
    }

    /* 连接结果页显示一段时间后自动回列表 */
    if (s_view == WIFI_VIEW_CONN && s_result_until_ms != 0 && now >= s_result_until_ms) {
        s_result_until_ms = 0;
        s_view = WIFI_VIEW_LIST;
        mark_dirty();
        return;
    }

    if (s_view != WIFI_VIEW_CONN || s_conn_state != CONN_WAIT) {
        return;
    }

    /* 连接中：成功则保存配置 */
    if (wifi_manager_is_connected()) {
        s_conn_state = CONN_OK;
        s_result_until_ms = now + 1500;
        mark_dirty();
        /* cfg 约 60KB，不能放栈（main_task 栈仅 3.5KB）也不能放 .bss（会挤爆内部 RAM），须堆分配 */
        app_config_t *cfg = malloc(sizeof(app_config_t));
        if (cfg) {
            config_load(cfg);
            strlcpy(cfg->ssid, s_target_ssid, sizeof(cfg->ssid));
            strlcpy(cfg->password, s_pass_buf, sizeof(cfg->password));
            config_save(cfg);
            free(cfg);
        }
        ESP_LOGI(TAG, "connected %s, saved", s_target_ssid);
        return;
    }

    /* 失败判定：超时 / 认证失败（密码错）/ 找不到网络 */
    int reason = wifi_manager_last_disconnect_reason();
    uint32_t el = now - s_conn_start_ms;
    bool fail = el >= CONN_TIMEOUT_MS ||
                (reason == WIFI_REASON_AUTH_FAIL && el >= AUTH_FAIL_MS) ||
                (reason == WIFI_REASON_NO_AP_FOUND && el >= 5000);
    if (fail) {
        s_conn_state = CONN_FAIL;
        s_conn_reason = reason;
        s_result_until_ms = now + 2000;
        mark_dirty();
        ESP_LOGW(TAG, "connect fail reason=%d", reason);
        /* 失败后恢复之前保存的配置连接；首次配网失败则停止自动重试 */
        if (s_prev_ssid[0] && strcmp(s_prev_ssid, s_target_ssid) != 0) {
            wifi_manager_connect(s_prev_ssid, s_prev_pass);
        } else if (!s_prev_ssid[0]) {
            wifi_manager_disconnect();
        }
    }
}

void wifi_ui_knob(knob_event_t ev)
{
    if (!s_active) {
        return;
    }
    switch (s_view) {
    case WIFI_VIEW_LIST:
        switch (ev) {
        case KNOB_EV_LEFT:
            move_cursor(-1);
            break;
        case KNOB_EV_RIGHT:
            move_cursor(1);
            break;
        case KNOB_EV_OK:
            select_current();
            break;
        case KNOB_EV_BACK:
            s_exit_requested = true;
            break;
        default:
            break;
        }
        break;

    case WIFI_VIEW_PASS:
        switch (ev) {
        case KNOB_EV_LEFT:
            del_char();
            break;
        case KNOB_EV_RIGHT:
            s_kb_symbols = !s_kb_symbols;
            s_kb_shift = false;
            build_cells();
            mark_dirty();
            break;
        case KNOB_EV_OK:
            kb_press(K_ACT_OK, 0);
            break;
        case KNOB_EV_BACK:
            s_view = WIFI_VIEW_LIST;
            mark_dirty();
            break;
        default:
            break;
        }
        break;

    case WIFI_VIEW_CONN:
        if (ev == KNOB_EV_BACK || ev == KNOB_EV_OK) {
            s_view = WIFI_VIEW_LIST;
            mark_dirty();
        }
        break;
    default:
        break;
    }
}

void wifi_ui_touch(const touch_event_t *ev)
{
    if (!s_active) {
        return;
    }

    /* 边缘向内滑 = 返回：列表 → 退出本页；密码/连接页 → 回列表 */
    if (ev->ev == TOUCH_SWIPE_BACK) {
        if (s_view == WIFI_VIEW_LIST) {
            s_exit_requested = true;
        } else {
            s_view = WIFI_VIEW_LIST;
            mark_dirty();
        }
        return;
    }

    if (ev->ev != TOUCH_TAP) {
        return; /* WiFi 页不响应中部左右滑动 */
    }
    int x = ev->x, y = ev->y;

    switch (s_view) {
    case WIFI_VIEW_LIST: {
        if (y < LIST_TOP) {
            return;
        }
        int row = (y - LIST_TOP) / LIST_ROW_H;
        int entry = s_list_top + row;
        if (row >= list_visible_rows() || entry < 0 || entry >= list_count()) {
            return;
        }
        s_list_cursor = entry;
        select_current();
        break;
    }
    case WIFI_VIEW_PASS: {
        if (y < CY(KB_ROW_Y0)) {
            return;
        }
        kb_cell_t *cell = cell_hit(x, y);
        if (cell) {
            kb_press(cell->act, cell->ch);
        }
        break;
    }
    case WIFI_VIEW_CONN:
        s_view = WIFI_VIEW_LIST; /* 点按结果页回列表 */
        mark_dirty();
        break;
    default:
        break;
    }
}

/* ---------------- 绘制 ---------------- */

static void draw_list(void)
{
    clear_canvas();

    /* 标题行：WiFi + 当前连接状态（右对齐） */
    display_draw_text(4, STATUS_BAR_HEIGHT + 2, "WiFi", 8, COL_TITLE);
    bool conn = wifi_manager_is_connected();
    const char *cs = wifi_manager_connected_ssid();
    char st[40];
    if (conn && cs[0]) {
        snprintf(st, sizeof(st), "CONN:%s", cs);
    } else {
        snprintf(st, sizeof(st), "NOT CONNECTED");
    }
    char st2[25];
    truncate_str(st, st2, 24); /* 右侧区域约 192px = 24 字符 */
    display_draw_text(240 - (int)strlen(st2) * 8, STATUS_BAR_HEIGHT + 2, st2, 8,
                      conn ? COL_OK : COL_ERR);

    /* 第二行：扫描状态 / 操作提示 */
    display_draw_text(4, STATUS_BAR_HEIGHT + 12,
                      s_scanning ? "Scanning..." : "Pick a network, OK to connect", 8, COL_DIM);

    /* 网络列表（含顶部 Add SSID 与底部 Rescan） */
    int n = list_count();
    int vis = list_visible_rows();
    if (s_list_top < 0) {
        s_list_top = 0;
    }
    if (s_list_top > n - vis) {
        s_list_top = (n - vis > 0) ? (n - vis) : 0;
    }
    for (int i = 0; i < vis; i++) {
        int entry = s_list_top + i;
        if (entry >= n) {
            break;
        }
        int ry = LIST_TOP + i * LIST_ROW_H;
        bool sel = (entry == s_list_cursor);
        if (sel) {
            display_fill_rect(2, ry, CANVAS_WIDTH - 4, LIST_ROW_H, COL_HL_BG);
        }

        if (entry == 0) {
            display_draw_text(6, ry + 4, "+ Add SSID", 8, sel ? COL_FG : COL_TITLE);
        } else if (entry == n - 1) {
            display_draw_text(6, ry + 4, "Rescan", 8, sel ? COL_FG : COL_DIM);
        } else {
            const wifi_ap_record_t *ap = &s_aps[entry - 1];
            bool open = (ap->authmode == WIFI_AUTH_OPEN);
            bool mine = conn && cs[0] && strcmp((const char *)ap->ssid, cs) == 0;

            /* 右侧：RSSI + 安全标记 */
            char rssi_str[8];
            snprintf(rssi_str, sizeof(rssi_str), "%d", ap->rssi);
            int rx = 238 - (int)strlen(rssi_str) * 8;
            display_draw_text(rx, ry + 4, rssi_str, 8, COL_DIM);
            const char *sec = open ? "OPN" : "WPA";
            int sx = rx - 4 - (int)strlen(sec) * 8;
            display_draw_text(sx, ry + 4, sec, 8, open ? COL_OK : COL_DIM);

            /* 左侧：SSID（限制宽度避免与右侧重叠） */
            int max_c = (sx - 6 - 8) / 8; /* 可用像素 → 字符数 */
            if (max_c < 1) {
                max_c = 1;
            }
            if (max_c > 30) {
                max_c = 30;
            }
            char ssid[31];
            truncate_str((const char *)ap->ssid, ssid, (size_t)max_c);
            display_draw_text(6, ry + 4, ssid, 8, mine ? COL_OK : (sel ? COL_FG : COL_FG));
            if (mine) {
                display_draw_text(6 + (int)strlen(ssid) * 8 + 2, ry + 4, "*", 8, COL_OK);
            }
        }
    }
}

static void draw_pass(void)
{
    clear_canvas();

    /* SSID 字段 */
    display_draw_text(4, CY(0), "SSID", 8, COL_DIM);
    char ssid[25];
    truncate_str(s_target_ssid, ssid, 24);
    display_draw_text(4 + 5 * 8, CY(0), ssid, 8, COL_FG);

    /* 密码字段（手动 SSID 编辑阶段显示占位提示） */
    if (s_edit_ssid) {
        display_draw_text(4, CY(16), "PASS: -- (OK to next)", 8, COL_DIM);
    } else {
        display_draw_text(4, CY(16), "PASS", 8, COL_DIM);
        char show[25];
        tail_str(s_pass_buf, show, 24);
        display_draw_text(4 + 5 * 8, CY(16), show, 8, COL_FG);
    }

    /* 提示行 */
    display_draw_text(4, CY(32),
                      s_edit_ssid ? "OK:next <-:del BACK:back" : "OK:connect <-:del BACK:back",
                      8, COL_DIM);

    /* 软键盘 */
    for (int i = 0; i < s_cell_count; i++) {
        const kb_cell_t *c = &s_cells[i];
        uint32_t bg = 0x1E1E1E;
        if (c->act == K_ACT_OK) {
            bg = COL_OK_BG;
        } else if (c->act == K_ACT_BACK || c->act == K_ACT_BACKSPACE) {
            bg = COL_WARN_BG;
        }
        display_fill_rect(c->x + 1, c->y + 1, c->w - 2, c->h - 2, bg);

        char label[8];
        switch (c->act) {
        case K_ACT_CHAR:
            label[0] = (!s_kb_symbols && s_kb_shift) ? (char)toupper((unsigned char)c->ch) : c->ch;
            label[1] = '\0';
            break;
        case K_ACT_SHIFT:   strcpy(label, s_kb_shift ? "A" : "a"); break;
        case K_ACT_BACKSPACE: strcpy(label, "<-"); break;
        case K_ACT_PAGE:    strcpy(label, s_kb_symbols ? "abc" : "123"); break;
        case K_ACT_SPACE:   strcpy(label, "space"); break;
        case K_ACT_OK:      strcpy(label, "OK"); break;
        case K_ACT_BACK:    strcpy(label, "back"); break;
        default:            label[0] = '\0'; break;
        }
        int lw = (int)strlen(label) * 8;
        int lx = c->x + (c->w - lw) / 2;
        int ly = c->y + (c->h - 8) / 2;
        display_draw_text(lx, ly, label, 8, COL_FG);
    }
}

static void draw_conn(void)
{
    clear_canvas();
    display_draw_text(4, STATUS_BAR_HEIGHT + 6, "WiFi Connect", 8, COL_TITLE);

    char ssid[25];
    truncate_str(s_target_ssid, ssid, 24);
    display_draw_text(4, STATUS_BAR_HEIGHT + 22, ssid, 8, COL_FG);

    if (s_conn_state == CONN_WAIT) {
        display_draw_text(4, STATUS_BAR_HEIGHT + 42, "Connecting...", 8, COL_FG);
    } else if (s_conn_state == CONN_OK) {
        display_draw_text(4, STATUS_BAR_HEIGHT + 42, "Connected", 8, COL_OK);
        char ip[32];
        wifi_manager_ip_str(ip, sizeof(ip));
        char line[40];
        snprintf(line, sizeof(line), "IP: %s", ip);
        display_draw_text(4, STATUS_BAR_HEIGHT + 58, line, 8, COL_FG);
    } else {
        display_draw_text(4, STATUS_BAR_HEIGHT + 42, "Failed", 8, COL_ERR);
        display_draw_text(4, STATUS_BAR_HEIGHT + 58, reason_str(s_conn_reason), 8, COL_DIM);
    }
    display_draw_text(4, STATUS_BAR_HEIGHT + 88, "BACK: back", 8, COL_DIM);
}

void wifi_ui_draw(void)
{
    if (!s_active) {
        return;
    }
    switch (s_view) {
    case WIFI_VIEW_LIST: draw_list(); break;
    case WIFI_VIEW_PASS: draw_pass(); break;
    case WIFI_VIEW_CONN: draw_conn(); break;
    default:             break;
    }
    s_dirty = false;
}

bool wifi_ui_need_redraw(void)
{
    return s_dirty;
}

bool wifi_ui_exit_requested(void)
{
    return s_exit_requested;
}
