#include "formatter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLOR_WHITE 0xFFFFFFu
#define COLOR_RED   0xFF0000u /* 红涨 */
#define COLOR_GREEN 0x00FF00u /* 绿跌 */
#define COLOR_GRAY  0x888888u

static double value_as_number(const cJSON *value, const char **str_out)
{
    if (cJSON_IsNumber(value)) {
        return value->valuedouble;
    }
    if (cJSON_IsString(value) && value->valuestring) {
        /* 字符串里若能整体转成数字则按数字处理，否则原样输出 */
        char *end = NULL;
        double d = strtod(value->valuestring, &end);
        if (end && *end == '\0' && end != value->valuestring) {
            return d;
        }
    }
    if (str_out) {
        *str_out = value->valuestring;
    }
    return 0;
}

void formatter_apply(const cJSON *value, const widget_t *w, formatted_t *out)
{
    memset(out, 0, sizeof(*out));
    out->color = COLOR_WHITE;

    if (!value || cJSON_IsNull(value)) {
        snprintf(out->text, sizeof(out->text), "--");
        return;
    }

    /* 非数字字段：原样显示 + 单位 */
    const char *raw_str = NULL;
    double num = value_as_number(value, &raw_str);
    if (raw_str) {
        snprintf(out->text, sizeof(out->text), "%s%s", raw_str, w->unit);
        return;
    }

    switch (w->format) {
    case FORMAT_PERCENT:
        snprintf(out->text, sizeof(out->text), "%+.*f%%", w->decimal_places, num);
        break;
    case FORMAT_DECIMAL:
        snprintf(out->text, sizeof(out->text), "%.*f%s", w->decimal_places, num, w->unit);
        break;
    default:
        if (num == (int)num) {
            snprintf(out->text, sizeof(out->text), "%d%s", (int)num, w->unit);
        } else {
            snprintf(out->text, sizeof(out->text), "%g%s", num, w->unit);
        }
        break;
    }

    /* 涨跌颜色：正红负绿 */
    if (w->use_change_color) {
        if (num > 0) {
            out->color = COLOR_RED;
        } else if (num < 0) {
            out->color = COLOR_GREEN;
        } else {
            out->color = COLOR_GRAY;
        }
    }
}
