#pragma once
#include "ble.h"
#include "sd_rpc.h"

#include <string>

typedef struct
{
	uint8_t addr_type : 7;       /**< See @ref BLE_GAP_ADDR_TYPES. */
	uint8_t addr[BLE_GAP_ADDR_LEN]; /**< 48-bit address, LSB format. */
	int8_t  rssi;  /**< Received Signal Strength Indication in dBm. */
} addr_t;

typedef struct
{
	uint8_t *     p_data;   /**< Pointer to data. */
	uint16_t      data_len; /**< Length of data. */
} data_t;

typedef void(*fn_on_discovered)(std::string &addr_str, std::string &name, addr_t &addr);
typedef void(*fn_on_connected)(addr_t &addr);
typedef void(*fn_on_passkey_required)(std::string &passkey);
typedef void(*fn_on_authenticated)(uint8_t &status);
typedef void(*fn_on_service_discovered)(uint16_t &last_handle);
typedef void(*fn_on_service_enabled)();
typedef void(*fn_on_disconnected)(uint8_t &reason);
typedef void(*fn_on_failed)(std::string &stage); /* failure from connection or authentication */
typedef void(*fn_on_data_received)(uint16_t &handle, data_t &data);
typedef void(*fn_on_data_sent)(uint16_t &handle, data_t &data);

uint32_t set_callback(std::string fn_name, void* fn);

/*serial_port:"COMx", baud_rate:10000*/
uint32_t dongle_init(char* serial_port, uint32_t baud_rate);
/*interval:2.5~10240(ms), window:2.5~10240(ms), timeout:0(disable),1~65535(s)*/
uint32_t scan_start(float interval, float window, bool active, uint16_t timeout);
uint32_t conn_start(addr_t peer_addr);
/*io_caps:0x2(BLE_GAP_IO_CAPS_KEYBOARD_ONLY)*/
uint32_t auth_start(bool bond, bool keypress, uint8_t io_caps);
uint32_t service_discovery_start(uint16_t uuid, uint8_t type);
uint32_t service_enable_start();
uint32_t dongle_disconnect();
uint32_t dongle_close();
