# components/5_Utils/CMakeLists.txt

idf_component_register(
    SRCS "src/event_bus.c" "src/ring_buffer.c"
    INCLUDE_DIRS "include"
    REQUIRES 1_DataRepo  # 依赖 system_types.h
)

