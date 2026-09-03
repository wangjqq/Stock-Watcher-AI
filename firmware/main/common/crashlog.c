#include "crashlog.h"

#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "crash";

#define NS          "crash"
#define KEY_COUNT   "count"
#define KEY_REASON  "reason"

/* 运行期缓存（仅开机 init 时更新，NVS 读取开销小、避免重复打开） */
static uint32_t s_count = 0;
static int32_t  s_last_code = 0;

/* 判定异常复位（应计入崩溃）：程序异常 / 各类看门狗 / 电压跌落 / CPU 锁死 / 电源波动。
 * 正常重启（esp_restart）、上电、深度睡眠唤醒、软复位均不计数。 */
static bool is_crash_reason(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
    case ESP_RST_BROWNOUT:
    case ESP_RST_PWR_GLITCH:
    case ESP_RST_CPU_LOCKUP:
        return true;
    default:
        return false;
    }
}

const char *crashlog_reason_str(int32_t code)
{
    switch ((esp_reset_reason_t)code) {
    case ESP_RST_PANIC:      return "Panic";
    case ESP_RST_INT_WDT:    return "Int WDT";
    case ESP_RST_TASK_WDT:   return "Task WDT";
    case ESP_RST_WDT:        return "WDT";
    case ESP_RST_BROWNOUT:   return "Brownout";
    case ESP_RST_PWR_GLITCH: return "Power Glitch";
    case ESP_RST_CPU_LOCKUP: return "CPU Lockup";
    default:                 return "";
    }
}

void crashlog_init(void)
{
    /* 先读已有统计（旧版本无该命名空间时按 0 处理） */
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "nvs open failed");
        return;
    }
    nvs_get_u32(h, KEY_COUNT, &s_count);
    nvs_get_i32(h, KEY_REASON, &s_last_code);

    esp_reset_reason_t reason = esp_reset_reason();
    if (is_crash_reason(reason)) {
        s_count++;
        s_last_code = (int32_t)reason;
        nvs_set_u32(h, KEY_COUNT, s_count);
        nvs_set_i32(h, KEY_REASON, s_last_code);
        nvs_commit(h);
        ESP_LOGW(TAG, "crash detected: reason=%d (%s), total=%u",
                 (int)reason, crashlog_reason_str((int32_t)reason), (unsigned)s_count);
    } else {
        ESP_LOGI(TAG, "boot reason=%d (%s), crash total=%u",
                 (int)reason, crashlog_reason_str((int32_t)reason), (unsigned)s_count);
    }
    nvs_close(h);
}

uint32_t crashlog_get_count(void)
{
    return s_count;
}

int32_t crashlog_get_last_code(void)
{
    return s_last_code;
}
