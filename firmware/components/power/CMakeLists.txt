idf_component_register(
    SRCS "power_mgmt.c" "battery_monitor.c"
    INCLUDE_DIRS "include"
    REQUIRES log esp_adc esp_hw_support driver
)
