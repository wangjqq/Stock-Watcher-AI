# 本地 json 组件：cJSON v1.7.18（vendored，MIT）
# ESP-IDF v6.0 已把 json 组件从内核移出到组件仓库；本机无法访问组件仓库，
# 故在此以本地组件形式提供，供 main 的 REQUIRES json 使用。
idf_component_register(
    SRCS "cJSON.c"
    INCLUDE_DIRS "."
)
