idf_component_register(
    SRCS
        "src/OTAUpdateChecker.cpp"
    INCLUDE_DIRS
        "include"
    PRIV_REQUIRES esp_system nvs_flash app_update
)



include_directories("${PROJECT_DIR}/.pio/libdeps/${IDF_TARGET}/ArduinoJson/src")
target_include_directories(${COMPONENT_LIB} PRIVATE ${PROJECT_DIR}/.pio/libdeps/esp32dev/ArduinoJson/src)