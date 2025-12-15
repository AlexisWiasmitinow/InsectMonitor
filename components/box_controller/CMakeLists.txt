idf_component_register(SRCS "box_controller.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES sensors_handler sd_storage esp_driver_ledc utils)
