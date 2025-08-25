set(priv_requires "bean_context" "driver" "freertos")
idf_component_register(SRCS "bean_pyro.c"
                    INCLUDE_DIRS "include"
                    REQUIRES ${priv_requires})
