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
        display_clear(0x000000);
        display_update();
        return;
    }

    display_clear(0x000000);

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
        display_draw_text(w->x, w->y, line, w->font_size, out.color);
    }

    display_update();
    cJSON_Delete(root);
}
