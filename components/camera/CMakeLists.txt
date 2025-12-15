idf_component_register(SRCS "camera_handler.c" "camera.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp32-camera sd_storage utils status_handler box_controller tg_bot)
