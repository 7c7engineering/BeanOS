set(priv_requires "bean_context" "freertos" "esp_driver_ledc")
idf_component_register(SRCS "bean_beep.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES ${priv_requires})
