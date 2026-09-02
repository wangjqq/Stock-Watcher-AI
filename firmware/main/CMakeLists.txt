# 在配置阶段生成 web_assets.c：把 firmware/web_dist 下的前端产物以字节数组内嵌进固件
set(WEB_DIST_DIR "${CMAKE_CURRENT_LIST_DIR}/../web_dist")
set(WEB_ASSETS_C "${CMAKE_CURRENT_BINARY_DIR}/web_assets.c")
set(GEN_WEB_ASSETS "${CMAKE_CURRENT_LIST_DIR}/../tools/gen_web_assets.py")
execute_process(
    COMMAND python "${GEN_WEB_ASSETS}" "${WEB_DIST_DIR}" "${WEB_ASSETS_C}"
    RESULT_VARIABLE GEN_WEB_ASSETS_RESULT
)
if(NOT GEN_WEB_ASSETS_RESULT EQUAL 0)
    message(FATAL_ERROR
        "生成 web_assets.c 失败。请先构建前端：cd web && npm run build（产物输出到 firmware/web_dist）")
endif()

idf_component_register(
    SRCS
        "main.c"
        "common/app_config.c"
        "common/font_zh_table.c"
        "network/wifi_manager.c"
        "network/http_server.c"
        "network/ota.c"
        "network/data_fetcher.c"
        "data/field_parser.c"
        "data/formatter.c"
        "display/display.c"
        "display/font_zh.c"
        "display/app_ui.c"
        "display/wifi_ui.c"
        "display/layout_renderer.c"
        "display/status_bar.c"
        "hardware/knob.c"
        "hardware/buzzer.c"
        "hardware/led.c"
        "hardware/indicator.c"
        "hardware/selftest.c"
        "hardware/battery.c"
        "hardware/light_sensor.c"
        "hardware/touch.c"
        "${WEB_ASSETS_C}"
    INCLUDE_DIRS
        "."
        "common"
        "network"
        "data"
        "display"
        "hardware"
        "web"
    REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        esp_http_server
        esp_http_client
        esp_timer
        esp_pm
        json
        mdns
        driver
        esp_driver_spi
        esp_driver_gpio
        esp_driver_ledc
        esp_adc
        app_update
        esp_partition
        heap
        lwip
)
