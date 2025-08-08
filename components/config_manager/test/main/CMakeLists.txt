idf_component_register(
    SRCS "main_test.c"
        "test_config_manager.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity nvs_flash config_manager
)
