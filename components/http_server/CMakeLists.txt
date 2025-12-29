idf_component_register(
    SRCS
        # "src/http_server.cpp"
        # "src/handlers/root_handler.cpp"
        # "src/handlers/device_handler.cpp"
        # "src/handlers/network_handler.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_http_server
        config_manager
        sensor_manager
        json
)
