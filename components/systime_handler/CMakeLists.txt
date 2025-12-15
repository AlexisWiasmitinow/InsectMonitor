idf_component_register(SRCS "systime_handler.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES lwip esp_netif json)
