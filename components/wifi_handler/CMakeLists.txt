idf_component_register(SRCS "wifi_handler.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi mdns status_handler)
