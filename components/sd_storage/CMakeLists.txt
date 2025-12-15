idf_component_register(SRCS "sd_storage_csv.c" "sd_storage.c" "sd_storage_data_logs.c"
                    INCLUDE_DIRS "."
                    REQUIRES vfs fatfs
                    PRIV_REQUIRES json)
