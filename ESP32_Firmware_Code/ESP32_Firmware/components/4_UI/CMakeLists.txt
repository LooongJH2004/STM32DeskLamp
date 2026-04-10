# components/4_UI/CMakeLists.txt

idf_component_register(
    SRCS "src/ui_port_disp.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd driver lvgl  # 依赖官方 LCD 驱动和 lvgl 组件
)
