idf_component_register(
    SRCS "nvs_config.c" "preset_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES log nvs_flash newlib
)
