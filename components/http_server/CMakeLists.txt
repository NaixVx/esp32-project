idf_component_register(SRCS "src/http_server.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_server config_manager)
