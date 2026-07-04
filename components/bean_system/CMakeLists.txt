set(priv_requires "bean_context" "freertos" "esp_driver_i2c")
idf_component_register(SRCS "systemio.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES ${priv_requires})
