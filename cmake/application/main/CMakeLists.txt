set(ESP32_MODULE_ROOT "${BLITZMAX_ESP32_SDK}/mod/esp32.mod")
set(EMBEDDED_MODULE_ROOT "${BLITZMAX_ESP32_SDK}/mod/embedded.mod")
set(BRL_BLITZ_ROOT "${BLITZMAX_ESP32_SDK}/mod/brl.mod/blitz.mod")
set(GENERATED_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(GENERATED_BOOTSTRAP_C "${GENERATED_DIRECTORY}/blitzmax_esp32_modules.c")
set(GENERATED_ENTRY_C "${GENERATED_DIRECTORY}/blitzmax_esp32_entry.c")

if (NOT DEFINED BLITZMAX_ESP32_ARENA_IN_PSRAM)
    set(BLITZMAX_ESP32_ARENA_IN_PSRAM 0)
endif()

if (DEFINED BLITZMAX_ESP32_NATIVE_CMAKE)
    include("${BLITZMAX_ESP32_NATIVE_CMAKE}")
endif()

set(BLITZMAX_ESP32_NETWORK_SOURCES)
set(BLITZMAX_ESP32_NETWORK_REQUIREMENTS)
if (BLITZMAX_ESP32_BLE_ENABLED)
    list(APPEND BLITZMAX_ESP32_NETWORK_SOURCES
        "${ESP32_MODULE_ROOT}/network.mod/ble.mod/native/esp32_ble.c")
    list(APPEND BLITZMAX_ESP32_NETWORK_REQUIREMENTS bt nvs_flash)
endif()
if (BLITZMAX_ESP32_WIFI_ENABLED)
    list(APPEND BLITZMAX_ESP32_NETWORK_SOURCES
        "${ESP32_MODULE_ROOT}/network.mod/wifi.mod/native/esp32_wifi.c")
    list(APPEND BLITZMAX_ESP32_NETWORK_REQUIREMENTS esp_event esp_netif esp_wifi nvs_flash)
endif()

file(MAKE_DIRECTORY "${GENERATED_DIRECTORY}")
file(WRITE "${GENERATED_BOOTSTRAP_C}" "/* Generated ESP32 module initialization order. */\n")
foreach(module_initializer IN LISTS BLITZMAX_ESP32_MODULE_INITIALIZERS)
    file(APPEND "${GENERATED_BOOTSTRAP_C}" "extern int ${module_initializer}(void);\n")
endforeach()
file(APPEND "${GENERATED_BOOTSTRAP_C}" "void bmx_embedded_modules_init(void) {\n")
foreach(module_initializer IN LISTS BLITZMAX_ESP32_MODULE_INITIALIZERS)
    file(APPEND "${GENERATED_BOOTSTRAP_C}" "    (void)${module_initializer}();\n")
endforeach()
file(APPEND "${GENERATED_BOOTSTRAP_C}" "}\n")

file(WRITE "${GENERATED_ENTRY_C}"
    "extern int main(void);\n"
    "void app_main(void) { (void)main(); }\n")

set(BLITZMAX_ESP32_SOURCES
    ${BLITZMAX_GENERATED_C}
    ${BLITZMAX_ESP32_GENERATED_SOURCES}
    ${BLITZMAX_PICO_NATIVE_SOURCES}
    "${GENERATED_BOOTSTRAP_C}"
    "${GENERATED_ENTRY_C}"
    "${EMBEDDED_MODULE_ROOT}/runtime.mod/native/embedded_runtime.c"
    "${EMBEDDED_MODULE_ROOT}/runtime.mod/native/embedded_string_integer.c"
    "${EMBEDDED_MODULE_ROOT}/runtime.mod/native/embedded_string_float.cpp"
    "${EMBEDDED_MODULE_ROOT}/runtime.mod/native/embedded_unicode.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_adc.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_calendar.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_device.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_runtime.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_events.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_gpio.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_i2c.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_nvs.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_ota.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_partition.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_random.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_pwm.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_power.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_spi.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_time.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_uart.c"
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_watchdog.c"
    ${BLITZMAX_ESP32_NETWORK_SOURCES}
    "${BRL_BLITZ_ROOT}/ryu/ryu/f2s.c"
    "${BRL_BLITZ_ROOT}/ryu/ryu/d2s.c"
)

idf_component_register(
    SRCS ${BLITZMAX_ESP32_SOURCES}
    INCLUDE_DIRS
        "${EMBEDDED_MODULE_ROOT}/runtime.mod/native/include"
        "${ESP32_MODULE_ROOT}/runtime.mod/native/include"
        "${BLITZMAX_ESP32_SDK}/mod"
        "${BLITZMAX_APPLICATION_ROOT}"
        "${BRL_BLITZ_ROOT}"
        "${BRL_BLITZ_ROOT}/ryu"
    PRIV_REQUIRES app_update bootloader_support esp_adc esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_driver_sdmmc esp_driver_spi esp_driver_uart esp_hw_support esp_partition esp_psram esp_system esp_timer fatfs freertos heap joltwallet__littlefs nvs_flash sdmmc ${BLITZMAX_ESP32_NETWORK_REQUIREMENTS}
)

if (BLITZMAX_ESP32_NET_ENABLED)
    idf_component_get_property(BLITZMAX_ESP32_LWIP_LIB lwip COMPONENT_LIB)
    idf_component_get_property(BLITZMAX_ESP32_NETIF_LIB esp_netif COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PRIVATE
        ${BLITZMAX_ESP32_LWIP_LIB}
        ${BLITZMAX_ESP32_NETIF_LIB})
endif()

if (BLITZMAX_ESP32_WIFI_ENABLED)
    idf_component_get_property(BLITZMAX_ESP32_ESP_EVENT_LIB esp_event COMPONENT_LIB)
    idf_component_get_property(BLITZMAX_ESP32_ESP_NETIF_LIB esp_netif COMPONENT_LIB)
    idf_component_get_property(BLITZMAX_ESP32_ESP_WIFI_LIB esp_wifi COMPONENT_LIB)
    idf_component_get_property(BLITZMAX_ESP32_NVS_FLASH_LIB nvs_flash COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PRIVATE
        ${BLITZMAX_ESP32_ESP_EVENT_LIB}
        ${BLITZMAX_ESP32_ESP_NETIF_LIB}
        ${BLITZMAX_ESP32_ESP_WIFI_LIB}
        ${BLITZMAX_ESP32_NVS_FLASH_LIB})
endif()

if (BLITZMAX_ESP32_BLE_ENABLED)
    idf_component_get_property(BLITZMAX_ESP32_BT_LIB bt COMPONENT_LIB)
    idf_component_get_property(BLITZMAX_ESP32_NVS_FLASH_LIB nvs_flash COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PRIVATE
        ${BLITZMAX_ESP32_BT_LIB}
        ${BLITZMAX_ESP32_NVS_FLASH_LIB})
endif()

if (DEFINED BLITZMAX_ESP32_BOARD_INCLUDE)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${BLITZMAX_ESP32_BOARD_INCLUDE}")
endif()

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    BMX_EMBEDDED=1
    BMX_ESP32=1
    RYU_OPTIMIZE_SIZE=1
    BMX_EMBEDDED_ARENA_SIZE=${BLITZMAX_ESP32_ARENA_SIZE}u
    BMX_EMBEDDED_ARENA_IN_PSRAM=${BLITZMAX_ESP32_ARENA_IN_PSRAM}
)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=unused-const-variable
    -Wno-error=clobbered
    -Wno-error=misleading-indentation
    -Wno-error=type-limits
)

if (BLITZMAX_PICO_NATIVE_LINK_OPTIONS)
    target_link_options(${COMPONENT_LIB} PRIVATE ${BLITZMAX_PICO_NATIVE_LINK_OPTIONS})
endif()
