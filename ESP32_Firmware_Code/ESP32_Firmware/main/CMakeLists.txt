# main/CMakeLists.txt

idf_component_register(
    SRCS "main.c" "app_config.h"
    INCLUDE_DIRS "."
    # 显式声明依赖 4_UI 和 lvgl
    REQUIRES 2_Device 3_Service 4_UI lvgl 
)
