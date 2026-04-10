# components/4_UI/CMakeLists.txt

idf_component_register(
    SRCS "src/ui_port_disp.c" "src/ui_port_touch.c" "src/ui_main.c" 
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd driver lvgl 1_DataRepo # 依赖官方 LCD 驱动和 lvgl 组件
)
