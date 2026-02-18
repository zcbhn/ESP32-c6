set(MAIN_SRCS "app_main.c")

if(CONFIG_NODE_TYPE_A)
    list(APPEND MAIN_SRCS "app_type_a.c")
endif()

if(CONFIG_NODE_TYPE_B)
    list(APPEND MAIN_SRCS "app_type_b.c")
endif()

idf_component_register(
    SRCS ${MAIN_SRCS}
    INCLUDE_DIRS "."
    REQUIRES sensor actuator control safety comm power config nvs_flash
)
