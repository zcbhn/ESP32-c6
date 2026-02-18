idf_component_register(
    SRCS "sht30.c" "ds18b20.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer
)
