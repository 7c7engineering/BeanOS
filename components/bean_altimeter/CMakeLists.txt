set(priv_requires "driver" "freertos" "esp_rom")
idf_component_register(SRCS "bean_altimeter.c" "BMP3/bmp3.c"
                    INCLUDE_DIRS "include" "BMP3"
                    PRIV_REQUIRES ${priv_requires})
