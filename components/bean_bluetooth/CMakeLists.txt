set(priv_requires "bean_context" "bean_storage")
idf_component_register(SRCS "bean_bluetooth.c" "gap.c" "gatt_svc.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "bt" "nvs_flash"
                    PRIV_REQUIRES ${priv_requires})
