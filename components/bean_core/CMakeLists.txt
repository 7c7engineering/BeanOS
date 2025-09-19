set(priv_requires "bean_context" "driver" "freertos" "bean_imu" "bean_led" "bean_altimeter")
idf_component_register(SRCS "bean_core.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES ${priv_requires})