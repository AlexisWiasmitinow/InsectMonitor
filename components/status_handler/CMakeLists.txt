idf_component_register(SRCS "status_handler.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES json utils sd_storage esp_wifi)