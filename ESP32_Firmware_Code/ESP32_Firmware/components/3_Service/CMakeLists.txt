# components/3_Service/CMakeLists.txt

idf_component_register(
    SRCS    "src/manager/mgr_wifi.c"
            "src/agents/agent_baidu_asr.c"
            "src/agents/agent_lampmind.c"
            "src/service_core.c"            
    INCLUDE_DIRS "include" "../../main"  # <--- 【关键修改】添加这一项
    # 添加 2_Device 到依赖列表
    REQUIRES esp_wifi esp_event nvs_flash lwip esp_netif esp_http_client json mbedtls 2_Device 5_Utils
)

