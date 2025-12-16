idf_component_register(SRCS "remote_server.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_http_client json mbedtls esp_ringbuf camera)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-but-set-parameter)