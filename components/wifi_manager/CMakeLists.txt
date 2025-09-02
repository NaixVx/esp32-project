idf_component_register(SRCS "src/wifi_manager.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_wifi esp_event config_manager utils)
