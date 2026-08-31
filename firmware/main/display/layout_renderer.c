#include "layout_renderer.h"

#include <string.h>

#include "cJSON.h"
#include "display.h"
#include "field_parser.h"
#include "formatter.h"

void layout_render(const app_config_t *cfg, const char *json_body)
{
    cJSON *root = cJSON_Parse(json_body);
    if (!root) {
        /* 只清可配置画布区（状态栏以上不动） */
        display_fill_rect(0, STATUS_BAR_HEIGHT, CANVAS_WIDTH, CANVAS_HEIGHT, 0x000000);
        return;
    }

    /* 只清可配置画布区，状态栏由 status_bar 单独绘制 */
    display_fill_rect(0, STATUS_BAR_HEIGHT, CANVAS_WIDTH, CANVAS_HEIGHT, 0x000000);

    for (uint32_t i = 0; i < cfg->widget_count; i++) {
        const widget_t *w = &cfg->widgets[i];
        cJSON *val = json_get_by_path(root, w->field_path);
        formatted_t out;
        formatter_apply(val, w, &out);
        /* 标签 + 数值 */
        char line[192];
        if (strlen(w->label) > 0) {
            snprintf(line, sizeof(line), "%s %s", w->label, out.text);
        } else {
            snprintf(line, sizeof(line), "%s", out.text);
        }
        /* x/y 为画布内像素坐标（0..127, 0..143），y 偏移到状态栏下方 */
        display_draw_text(w->x, w->y + STATUS_BAR_HEIGHT, line, w->font_size, out.color);
    }

    cJSON_Delete(root);
}
