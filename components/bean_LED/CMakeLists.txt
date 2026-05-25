set(priv_requires "bean_context" "freertos" "esp_driver_ledc")
idf_component_register(SRCS "bean_led.c"
                    INCLUDE_DIRS "include"
                    REQUIRES ${priv_requires})
