set(priv_requires "bean_context" "driver" "freertos" "esp_adc")
idf_component_register(SRCS "bean_battery.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES ${priv_requires})
