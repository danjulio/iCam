file(GLOB SOURCES *.c)

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS . ../cmd ../esp32_utilities ../../main ../palettes ../tiny1c
                    EMBED_FILES index.html.gz favicon.ico
                    REQUIRES esp_event esp_netif esp_http_server esp_wifi icam_mini_specific)
