idf_component_register(
    SRCS "src/data_center.c"
    INCLUDE_DIRS "include" "../../main"  # <--- 【关键修改】添加这一项
    PRIV_REQUIRES 5_Utils
)
