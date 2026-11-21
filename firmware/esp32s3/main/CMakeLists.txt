get_filename_component(EULO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
idf_component_register(
    SRCS "main.cpp" "${EULO_ROOT}/src/core/bench_board.cpp"
    INCLUDE_DIRS "." "${EULO_ROOT}/src"
    PRIV_REQUIRES esp_tinyusb esp_driver_gpio esp_driver_ledc esp_timer
)
