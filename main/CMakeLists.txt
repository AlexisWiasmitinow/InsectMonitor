idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "." 
                    PRIV_REQUIRES   camera sd_storage webpage wifi_handler 
                                    nvs_flash status_handler esp_timer 
                                    systime_handler utils box_controller tg_bot)
