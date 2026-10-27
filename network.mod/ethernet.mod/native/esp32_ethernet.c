#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "esp_eth.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_netif_glue.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "blitzmax/embedded_runtime.h"
#include "blitzmax/embedded_system.h"
#include "blitzmax_esp32_board.h"

#ifndef BMX_ESP32_BOARD_ETHERNET
#error "The selected ESP32 board profile does not define an Ethernet resource"
#endif

#define BMX_ETHERNET_EVENT_CAPACITY 16u
#define BMX_ETHERNET_EVENT_MASK (BMX_ETHERNET_EVENT_CAPACITY - 1u)
#define BMX_ETHERNET_LINK_STATE 1

#define BMX_ETHERNET_LINK_DOWN 0
#define BMX_ETHERNET_LINK_CONNECTED 1
#define BMX_ETHERNET_LINK_NO_IP 2
#define BMX_ETHERNET_LINK_UP 3

typedef struct BMXEthernetEvent {
    int32_t kind;
    int32_t link_status;
    uint32_t address;
    uint32_t netmask;
    uint32_t gateway;
    uint8_t mac[6];
    int32_t speed;
    int32_t full_duplex;
} BMXEthernetEvent;

static BMXEthernetEvent bmx_ethernet_events[BMX_ETHERNET_EVENT_CAPACITY];
static uint32_t bmx_ethernet_event_put;
static uint32_t bmx_ethernet_event_get;
static uint32_t bmx_ethernet_dropped;
static portMUX_TYPE bmx_ethernet_lock = portMUX_INITIALIZER_UNLOCKED;
static esp_netif_t *bmx_ethernet_netif;
static esp_eth_handle_t bmx_ethernet_handle;
static esp_eth_mac_t *bmx_ethernet_mac;
static esp_eth_phy_t *bmx_ethernet_phy;
static esp_eth_netif_glue_handle_t bmx_ethernet_glue;
static esp_event_handler_instance_t bmx_ethernet_event_handler;
static esp_event_handler_instance_t bmx_ethernet_ip_handler;
static bool bmx_ethernet_event_registered;
static bool bmx_ethernet_ip_registered;
static bool bmx_ethernet_started;
static bool bmx_ethernet_initialized;
static bool bmx_ethernet_has_link;
static int32_t bmx_ethernet_link_status;
static int32_t bmx_ethernet_speed;
static int32_t bmx_ethernet_full_duplex;
static uint8_t bmx_ethernet_mac_address[6];

extern uint32_t bmx_embedded_net_active_socket_count(void) __attribute__((weak));

static bool bmx_ethernet_queue(const BMXEthernetEvent *event) {
    bool queued = false;
    portENTER_CRITICAL(&bmx_ethernet_lock);
    if (bmx_ethernet_event_put - bmx_ethernet_event_get < BMX_ETHERNET_EVENT_CAPACITY) {
        bmx_ethernet_events[bmx_ethernet_event_put & BMX_ETHERNET_EVENT_MASK] = *event;
        ++bmx_ethernet_event_put;
        queued = true;
    } else if (bmx_ethernet_dropped != UINT32_MAX) {
        ++bmx_ethernet_dropped;
    }
    portEXIT_CRITICAL(&bmx_ethernet_lock);
    if (queued) bmx_embedded_system_wake();
    return queued;
}

static uint32_t bmx_ethernet_pack_ip(esp_ip4_addr_t address) {
    return address.addr;
}

static int32_t bmx_ethernet_speed_mbps(eth_speed_t speed) {
    switch (speed) {
        case ETH_SPEED_10M: return 10;
        case ETH_SPEED_100M: return 100;
        case ETH_SPEED_1000M: return 1000;
        default: return 0;
    }
}

static void bmx_ethernet_refresh_link_details(void) {
    if (!bmx_ethernet_handle) return;
    eth_speed_t speed;
    eth_duplex_t duplex;
    if (esp_eth_ioctl(bmx_ethernet_handle, ETH_CMD_G_SPEED, &speed) == ESP_OK) {
        bmx_ethernet_speed = bmx_ethernet_speed_mbps(speed);
    }
    if (esp_eth_ioctl(bmx_ethernet_handle, ETH_CMD_G_DUPLEX_MODE, &duplex) == ESP_OK) {
        bmx_ethernet_full_duplex = duplex == ETH_DUPLEX_FULL;
    }
    (void)esp_eth_ioctl(bmx_ethernet_handle, ETH_CMD_G_MAC_ADDR, bmx_ethernet_mac_address);
}

static void bmx_ethernet_queue_state(int32_t status, const esp_netif_ip_info_t *info) {
    BMXEthernetEvent event = {0};
    event.kind = BMX_ETHERNET_LINK_STATE;
    event.link_status = status;
    if (info) {
        event.address = bmx_ethernet_pack_ip(info->ip);
        event.netmask = bmx_ethernet_pack_ip(info->netmask);
        event.gateway = bmx_ethernet_pack_ip(info->gw);
    }
    memcpy(event.mac, bmx_ethernet_mac_address, sizeof(event.mac));
    event.speed = bmx_ethernet_speed;
    event.full_duplex = bmx_ethernet_full_duplex;
    bmx_ethernet_link_status = status;
    bmx_ethernet_queue(&event);
}

static void bmx_ethernet_event_callback(void *argument, esp_event_base_t base,
        int32_t id, void *data) {
    (void)argument;
    (void)data;
    if (base == ETH_EVENT) {
        switch (id) {
            case ETHERNET_EVENT_CONNECTED:
                bmx_ethernet_has_link = true;
                bmx_ethernet_refresh_link_details();
                bmx_ethernet_queue_state(BMX_ETHERNET_LINK_CONNECTED, NULL);
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                bmx_ethernet_has_link = false;
                bmx_ethernet_speed = 0;
                bmx_ethernet_full_duplex = false;
                bmx_ethernet_queue_state(BMX_ETHERNET_LINK_DOWN, NULL);
                break;
            case ETHERNET_EVENT_STOP:
                bmx_ethernet_has_link = false;
                bmx_ethernet_queue_state(BMX_ETHERNET_LINK_DOWN, NULL);
                break;
            default:
                break;
        }
    } else if (base == IP_EVENT) {
        if (id == IP_EVENT_ETH_GOT_IP) {
            ip_event_got_ip_t *event = data;
            bmx_ethernet_refresh_link_details();
            bmx_ethernet_queue_state(BMX_ETHERNET_LINK_UP,
                event ? &event->ip_info : NULL);
        } else if (id == IP_EVENT_ETH_LOST_IP) {
            bmx_ethernet_queue_state(bmx_ethernet_has_link ?
                BMX_ETHERNET_LINK_NO_IP : BMX_ETHERNET_LINK_DOWN, NULL);
        }
    }
}

#ifdef BMX_ESP32_BOARD_ETHERNET_PHY_YT8531
static esp_err_t bmx_ethernet_initialize_yt8531(esp_eth_handle_t handle) {
    bool auto_negotiation = true;
    esp_err_t result = esp_eth_ioctl(handle, ETH_CMD_S_AUTONEGO, &auto_negotiation);
    if (result != ESP_OK) return result;

    uint32_t value;
    esp_eth_phy_reg_rw_data_t reg = {.reg_value_p = &value};
    value = 0xA001;
    reg.reg_addr = 0x1E;
    result = esp_eth_ioctl(handle, ETH_CMD_WRITE_PHY_REG, &reg);
    if (result != ESP_OK) return result;
    reg.reg_addr = 0x1F;
    result = esp_eth_ioctl(handle, ETH_CMD_READ_PHY_REG, &reg);
    if (result != ESP_OK) return result;
    value |= 1u << 8;
    result = esp_eth_ioctl(handle, ETH_CMD_WRITE_PHY_REG, &reg);
    if (result != ESP_OK) return result;

    value = 0xA003;
    reg.reg_addr = 0x1E;
    result = esp_eth_ioctl(handle, ETH_CMD_WRITE_PHY_REG, &reg);
    if (result != ESP_OK) return result;
    reg.reg_addr = 0x1F;
    result = esp_eth_ioctl(handle, ETH_CMD_READ_PHY_REG, &reg);
    if (result != ESP_OK) return result;
    value = (value & ~0xffu) | (13u << 4) | 13u;
    return esp_eth_ioctl(handle, ETH_CMD_WRITE_PHY_REG, &reg);
}
#endif

static void bmx_ethernet_release(void) {
    if (bmx_ethernet_started && bmx_ethernet_handle) {
        (void)esp_eth_stop(bmx_ethernet_handle);
        bmx_ethernet_started = false;
    }
    if (bmx_ethernet_ip_registered) {
        (void)esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID,
            bmx_ethernet_ip_handler);
        bmx_ethernet_ip_registered = false;
    }
    if (bmx_ethernet_event_registered) {
        (void)esp_event_handler_instance_unregister(ETH_EVENT, ESP_EVENT_ANY_ID,
            bmx_ethernet_event_handler);
        bmx_ethernet_event_registered = false;
    }
    if (bmx_ethernet_glue) {
        (void)esp_eth_del_netif_glue(bmx_ethernet_glue);
        bmx_ethernet_glue = NULL;
    }
    if (bmx_ethernet_netif) {
        esp_netif_destroy(bmx_ethernet_netif);
        bmx_ethernet_netif = NULL;
    }
    if (bmx_ethernet_handle) {
        (void)esp_eth_driver_uninstall(bmx_ethernet_handle);
        bmx_ethernet_handle = NULL;
    }
    if (bmx_ethernet_mac) {
        bmx_ethernet_mac->del(bmx_ethernet_mac);
        bmx_ethernet_mac = NULL;
    }
    if (bmx_ethernet_phy) {
        bmx_ethernet_phy->del(bmx_ethernet_phy);
        bmx_ethernet_phy = NULL;
    }
}

int32_t bmx_embedded_ethernet_initialize(void) {
    if (bmx_ethernet_initialized) return ESP_OK;

    esp_err_t result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = -1;
    phy_config.reset_gpio_num = BMX_ESP32_BOARD_ETHERNET_RESET;

    eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    emac_config.smi_gpio.mdc_num = BMX_ESP32_BOARD_ETHERNET_MDC;
    emac_config.smi_gpio.mdio_num = BMX_ESP32_BOARD_ETHERNET_MDIO;
#ifdef BMX_ESP32_BOARD_ETHERNET_INTERFACE_RGMII
    emac_config.interface = EMAC_DATA_INTERFACE_RGMII;
    emac_config.clock_config.rgmii.clock_rx_gpio = BMX_ESP32_BOARD_ETHERNET_RX_CLOCK;
    emac_config.clock_config.rgmii.clock_tx_gpio = BMX_ESP32_BOARD_ETHERNET_TX_CLOCK;
    emac_config.clock_config.rgmii.clock_phy_ref_gpio = -1;
    emac_config.emac_dataif_gpio.rgmii.tx_ctl_num = BMX_ESP32_BOARD_ETHERNET_TX_CONTROL;
    emac_config.emac_dataif_gpio.rgmii.txd0_num = BMX_ESP32_BOARD_ETHERNET_TXD0;
    emac_config.emac_dataif_gpio.rgmii.txd1_num = BMX_ESP32_BOARD_ETHERNET_TXD1;
    emac_config.emac_dataif_gpio.rgmii.txd2_num = BMX_ESP32_BOARD_ETHERNET_TXD2;
    emac_config.emac_dataif_gpio.rgmii.txd3_num = BMX_ESP32_BOARD_ETHERNET_TXD3;
    emac_config.emac_dataif_gpio.rgmii.rx_ctl_num = BMX_ESP32_BOARD_ETHERNET_RX_CONTROL;
    emac_config.emac_dataif_gpio.rgmii.rxd0_num = BMX_ESP32_BOARD_ETHERNET_RXD0;
    emac_config.emac_dataif_gpio.rgmii.rxd1_num = BMX_ESP32_BOARD_ETHERNET_RXD1;
    emac_config.emac_dataif_gpio.rgmii.rxd2_num = BMX_ESP32_BOARD_ETHERNET_RXD2;
    emac_config.emac_dataif_gpio.rgmii.rxd3_num = BMX_ESP32_BOARD_ETHERNET_RXD3;
#else
#error "The selected Ethernet interface is not implemented by ESP32.Network.Ethernet"
#endif

    bmx_ethernet_mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
    if (!bmx_ethernet_mac) return ESP_FAIL;
    bmx_ethernet_phy = esp_eth_phy_new_generic(&phy_config);
    if (!bmx_ethernet_phy) {
        bmx_ethernet_release();
        return ESP_ERR_NO_MEM;
    }
    esp_eth_config_t driver_config = ETH_DEFAULT_CONFIG(bmx_ethernet_mac, bmx_ethernet_phy);
    result = esp_eth_driver_install(&driver_config, &bmx_ethernet_handle);
    if (result != ESP_OK) {
        bmx_ethernet_release();
        return result;
    }
#ifdef BMX_ESP32_BOARD_ETHERNET_PHY_YT8531
    result = bmx_ethernet_initialize_yt8531(bmx_ethernet_handle);
    if (result != ESP_OK) {
        bmx_ethernet_release();
        return result;
    }
#endif

    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
    bmx_ethernet_netif = esp_netif_new(&netif_config);
    if (!bmx_ethernet_netif) {
        bmx_ethernet_release();
        return ESP_ERR_NO_MEM;
    }
    bmx_ethernet_glue = esp_eth_new_netif_glue(bmx_ethernet_handle);
    if (!bmx_ethernet_glue) {
        bmx_ethernet_release();
        return ESP_ERR_NO_MEM;
    }
    result = esp_netif_attach(bmx_ethernet_netif, bmx_ethernet_glue);
    if (result != ESP_OK) {
        bmx_ethernet_release();
        return result;
    }
    result = esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID,
        bmx_ethernet_event_callback, NULL, &bmx_ethernet_event_handler);
    if (result != ESP_OK) {
        bmx_ethernet_release();
        return result;
    }
    bmx_ethernet_event_registered = true;
    result = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
        bmx_ethernet_event_callback, NULL, &bmx_ethernet_ip_handler);
    if (result != ESP_OK) {
        bmx_ethernet_release();
        return result;
    }
    bmx_ethernet_ip_registered = true;

    bmx_ethernet_event_put = bmx_ethernet_event_get = bmx_ethernet_dropped = 0;
    bmx_ethernet_has_link = false;
    bmx_ethernet_link_status = BMX_ETHERNET_LINK_DOWN;
    bmx_ethernet_speed = 0;
    bmx_ethernet_full_duplex = false;
    memset(bmx_ethernet_mac_address, 0, sizeof(bmx_ethernet_mac_address));
    result = esp_eth_start(bmx_ethernet_handle);
    if (result != ESP_OK) {
        bmx_ethernet_release();
        return result;
    }
    bmx_ethernet_started = true;
    bmx_ethernet_initialized = true;
    return ESP_OK;
}

int32_t bmx_embedded_ethernet_deinitialize(void) {
    if (!bmx_ethernet_initialized) return ESP_OK;
    if (bmx_embedded_net_active_socket_count &&
            bmx_embedded_net_active_socket_count()) return ESP_ERR_INVALID_STATE;
    bmx_ethernet_release();
    bmx_ethernet_initialized = false;
    bmx_ethernet_has_link = false;
    bmx_ethernet_link_status = BMX_ETHERNET_LINK_DOWN;
    bmx_ethernet_event_put = bmx_ethernet_event_get = 0;
    return ESP_OK;
}

int32_t bmx_embedded_ethernet_initialized(void) { return bmx_ethernet_initialized; }
int32_t bmx_embedded_ethernet_link_status(void) { return bmx_ethernet_link_status; }
void bmx_embedded_ethernet_service(void) {}

int32_t bmx_embedded_ethernet_take_event(int32_t *kind, int32_t *link_status,
        uint32_t *address, uint32_t *netmask, uint32_t *gateway, uint8_t *mac,
        int32_t *speed, int32_t *full_duplex) {
    portENTER_CRITICAL(&bmx_ethernet_lock);
    if (bmx_ethernet_event_get == bmx_ethernet_event_put) {
        portEXIT_CRITICAL(&bmx_ethernet_lock);
        return false;
    }
    BMXEthernetEvent event = bmx_ethernet_events[bmx_ethernet_event_get & BMX_ETHERNET_EVENT_MASK];
    ++bmx_ethernet_event_get;
    portEXIT_CRITICAL(&bmx_ethernet_lock);
    *kind = event.kind;
    *link_status = event.link_status;
    *address = event.address;
    *netmask = event.netmask;
    *gateway = event.gateway;
    memcpy(mac, event.mac, sizeof(event.mac));
    *speed = event.speed;
    *full_duplex = event.full_duplex;
    return true;
}

static uint32_t bmx_ethernet_address(int which) {
    if (!bmx_ethernet_initialized || !bmx_ethernet_netif) return 0;
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(bmx_ethernet_netif, &info) != ESP_OK) return 0;
    return bmx_ethernet_pack_ip(which == 0 ? info.ip : which == 1 ? info.netmask : info.gw);
}

uint32_t bmx_embedded_ethernet_ipv4_address(void) { return bmx_ethernet_address(0); }
uint32_t bmx_embedded_ethernet_ipv4_netmask(void) { return bmx_ethernet_address(1); }
uint32_t bmx_embedded_ethernet_ipv4_gateway(void) { return bmx_ethernet_address(2); }
uint32_t bmx_embedded_ethernet_dropped_events(void) { return bmx_ethernet_dropped; }
int32_t bmx_embedded_ethernet_link_speed(void) { return bmx_ethernet_speed; }
int32_t bmx_embedded_ethernet_full_duplex(void) { return bmx_ethernet_full_duplex; }

int32_t bmx_embedded_ethernet_mac_address(uint8_t *mac) {
    if (!mac) return ESP_ERR_INVALID_ARG;
    if (!bmx_ethernet_initialized) return ESP_ERR_INVALID_STATE;
    memcpy(mac, bmx_ethernet_mac_address, sizeof(bmx_ethernet_mac_address));
    return ESP_OK;
}

const BMXEmbeddedString *bmx_esp32_ethernet_result_name(int32_t result) {
    const char *name = esp_err_to_name((esp_err_t)result);
    return bmx_embedded_string_from_ascii(name, (int32_t)strlen(name));
}

int32_t bmx_esp32_ethernet_phy_address(int32_t *address) {
    if (!address) return ESP_ERR_INVALID_ARG;
    if (!bmx_ethernet_initialized || !bmx_ethernet_handle) return ESP_ERR_INVALID_STATE;
    return esp_eth_ioctl(bmx_ethernet_handle, ETH_CMD_G_PHY_ADDR, address);
}

int32_t bmx_esp32_ethernet_make_default(void) {
    if (!bmx_ethernet_initialized || !bmx_ethernet_netif) return ESP_ERR_INVALID_STATE;
    return esp_netif_set_default_netif(bmx_ethernet_netif);
}

int32_t bmx_esp32_ethernet_is_default(void) {
    return bmx_ethernet_initialized && bmx_ethernet_netif &&
        esp_netif_get_default_netif() == bmx_ethernet_netif;
}

int32_t bmx_esp32_ethernet_route_priority(void) {
    if (!bmx_ethernet_initialized || !bmx_ethernet_netif) return -1;
    return esp_netif_get_route_prio(bmx_ethernet_netif);
}

int32_t bmx_esp32_ethernet_set_route_priority(int32_t priority) {
    if (!bmx_ethernet_initialized || !bmx_ethernet_netif) return -1;
    return esp_netif_set_route_prio(bmx_ethernet_netif, priority);
}
