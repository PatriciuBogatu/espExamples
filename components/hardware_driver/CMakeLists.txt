
if(CONFIG_ESP32_S3_BOX_3_BOARD)
    list(APPEND BSP_BOARD_SRC "./boards/esp32s3-box-3")
endif()




idf_component_register(
    SRC_DIRS
        ./
        ${BSP_BOARD_SRC}
    INCLUDE_DIRS
        "include"
        "boards/include"
    REQUIRES
        driver
        fatfs
        spiffs
        )

component_compile_options(-w)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
