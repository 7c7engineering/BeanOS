idf_component_register(
    SRCS "bean_context.c"
    INCLUDE_DIRS "include"
    REQUIRES "cjson"
    EMBED_FILES "default.json"
)
