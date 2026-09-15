#include <stdint.h>
#include <string.h>

#include "blitzmax/embedded_device.h"
#include "blitzmax/embedded_time.h"
#include "esp_chip_info.h"
#include "esp_mac.h"
#include "esp_system.h"

#define BMX_ESP32_DEVICE_ID_BYTES 6u

static int32_t bmx_esp32_device_id(uint8_t identifier[BMX_ESP32_DEVICE_ID_BYTES]) {
    return esp_efuse_mac_get_default(identifier) == ESP_OK;
}

const BMXEmbeddedString *bmx_embedded_unique_device_id(void) {
    static const char hex[] = "0123456789ABCDEF";
    uint8_t identifier[BMX_ESP32_DEVICE_ID_BYTES];
    char text[BMX_ESP32_DEVICE_ID_BYTES * 2u];
    if (!bmx_esp32_device_id(identifier))
        return bmx_embedded_string_from_ascii("", 0);
    for (uint32_t index = 0; index < BMX_ESP32_DEVICE_ID_BYTES; ++index) {
        text[index * 2u] = hex[identifier[index] >> 4];
        text[index * 2u + 1u] = hex[identifier[index] & 15u];
    }
    return bmx_embedded_string_from_ascii(text, sizeof(text));
}

BMXEmbeddedArray *bmx_embedded_unique_device_id_bytes(void) {
    uint8_t identifier[BMX_ESP32_DEVICE_ID_BYTES];
    if (!bmx_esp32_device_id(identifier)) return &bmx_embedded_empty_array;
    BMXEmbeddedArray *result = bmx_embedded_array_new_1d(BMX_ESP32_DEVICE_ID_BYTES,
        sizeof(uint8_t), BMX_EMBEDDED_ARRAY_ELEMENT_VALUE, NULL, NULL);
    if (result != &bmx_embedded_empty_array)
        memcpy(bmx_embedded_array_data(result), identifier, sizeof(identifier));
    return result;
}

int32_t bmx_esp32_device_reset_reason_native(void) {
    return (int32_t)esp_reset_reason();
}

int32_t bmx_embedded_device_reset_reason(void) {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return BMX_EMBEDDED_RESET_REASON_POWER_ON;
        case ESP_RST_EXT:
        case ESP_RST_USB:
        case ESP_RST_JTAG: return BMX_EMBEDDED_RESET_REASON_EXTERNAL;
        case ESP_RST_SW: return BMX_EMBEDDED_RESET_REASON_SOFTWARE;
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT: return BMX_EMBEDDED_RESET_REASON_WATCHDOG;
        case ESP_RST_PANIC: return BMX_EMBEDDED_RESET_REASON_PANIC;
        case ESP_RST_DEEPSLEEP: return BMX_EMBEDDED_RESET_REASON_DEEP_SLEEP;
        case ESP_RST_BROWNOUT: return BMX_EMBEDDED_RESET_REASON_BROWNOUT;
        case ESP_RST_PWR_GLITCH: return BMX_EMBEDDED_RESET_REASON_POWER_GLITCH;
        case ESP_RST_CPU_LOCKUP: return BMX_EMBEDDED_RESET_REASON_CPU_LOCKUP;
        default: return BMX_EMBEDDED_RESET_REASON_UNKNOWN;
    }
}

int32_t bmx_embedded_device_reboot(uint32_t delay_ms) {
    if (delay_ms) bmx_embedded_sleep_milliseconds(delay_ms);
    esp_restart();
}

static esp_chip_info_t bmx_esp32_device_chip_info(void) {
    esp_chip_info_t info;
    esp_chip_info(&info);
    return info;
}

int32_t bmx_esp32_device_chip_model(void) {
    return (int32_t)bmx_esp32_device_chip_info().model;
}

const BMXEmbeddedString *bmx_esp32_device_chip_model_name(void) {
    const char *name;
    switch (bmx_esp32_device_chip_info().model) {
        case CHIP_ESP32: name = "ESP32"; break;
        case CHIP_ESP32S2: name = "ESP32-S2"; break;
        case CHIP_ESP32S3: name = "ESP32-S3"; break;
        case CHIP_ESP32C3: name = "ESP32-C3"; break;
        case CHIP_ESP32C2: name = "ESP32-C2"; break;
        case CHIP_ESP32C6: name = "ESP32-C6"; break;
        case CHIP_ESP32H2: name = "ESP32-H2"; break;
        case CHIP_ESP32P4: name = "ESP32-P4"; break;
        case CHIP_ESP32C61: name = "ESP32-C61"; break;
        case CHIP_ESP32C5: name = "ESP32-C5"; break;
        case CHIP_ESP32H21: name = "ESP32-H21"; break;
        case CHIP_ESP32H4: name = "ESP32-H4"; break;
        case CHIP_ESP32S31: name = "ESP32-S31"; break;
        default: name = "Unknown ESP32"; break;
    }
    return bmx_embedded_string_from_ascii(name, (int32_t)strlen(name));
}

uint32_t bmx_esp32_device_chip_revision(void) {
    return bmx_esp32_device_chip_info().revision;
}

uint32_t bmx_esp32_device_chip_cores(void) {
    return bmx_esp32_device_chip_info().cores;
}

uint32_t bmx_esp32_device_chip_features(void) {
    return bmx_esp32_device_chip_info().features;
}
