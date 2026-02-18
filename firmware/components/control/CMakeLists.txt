idf_component_register(
    SRCS "pid.c" "scheduler.c" "adaptive_poll.c"
    INCLUDE_DIRS "include"
    REQUIRES log esp_timer newlib
)
