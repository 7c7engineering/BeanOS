set(priv_requires "bean_context" "fatfs" "esp_partition" "esp_rom" "spi_flash" "vfs" "soc")
idf_component_register(SRCS "bean_storage.c" "bean_storage_usb.c" "bean_storage_logger.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES ${priv_requires})
