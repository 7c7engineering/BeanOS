set(priv_requires "bean_context" "freertos" "esp_adc" "esp_driver_gpio")
idf_component_register(SRCS "bean_battery.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES ${priv_requires})
