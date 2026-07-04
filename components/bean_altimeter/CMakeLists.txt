set(priv_requires "bean_system" "esp_driver_i2c" "freertos" "esp_rom")
idf_component_register(SRCS "bean_altimeter.c" "BMP3/bmp3.c"
                    INCLUDE_DIRS "include" "BMP3"
                    PRIV_REQUIRES ${priv_requires})
