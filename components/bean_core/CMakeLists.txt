set(priv_requires "bean_context" "driver" "freertos" "bean_IMU" "bean_LED" "bean_altimeter" "esp_timer")
idf_component_register(SRCS "bean_core.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES ${priv_requires})
