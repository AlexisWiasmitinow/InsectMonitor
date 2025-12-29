idf_component_register(SRCS "webpage.c" "webpage_storage.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES joltwallet__littlefs vfs esp_http_server status_handler systime_handler wifi_handler app_update box_controller camera)
