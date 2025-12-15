idf_component_register(SRCS "wifi_handler.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi mdns sd_storage status_handler)
