# components/2_Device/CMakeLists.txt

idf_component_register(
    SRCS "src/dev_audio.c" "src/dev_stm32.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2s json 1_DataRepo# 必须显式依赖 driver 和 esp_driver_i2s 
)

