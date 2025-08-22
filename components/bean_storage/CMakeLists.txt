set(priv_requires "fatfs" "esp_partition" "esp_rom" "spi_flash" "vfs" "soc")
idf_component_register(SRCS "bean_storage.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES ${priv_requires})
