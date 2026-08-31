#include "layout_renderer.h"

#include <string.h>

#include "cJSON.h"
#include "display.h"
#include "field_parser.h"
#include "formatter.h"

void layout_render(const app_config_t *cfg, uint32_t app_index, const char *const *bodies, const bool *has_body, int *trend_out)
{
    if (app_index >= cfg->app_count) {
        return;
    }
    const app_t *app = &cfg->apps[app_index];

    /* 只清画布区，不动顶部状态栏 */
    display_fill_rect(0, STATUS_BAR_HEIGHT, CANVAS_WIDTH, CANVAS_HEIGHT, 0x000000);

    if (trend_out) {
        *trend_out = 0;
    }

    /* 先解析各接口数据（同一接口只解析一次） */
    cJSON *parsed[CONFIG_INTERFACE_MAX] = {0};
    for (uint32_t i = 0; i < cfg->interface_count; i++) {
        if (has_body && has_body[i] && bodies && bodies[i] && strlen(bodies[i]) > 0) {
            parsed[i] = cJSON_Parse(bodies[i]);
        }
    }

    for (uint32_t i = 0; i < app->widget_count; i++) {
        const widget_t *w = &app->widgets[i];

        /* 按接口 ID 找到对应的接口索引 */
        int idx = -1;
        for (uint32_t k = 0; k < cfg->interface_count; k++) {
            if (cfg->interfaces[k].id == w->interface_id) {
                idx = (int)k;
                break;
            }
        }
        if (idx < 0 || !parsed[idx]) {
            continue; /* 数据源缺失或数据未就绪 */
        }

        cJSON *val = json_get_by_path(parsed[idx], w->field_path);
        formatted_t out;
        formatter_apply(val, w, &out);

        /* 涨跌趋势：取第一个涨跌色数字字段的符号（供状态灯随涨跌变色） */
        if (trend_out && w->use_change_color && cJSON_IsNumber(val)) {
            double d = val->valuedouble;
            *trend_out = (d > 0) ? 1 : ((d < 0) ? -1 : 0);
        }

        char line[192];
        if (strlen(w->label) > 0) {
            snprintf(line, sizeof(line), "%s %s", w->label, out.text);
        } else {
            snprintf(line, sizeof(line), "%s", out.text);
        }
        display_draw_text(w->x, w->y + STATUS_BAR_HEIGHT, line, w->font_size, out.color);
    }

    for (uint32_t i = 0; i < CONFIG_INTERFACE_MAX; i++) {
        if (parsed[i]) {
            cJSON_Delete(parsed[i]);
        }
    }
}
