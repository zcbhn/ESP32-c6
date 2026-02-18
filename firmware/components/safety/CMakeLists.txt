idf_component_register(
    SRCS "safety_monitor.c"
    INCLUDE_DIRS "include"
    REQUIRES log esp_timer
)
