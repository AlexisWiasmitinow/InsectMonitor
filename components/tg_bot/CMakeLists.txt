idf_component_register(SRCS "tg_bot.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_http_client json mbedtls)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-but-set-parameter)