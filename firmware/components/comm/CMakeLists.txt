idf_component_register(
    SRCS "thread_node.c" "cbor_codec.c"
    INCLUDE_DIRS "include"
    REQUIRES log openthread esp_netif esp_event vfs
)
