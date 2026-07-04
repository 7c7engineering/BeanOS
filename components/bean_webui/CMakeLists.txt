set(priv_requires "bean_storage" "bean_altimeter" "bean_IMU" "bean_battery" "bean_LED" "bean_beep"
                  "esp_http_server" "esp_wifi" "esp_netif" "esp_event" "esp_app_format" "esp_timer"
                  "espressif__mdns")
idf_component_register(SRCS "bean_webui.c" "bean_webui_wifi.c" "bean_webui_http.c"
                            "bean_webui_ws.c" "bean_webui_sampler.c"
                    INCLUDE_DIRS "include"
                    REQUIRES bean_context
                    PRIV_REQUIRES ${priv_requires})

# Gzip the web UI at build time so the source stays directly editable.
# web/index.html is fully self-contained and also runs locally from file://
# during development (see api.md).
set(web_gz_dir ${CMAKE_CURRENT_BINARY_DIR}/web_gz)
file(MAKE_DIRECTORY ${web_gz_dir})
find_program(GZIP_PROG gzip REQUIRED)
add_custom_command(
    OUTPUT ${web_gz_dir}/index.html.gz
    COMMAND ${CMAKE_COMMAND} -E copy ${COMPONENT_DIR}/web/index.html ${web_gz_dir}/index.html
    COMMAND ${GZIP_PROG} -9 -n -f ${web_gz_dir}/index.html
    DEPENDS ${COMPONENT_DIR}/web/index.html
    VERBATIM)
target_add_binary_data(${COMPONENT_LIB} ${web_gz_dir}/index.html.gz BINARY
    DEPENDS ${web_gz_dir}/index.html.gz)
