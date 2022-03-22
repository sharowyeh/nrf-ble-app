#pragma once

#ifdef NRFBLELIBRARY_EXPORTS
#define NRFBLEAPI __declspec(dllexport)
#else
#define NRFBLEAPI __declspec(dllimport)
#endif
#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC extern
#endif

#define STRING_BUFFER_SIZE 50
#define DATA_BUFFER_SIZE 256

#include <string>

typedef enum {
	FN_ON_DISCOVERED,
	FN_ON_CONNECTED,
	FN_ON_PASSKEY_REQUIRED,
	FN_ON_AUTHENTICATED,
	FN_ON_SERVICE_DISCOVERED,
	FN_ON_SERVICE_ENABLED,
	FN_ON_DISCONNECTED,
	FN_ON_FAILED,
	FN_ON_DATA_RECEIVED,
	FN_ON_DATA_SENT
} fn_callback_id_t;

typedef void(*fn_on_discovered)(const char *addr_str, const char *name, 
	uint8_t addr_type, uint8_t addr[6], int8_t rssi);
typedef void(*fn_on_connected)(uint8_t addr_type, uint8_t addr[6]);
typedef void(*fn_on_passkey_required)(const char *passkey);
typedef void(*fn_on_authenticated)(uint8_t status);
typedef void(*fn_on_service_discovered)(uint16_t last_handle);
typedef void(*fn_on_service_enabled)();
typedef void(*fn_on_disconnected)(uint8_t reason);
typedef void(*fn_on_failed)(const char *stage); /* failure from connection or authentication */
typedef void(*fn_on_data_received)(uint16_t handle, uint8_t *data, uint16_t len);
typedef void(*fn_on_data_sent)(uint16_t handle, uint8_t *data, uint16_t len);

EXTERNC NRFBLEAPI uint32_t callback_add(fn_callback_id_t fn_id, void* fn);

/*serial_port:"COMx", baud_rate:10000*/
EXTERNC NRFBLEAPI uint32_t dongle_init(char* serial_port, uint32_t baud_rate);
/*interval:2.5~10240(ms), window:2.5~10240(ms), timeout:0(disable),1~65535(s)*/
EXTERNC NRFBLEAPI uint32_t scan_start(float interval, float window, bool active, uint16_t timeout);
EXTERNC NRFBLEAPI uint32_t scan_stop();
EXTERNC NRFBLEAPI uint32_t conn_start(uint8_t addr_type, uint8_t addr[6]);
/*io_caps:0x2(BLE_GAP_IO_CAPS_KEYBOARD_ONLY)*/
EXTERNC NRFBLEAPI uint32_t auth_start(bool bond, bool keypress, uint8_t io_caps);
EXTERNC NRFBLEAPI uint32_t service_discovery_start(uint16_t uuid, uint8_t type);
EXTERNC NRFBLEAPI uint32_t service_enable_start();
EXTERNC NRFBLEAPI uint32_t data_read(uint16_t handle, uint8_t *data, uint16_t *len);
/*overload for data_read(handle, *data)*/
EXTERNC NRFBLEAPI uint32_t data_read_by_report_ref(uint8_t *report_ref, uint8_t *data, uint16_t *len);
EXTERNC NRFBLEAPI uint32_t data_write(uint16_t handle, uint8_t *data, uint16_t len);
/*overload for data_write(handle, *data)*/
EXTERNC NRFBLEAPI uint32_t data_write_by_report_ref(uint8_t *report_ref, uint8_t *data, uint16_t len);
EXTERNC NRFBLEAPI uint32_t dongle_disconnect();
EXTERNC NRFBLEAPI uint32_t dongle_reset();
EXTERNC NRFBLEAPI uint32_t dongle_close();
