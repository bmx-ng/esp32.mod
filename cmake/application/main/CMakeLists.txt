set(ESP32_MODULE_ROOT "${BLITZMAX_ESP32_SDK}/mod/esp32.mod")
set(EMBEDDED_MODULE_ROOT "${BLITZMAX_ESP32_SDK}/mod/embedded.mod")
set(BRL_BLITZ_ROOT "${BLITZMAX_ESP32_SDK}/mod/brl.mod/blitz.mod")
set(GENERATED_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(GENERATED_BOOTSTRAP_C "${GENERATED_DIRECTORY}/blitzmax_esp32_modules.c")
set(GENERATED_ENTRY_C "${GENERATED_DIRECTORY}/blitzmax_esp32_entry.c")

if (DEFINED BLITZMAX_ESP32_NATIVE_CMAKE)
    include("${BLITZMAX_ESP32_NATIVE_CMAKE}")
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
    "${ESP32_MODULE_ROOT}/runtime.mod/native/esp32_runtime.c"
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
    PRIV_REQUIRES esp_hw_support esp_system freertos heap
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    RYU_OPTIMIZE_SIZE=1
    BMX_EMBEDDED_ARENA_SIZE=${BLITZMAX_ESP32_ARENA_SIZE}u
    BMX_EMBEDDED_ARENA_IN_PSRAM=0
)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=unused-const-variable
)

if (BLITZMAX_PICO_NATIVE_LINK_OPTIONS)
    target_link_options(${COMPONENT_LIB} PRIVATE ${BLITZMAX_PICO_NATIVE_LINK_OPTIONS})
endif()
