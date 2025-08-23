set(priv_requires "bean_context" "driver" "freertos")
idf_component_register(SRCS "bean_led.c"
                    INCLUDE_DIRS "include"
                    REQUIRES ${priv_requires})
