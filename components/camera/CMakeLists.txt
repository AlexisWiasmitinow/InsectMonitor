idf_component_register(SRCS "camera_handler.c" "camera.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES utils status_handler box_controller remote_server
                    REQUIRES esp32-camera)
