#include "dongle.h"
#include "ble.h"
//for nrf-ble-driver library compiling runtime library config /MT[d] or /MD[d],
//macro refer to https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=msvc-160
#if defined(_DLL) && !defined(_DEBUG)
// NOTICE: nordic offical static lib only support /MD
#if NRF_SD_BLE_API >= 6
#pragma comment(lib, "nrf-ble-driver-sd_api_v6-mt-static-4_1_4.lib")
#else
#pragma comment(lib, "nrf-ble-driver-sd_api_v5-mt-static-4_1_4.lib")
#endif
#else
// NOTICE: required nrf-ble-driver-sd_api_v5-mt-4_1_4.dll at output directory
#if NRF_SD_BLE_API >= 6
#pragma comment(lib, "nrf-ble-driver-sd_api_v6-mt-4_1_4.lib")
#else
#pragma comment(lib, "nrf-ble-driver-sd_api_v5-mt-4_1_4.lib")
#endif
#endif
#include "sd_rpc.h"
#include "security.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <time.h>
#include <chrono>

typedef struct _addr_t
{
	uint8_t addr_type : 7;       /**< See @ref BLE_GAP_ADDR_TYPES. default 7 from ble_gap_addr_t */
	uint8_t addr[6/*BLE_GAP_ADDR_LEN*/]; /**< 48-bit address, LSB format. */
	int8_t  rssi;  /**< Received Signal Strength Indication in dBm. */
} addr_t;

/* see also ble_data_t but fixed data memory allocation for ease of init */
typedef struct _data_t
{
	uint8_t  p_data[DATA_BUFFER_SIZE] = { 0 };   /**< Pointer to data. */
	uint16_t len; /**< Length of data. */
} data_t;

enum _uint_ms
{
	UNIT_0_625_MS = 625,  /**< Number of microseconds in 0.625 milliseconds. */
	UNIT_1_25_MS = 1250, /**< Number of microseconds in 1.25 milliseconds. */
	UNIT_10_MS = 10000 /**< Number of microseconds in 10 milliseconds. */
};

#define MSEC_TO_UNITS(TIME, RESOLUTION) (((TIME) * 1000) / (RESOLUTION))

#define SCAN_INTERVAL 0x00A0 /**< Determines scan interval in units of 0.625 milliseconds. */
#define SCAN_WINDOW   0x0050 /**< Determines scan window in units of 0.625 milliseconds. */
#define SCAN_TIMEOUT  0x0    /**< Scan timeout between 0x01 and 0xFFFF in seconds, 0x0 disables timeout. */

#define MIN_CONNECTION_INTERVAL         MSEC_TO_UNITS(7.5, UNIT_1_25_MS) /**< Determines minimum connection interval in milliseconds. */
#define MAX_CONNECTION_INTERVAL         MSEC_TO_UNITS(18.75, UNIT_1_25_MS) /**< Determines maximum connection interval in milliseconds. */
#define SLAVE_LATENCY                   0                                /**< Slave Latency in number of connection events. */
#define CONNECTION_SUPERVISION_TIMEOUT  MSEC_TO_UNITS(4000, UNIT_10_MS)  /**< Determines supervision time-out in units of 10 milliseconds. */

// service
#define BLE_UUID_BATTERY_SRV 0x180F
#define BLE_UUID_HID_SRV 0x1812
// characteristic, refer to SDK https://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.s132.api.v2.0.0%2Fgroup__ble__types.html
// <nrf-sdk>/componments/ble/common/ble_srv_common.h
#define BLE_UUID_BATTERY_LEVEL_CHAR                              0x2A19     /**< Battery Level characteristic UUID. */
#define BLE_UUID_REPORT_REF_DESCR                                0x2908     /**< Report Reference descriptor UUID. */
#define BLE_UUID_HID_INFORMATION_CHAR                            0x2A4A     /**< Hid Information characteristic UUID. */
#define BLE_UUID_REPORT_MAP_CHAR                                 0x2A4B     /**< Report Map characteristic UUID. */
#define BLE_UUID_HID_CONTROL_POINT_CHAR                          0x2A4C     /**< Hid Control Point characteristic UUID. */
#define BLE_UUID_REPORT_CHAR                                     0x2A4D     /**< Report characteristic UUID. */
#define BLE_UUID_PROTOCOL_MODE_CHAR                              0x2A4E     /**< Protocol Mode characteristic UUID. */

#define BLE_UUID_CCCD                        0x2902
#define BLE_CCCD_NOTIFY                      0x01

// refer to SDK https://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.s132.api.v2.0.0%2Fgroup___b_l_e___h_c_i___s_t_a_t_u_s___c_o_d_e_s.html
#define BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION 0x13
#define BLE_HCI_LOCAL_HOST_TERMINATED_CONNECTION 0x16


/** Global variables */
static bool        m_dongle_initialized = false;
static ble_gap_addr_t  m_connected_addr = { 0 }; /* intent or connected peripheral address */
static bool        m_is_connected = false; /* peripheral address has been connected(BLE_GAP_EVT_DISCONNECTED) */
static char        m_passkey[6] = { '1', '2', '3', '4', '5', '6' }; /* default fixed passkey for auth request(BLE_GAP_EVT_AUTH_KEY_REQUEST) */
static bool	       m_is_authenticated = false; /* peripheral address has been authenticated(BLE_GAP_EVT_AUTH_STATUS) */
static uint8_t     m_connected_devices = 0; /* number of connected devices */
static uint16_t    m_connection_handle = 0;
static uint16_t    m_service_start_handle = 0;
static uint16_t    m_service_end_handle = 0;
static uint16_t    m_discovered_handle = 0;
static uint16_t    m_device_name_handle = 0;
static uint16_t    m_battery_level_handle = 0;
static bool        m_connection_is_in_progress = false;
static adapter_t * m_adapter = NULL;

/* Callback functions from caller */
static std::map<fn_callback_id_t, std::vector<void*>> m_callback_fn_list;

/* Advertising data */
typedef struct _adv_data_t {
	ble_gap_evt_adv_report_t adv_report;
	std::map<uint8_t, data_t> type_data_list; /*BLE_GAP_AD_TYPE_DEFINITIONS, data*/
} adv_data_t;

/* Advertising data key pair by address */
static std::map<uint64_t, adv_data_t> m_adv_list; /*addr, report*/

/* Discovered characteristic data structure */
typedef struct _dev_char_t {
	uint16_t handle = 0; /* ble_gattc_char_t::handle_value */
	uint16_t uuid = 0; /* ble_gattc_char_t::uuid */
	uint16_t handle_decl = 0; /* ble_gattc_char_t::handle_decl */
	ble_gattc_handle_range_t handle_range = { 0, 0 }; /* handle range of descs */
	uint16_t report_ref_handle = 0; /* none zero which has BLE_UUID_REPORT_REF_DESCR desc */
	uint8_t report_ref[32] = { 0 }; /* reference value of BLE_UUID_REPORT_REF_DESCR desc */
	bool report_ref_is_read = false; /* report reference is read */
	uint16_t cccd_handle = 0; /* none zero which has BLE_UUID_CCCD desc */
	bool cccd_enabled = false; /* enable state of BLE_UUID_CCCD desc */
	std::vector<ble_gattc_desc_t> desc_list;
} dev_char_t;

/* Discovered handles */
static std::vector<dev_char_t> m_char_list;
static uint32_t m_char_idx = 0; // discover procedure index

/* Data buffer for hvx */
static std::map <uint16_t, data_t> m_read_data; /* handle, p_data, data_len */
/* Data buffer to write */
static std::map<uint16_t, data_t> m_write_data; /* handle, p_data, data_len */

static char m_log_msg[4096] = { 0 };
#ifdef _DEBUG
static log_level_t m_log_level = LOG_DEBUG;
//static log_level_t m_log_level = LOG_TRACE;
#else
static log_level_t m_log_level = LOG_INFO;
#endif

/* Mutex and condition variable for data read/write */
static std::mutex m_mtx_read_write;
static std::condition_variable m_cond_read_write;
static std::unique_lock<std::mutex> m_lck{ m_mtx_read_write };

/* Mutex and condition variable for helper function device_find */
static std::mutex m_mtx_find;
static std::condition_variable m_cond_find;

#if NRF_SD_BLE_API >= 5
static uint32_t    m_config_id = 1;
#endif

#if NRF_SD_BLE_API >= 6
static uint8_t     mp_data[100] = { 0 };
static ble_data_t  m_adv_report_buffer;
#endif

static ble_gap_scan_params_t m_scan_param =
{
#if NRF_SD_BLE_API >= 6
	0,                       // Set if accept extended advertising packetets.
	0,                       // Set if report inomplete reports.
#endif
	0,                       // Set if active scanning.
#if NRF_SD_BLE_API < 6
	0,                       // Set if selective scanning.
#endif
#if NRF_SD_BLE_API >= 6
	BLE_GAP_SCAN_FP_ACCEPT_ALL,
	BLE_GAP_PHY_1MBPS,
#endif
#if NRF_SD_BLE_API == 2
	NULL,                    // Set white-list.
#endif
#if NRF_SD_BLE_API == 3 || NRF_SD_BLE_API == 5
	0,                       // Set adv_dir_report.
#endif
	(uint16_t)SCAN_INTERVAL,
	(uint16_t)SCAN_WINDOW,
	(uint16_t)SCAN_TIMEOUT
#if NRF_SD_BLE_API >= 6
	, { 0 }                  // Set chennel mask.
#endif
};

static ble_gap_conn_params_t m_connection_param =
{
	(uint16_t)MIN_CONNECTION_INTERVAL,
	(uint16_t)MAX_CONNECTION_INTERVAL,
	(uint16_t)SLAVE_LATENCY,
	(uint16_t)CONNECTION_SUPERVISION_TIMEOUT
};

static ble_gap_sec_params_t m_sec_params =
{
	(uint8_t)1, /*bond*/
	(uint8_t)0, /*mitm*/
	(uint8_t)1, /*lesc*/
	(uint8_t)0, /*keypress*/
	(uint8_t)BLE_GAP_IO_CAPS_KEYBOARD_ONLY, /*io_caps*/
	(uint8_t)0, /*oob*/
	(uint8_t)7, /*min_key_size, header default from driver test_util_adapater_wrapper*/
	(uint8_t)16, /*max_key_size, header default from driver test_util_adapater_wrapper*/
	{ /*kdist_own, default from pc-nrfconnect-ble/lib/reducer/securityReducer.js*/
		(uint8_t)1, /** < enc, Long Term Key and Master Identification. */
		(uint8_t)1, /** < id, Identity Resolving Key and Identity Address Information. */
		(uint8_t)0, /** < sign, Connection Signature Resolving Key. */
		(uint8_t)0 /** < link, Derive the Link Key from the LTK. */
	},
	{ /*kdist_peer*/
		1, 1, 0, 0
	}
};

// key pair for security params update, refer to security.h/cpp, duplicated from pc-ble-driver-js\src\driver_uecc.cpp
static uint8_t m_private_key[ECC_P256_SK_LEN] = { 0 };
static uint8_t m_public_key[ECC_P256_PK_LEN] = { 0 };

// keyset data for LE security authentication
static ble_gap_enc_key_t m_own_enc = { 0 };
static ble_gap_id_key_t m_own_id = { 0 };
static ble_gap_sign_info_t m_own_sign = { 0 };
static ble_gap_lesc_p256_pk_t m_own_pk = { 0 };
static ble_gap_enc_key_t m_peer_enc = { 0 };
static ble_gap_id_key_t m_peer_id = { 0 };
static ble_gap_sign_info_t m_peer_sign = { 0 };
static ble_gap_lesc_p256_pk_t m_peer_pk = { 0 };


/** Global functions */

/* helper function export message to a log file with timestamp */
static void log_file(char *level, const char * message)
{
	auto now = std::chrono::system_clock::now();
	auto fraction = now - std::chrono::time_point_cast<std::chrono::seconds>(now);
	auto chronoms = std::chrono::duration_cast<std::chrono::milliseconds>(fraction);
	time_t tt = std::chrono::system_clock::to_time_t(now);
	tm local = { 0 };
	localtime_s(&local, &tt);
	char path[16] = { 0 };
	char time[16] = { 0 };
	strftime(path, sizeof(path), "log-%m-%d.log", &local);
	strftime(time, sizeof(time), "%H:%M:%S", &local);
	char ms[8] = { 0 };
	sprintf_s(ms, ".%03d", (int)chronoms.count());
	strcat_s(time, ms);

	FILE *f;
	errno_t err = fopen_s(&f, path, "a+");
	if (err != 0 || f == 0)
		return;

	fprintf(f, "%s [%s] %s\n", time, level, message);

	err = fclose(f);
}

static void log_level(log_level_t level, const char *message, ...) {
	if (level < m_log_level)
		return;

	char label[8] = { 0 };
	switch (level)
	{
	case SD_RPC_LOG_FATAL: /*LOG_FATAL*/
	case SD_RPC_LOG_ERROR: /*LOG_ERROR*/
		sprintf_s(label, "ERROR");
		break;

	case SD_RPC_LOG_WARNING: /*LOG_WARNING*/
		sprintf_s(label, "WARN ");
		break;

	case SD_RPC_LOG_INFO: /*LOG_INFO*/
		sprintf_s(label, "INFO ");
		break;

	case SD_RPC_LOG_DEBUG: /*LOG_DEBUG*/
		sprintf_s(label, "DEBUG");
		break;

	case SD_RPC_LOG_TRACE: /*LOG_TRACE*/
		sprintf_s(label, "TRACE");
		break;

	default:
		sprintf_s(label, "     ");
		break;
	}

	va_list _args;
	__crt_va_start(_args, message);
	vsprintf_s(m_log_msg, message, _args);
	__crt_va_end(_args);


	printf("%s %s\n", label, m_log_msg);
	fflush(stdout);

	log_file(label, m_log_msg);
}

/**@brief Function for handling the log message events from sd_rpc.
 *
 * @param[in] adapter The transport adapter.
 * @param[in] severity Level of severity that the log message is associated with.
 * @param[in] message The log message that the callback is associated with.
 */
static void log_handler(adapter_t * adapter, sd_rpc_log_severity_t severity, const char * message)
{
	// since additionally use the log_handler callback as generic function, check adapter pointer required
	if (adapter == NULL)
		return;
	// log_level_t is aligned to sd_rpc_log_severity_t
	log_level((log_level_t)severity, message);
}

/**@brief Function for handling error message events from sd_rpc.
 *
 * @param[in] adapter The transport adapter.
 * @param[in] code Error code that the error message is associated with.
 * @param[in] message The error message that the callback is associated with.
 */
static void status_handler(adapter_t * adapter, sd_rpc_app_status_t code, const char * message)
{
	log_level(LOG_INFO, "Adapter status: %d, message: %s", (uint32_t)code, message);
}


#pragma region /** Helper functions */

/**@brief Function for converting a BLE address to a string.
 *
 * @param[in] address       Bluetooth Low Energy address.
 * @param[out] string_buffer The serial port the target nRF5 device is connected to.
 */
static void ble_address_to_string_convert(ble_gap_addr_t address, uint8_t * string_buffer)
{
	const int address_length = BLE_GAP_ADDR_LEN;
	char      temp_str[3] = { 0 };
	size_t len = 0;
	for (int i = address_length - 1; i >= 0; --i)
	{
		sprintf_s(temp_str, sizeof(temp_str), "%02X", address.addr[i]);
		len += sizeof(temp_str);
		strcat_s((char *)string_buffer, len, temp_str);
	}
}

static void ble_address_to_uint64_convert(uint8_t addr[BLE_GAP_ADDR_LEN], uint64_t *number)
{
	for (int i = BLE_GAP_ADDR_LEN - 1; i >= 0; --i)
	{
		*number += (uint64_t)addr[i] << (i * 8);
	}
}

static uint32_t convert_byte_string(char* byte_array, uint32_t len, char* str);

/* func duplicated from adv_report_parse splitted advertising data by each types */
static uint32_t adv_report_data_slice(const ble_gap_evt_adv_report_t* p_adv_report, std::map<uint8_t, data_t>* pp_type_data)
{
	ble_data_t   adv_data;

	// Initialize advertisement report for parsing
#if NRF_SD_BLE_API >= 6
	adv_data.p_data = (uint8_t*)p_adv_report->data.p_data;
	adv_data.len = p_adv_report->data.len;
#else
	adv_data.p_data = (uint8_t*)p_adv_report->data;
	adv_data.len = p_adv_report->dlen;
#endif

	char log_data[256] = { 0 };
	convert_byte_string((char*)adv_data.p_data, adv_data.len, log_data);
	log_level(LOG_TRACE, "Raw adv: %s", log_data);

	uint32_t  index = 0;
	uint8_t* p_data = adv_data.p_data;
	uint16_t len = adv_data.len;
	
	// advertising data format:
	// https://docs.silabs.com/bluetooth/4.0/general/adv-and-scanning/bluetooth-adv-data-basics
	// assigned numbers, for AD type
	// https://btprodspecificationrefs.blob.core.windows.net/assigned-numbers/Assigned%20Number%20Types/Generic%20Access%20Profile.pdf

	while (index < len)
	{
		uint8_t field_len = p_data[index];
		uint8_t field_type = p_data[index + 1];

		auto field_data = (*pp_type_data)[field_type];
		field_data.len = field_len - 1;
		memset(field_data.p_data, 0, DATA_BUFFER_SIZE);
		memcpy(field_data.p_data, &p_data[index + 2], field_data.len);

		pp_type_data->insert_or_assign(field_type, field_data);		
		log_level(LOG_TRACE, " type:%x datalen:%d", field_type, field_len);

		index += field_len + 1;
	}
	return NRF_SUCCESS;
}

/* will find name in m_adv_list[].type_data_list */
static bool get_adv_name(std::map<uint8_t, data_t>* pp_type_data, char* name)
{
	auto found = pp_type_data->find(BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME);
	if (found != pp_type_data->end()) {
		memcpy(name, found->second.p_data, found->second.len);
		return true;
	}

	found = pp_type_data->find(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME);
	if (found != pp_type_data->end()) {
		memcpy(name, found->second.p_data, found->second.len);
		return true;
	}

	return false;
}

/**
 * @brief Parses advertisement data, providing length and location of the field in case
 *        matching data is found.
 *
 * @param[in]  Type of data to be looked for in advertisement data.
 * @param[in]  Advertisement report length and pointer to report.
 * @param[out] If data type requested is found in the data report, type data length and
 *             pointer to data will be populated here.
 *
 * @retval NRF_SUCCESS if the data type is found in the report.
 * @retval NRF_ERROR_NOT_FOUND if the data type could not be found.
 */
static uint32_t adv_report_parse(uint8_t type, ble_data_t * p_advdata, ble_data_t* p_typedata)
{
	uint32_t  index = 0;
	uint8_t * p_data;

	p_data = p_advdata->p_data;

	while (index < p_advdata->len)
	{
		uint8_t field_length = p_data[index];
		uint8_t field_type = p_data[index + 1];

		if (field_type == type)
		{
			p_typedata->p_data = &p_data[index + 2];
			p_typedata->len = field_length - 1;
			return NRF_SUCCESS;
		}
		index += field_length + 1;
	}
	return NRF_ERROR_NOT_FOUND;
}

// NOTICE: func has replaced by store adv data in type_data_list
static bool get_adv_name(const ble_gap_evt_adv_report_t *p_adv_report, char * name)
{
	uint32_t err_code;
	ble_data_t   adv_data;
	ble_data_t   dev_name;

	// Initialize advertisement report for parsing
#if NRF_SD_BLE_API >= 6
	adv_data.p_data = (uint8_t *)p_adv_report->data.p_data;
	adv_data.len = p_adv_report->data.len;
#else
	adv_data.p_data = (uint8_t *)p_adv_report->data;
	adv_data.len = p_adv_report->dlen;
#endif

	//search for advertising names
	err_code = adv_report_parse(BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
		&adv_data,
		&dev_name);
	if (err_code == NRF_SUCCESS)
	{
		memcpy(name, dev_name.p_data, dev_name.len);
		return true;
	}

	// Look for the short local name if it was not found as complete
	err_code = adv_report_parse(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME,
		&adv_data,
		&dev_name);
	if (err_code == NRF_SUCCESS)
	{
		memcpy(name, dev_name.p_data, dev_name.len);
		return true;
	}

	//NOTICE: only get readable ascii, otherwise refer to adv_report_data_slice()
	//// Look for the manufacturing data if it was not found as complete
	//err_code = adv_report_parse(BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA,
	//	&adv_data,
	//	&dev_name);
	//if (err_code == NRF_SUCCESS)
	//{
	//	memcpy(name, dev_name.p_data, dev_name.data_len);
	//	return true;
	//}

	return false;
}

static uint32_t convert_byte_string(char *byte_array, uint32_t len, char *str) {
	if (byte_array == NULL || str == NULL)
		return 0;

	uint32_t total_len = 0;
	for (int i = 0; i < len; i++) {
		auto print_len = sprintf_s(str, 4, "%02x ", (unsigned char)byte_array[i]);
		str += print_len;
		total_len += print_len;
	}
	return total_len;
}

static void print_byte_string(char *byte_array, uint32_t len) {
	if (byte_array == NULL)
		return;
	for (int i = 0; i < len; i++) {
		printf("%02x ", (unsigned char)byte_array[i]);
	}
	fflush(stdout);
}

static bool get_uuid_string(uint16_t uuid, char *uuid_string) {
	if (uuid_string == NULL) {
		return false;
	}
	bool result = false;
	switch (uuid) {
		// service uuids
	case BLE_UUID_GAP:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "GAP");
		result = true;
		break;
	case BLE_UUID_GATT:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "GATT");
		result = true;
		break;
	case BLE_UUID_BATTERY_SRV:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "BATTERY");
		result = true;
		break;
	case BLE_UUID_HID_SRV:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "HID");
		result = true;
		break;
	case BLE_UUID_SERVICE_PRIMARY:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "ServicePrimary");
		result = true;
		break;
	case BLE_UUID_CHARACTERISTIC:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "Characteristic");
		result = true;
		break;
		// characteristic uuids
	case BLE_UUID_GAP_CHARACTERISTIC_DEVICE_NAME:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "DeviceName");
		result = true;
		break;
	case BLE_UUID_GAP_CHARACTERISTIC_APPEARANCE:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "Appearance");
		result = true;
		break;
	case BLE_UUID_GAP_CHARACTERISTIC_PPCP:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "PreferConnParam");
		result = true;
		break;
	case BLE_UUID_BATTERY_LEVEL_CHAR:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "BatteryLevel");
		result = true;
		break;
	case BLE_UUID_PROTOCOL_MODE_CHAR:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "ProtocolMode");
		result = true;
		break;
	case BLE_UUID_REPORT_CHAR:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "Report");
		result = true;
		break;
		// descriptor
	case BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "ClientCharConfig");
		result = true;
		break;
	case BLE_UUID_REPORT_REF_DESCR:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "ReportRef");
		result = true;
		break;
	case BLE_UUID_HID_INFORMATION_CHAR:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "HidInfo");
		result = true;
		break;
	case BLE_UUID_REPORT_MAP_CHAR:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "ReportMap");
		result = true;
		break;
	case BLE_UUID_HID_CONTROL_POINT_CHAR:
		strcpy_s(uuid_string, STRING_BUFFER_SIZE, "HidContrlPoint");
		result = true;
		break;
	default:
		sprintf_s(uuid_string, STRING_BUFFER_SIZE, "0x%04X", uuid);
		result = false;
		break;
	}
	return result;
}

/**
cleanup stored data for connected device
*/
static void connection_cleanup() {
	m_connected_addr = { 0 };
	m_is_connected = false;
	m_is_authenticated = false;
	m_device_name_handle = 0;
	m_battery_level_handle = 0;
	m_char_list.clear();
	m_char_idx = 0;
	m_cond_read_write.notify_all();
	m_cond_find.notify_all();
}

#pragma endregion


#pragma region /** Start actions */

/**@brief Start scanning (GAP Discovery procedure, Observer Procedure).
 * *
 * @return NRF_SUCCESS on successfully initiating scanning procedure, otherwise an error code.
 */
uint32_t scan_start(float interval, float window, bool active, uint16_t timeout)
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	//m_discovered_report = { 0 };
	m_adv_list.clear();

#if NRF_SD_BLE_API >= 6
	m_adv_report_buffer.p_data = mp_data;
	m_adv_report_buffer.len = sizeof(mp_data);
#endif

	// scan-param-v5: 0,0,0,SCAN_INTERVAL,SCAN_WINDOW,SCAN_TIMEOUT
	//                active,selective(whitelist),adv_dir_report
	m_scan_param.interval = MSEC_TO_UNITS(interval, UNIT_0_625_MS); // 0x00A0=100ms, 0x0140=200ms
	m_scan_param.window = MSEC_TO_UNITS(window, UNIT_0_625_MS); // 0x0050=50ms
	m_scan_param.active = active ? 1 : 0;
	m_scan_param.timeout = timeout;

	uint32_t error_code = sd_ble_gap_scan_start(m_adapter, &m_scan_param
#if NRF_SD_BLE_API >= 6
		, &m_adv_report_buffer
#endif
	);

	if (error_code != NRF_SUCCESS) {
		log_level(LOG_ERROR, "Scan start failed with error code: %d", error_code);
	}
	else {
		log_level(LOG_INFO, "Scan started");
	}

	return error_code;
}

uint32_t scan_stop()
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	uint32_t error_code = 0;
	error_code = sd_ble_gap_scan_stop(m_adapter);

	if (error_code != NRF_SUCCESS) {
		log_level(LOG_ERROR, "Scan stop failed, code: %d", error_code);
	}
	else {
		log_level(LOG_INFO, "Scan stop");
	}

	return error_code;
}

uint32_t conn_start(uint8_t addr_type, uint8_t addr[6])
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	// cleanup previous data
	connection_cleanup();

	m_connected_addr.addr_id_peer = 1;
	m_connected_addr.addr_type = addr_type;
	memcpy_s(&(m_connected_addr.addr[0]), BLE_GAP_ADDR_LEN, &addr[0], BLE_GAP_ADDR_LEN);

	m_connection_param.min_conn_interval = MIN_CONNECTION_INTERVAL;
	m_connection_param.max_conn_interval = MAX_CONNECTION_INTERVAL;
	m_connection_param.slave_latency = 0;
	m_connection_param.conn_sup_timeout = CONNECTION_SUPERVISION_TIMEOUT;
	log_level(LOG_DEBUG, "conn start, conn params min=%d max=%d late=%d timeout=%d",
		(int)(m_connection_param.min_conn_interval * 1.25),
		(int)(m_connection_param.max_conn_interval * 1.25),
		m_connection_param.slave_latency,
		(int)(m_connection_param.conn_sup_timeout * 10));
	
	//TODO: debug thu diff cfg
	/*sd_ble_gap_connect(m_adapter, &(m_connected_addr), &m_scan_param, &m_connection_param, BLE_CONN_CFG_TAG_DEFAULT);
	ble_gap_adv_params_t adv_param = { 0 };
	sd_ble_gap_adv_start(m_adapter, NULL, m_config_id);*/

	uint32_t err_code;
	err_code = sd_ble_gap_connect(m_adapter,
		&(m_connected_addr),
		&m_scan_param,
		&m_connection_param
#if NRF_SD_BLE_API >= 5
		, m_config_id
#endif
	);
	if (err_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "Connection Request Failed, reason %d", err_code);
		return err_code;
	}

	m_connection_is_in_progress = true;

	return err_code;
}

uint32_t auth_set_params(bool lesc, bool oob, bool mitm, uint8_t role, bool enc, bool id, bool sign, bool link)
{
	m_sec_params.lesc = lesc ? 1 : 0; /* enable LE secure conn */
	m_sec_params.oob = oob ? 1 : 0; /* set if has out of band auth data */
	m_sec_params.mitm = mitm ? 1 : 0;
	/* OOB enabled will use OOB method if:
	- both of devices have out of band(legacy), or
	- at least one of device has peer OOB data(lesc enabled) */
	//https://infocenter.nordicsemi.com/topic/com.nordic.infocenter.s132.api.v5.0.0/s132_msc_overview.html?cp=4_7_3_7_1
	// GAP MSC -> Central Security Procedures
	//TODO: debug gen ltk
	/*ble_gap_master_id_t master_id = { 0 };
	ble_gap_enc_info_t info = { 0 };
	sd_ble_gap_encrypt(m_adapter, m_connection_handle, NULL, NULL);*/
	//TODO: debug2 req peer enc
	if (role == 0) {
		m_sec_params.kdist_own.enc = enc ? 1 : 0;
		m_sec_params.kdist_own.id = id ? 1 : 0;
		m_sec_params.kdist_own.sign = sign ? 1 : 0; // 1:NRF_ERROR_NOT_SUPPORTED
		m_sec_params.kdist_own.link = link ? 1 : 0; // 1:NRF_ERROR_NOT_SUPPORTED
	}
	if (role == 1) {
		m_sec_params.kdist_peer.enc = enc ? 1 : 0;
		m_sec_params.kdist_peer.id = id ? 1 : 0;
		m_sec_params.kdist_peer.sign = sign ? 1 : 0; // 1:NRF_ERROR_NOT_SUPPORTED
		m_sec_params.kdist_peer.link = link ? 1 : 0; // 1:NRF_ERROR_NOT_SUPPORTED
	}
	return NRF_SUCCESS;
}

uint32_t auth_start(bool bond, bool keypress, uint8_t io_caps, const char* passkey)
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	// update fixed passkey
	if (passkey) {
		memcpy_s(m_passkey, 6, passkey, 6);
	}

	uint32_t error_code;

	// try to get security mode before authenticate
	ble_gap_conn_sec_t conn_sec;
	error_code = sd_ble_gap_conn_sec_get(m_adapter, m_connection_handle, &conn_sec);
	log_level(LOG_DEBUG, "get security, return=%d mode=%d level=%d",
		error_code, conn_sec.sec_mode.sm, conn_sec.sec_mode.lv);

	m_sec_params.bond = bond ? 1 : 0;
	m_sec_params.keypress = keypress ? 1 : 0;
	m_sec_params.io_caps = io_caps;
	// NOTICE: refer to m_sec_params default value for other security options,
	//   or change by auth_config() before authentication

	// NOTICE: refer to driver, testcase_security.cpp, we'll use the default security params
	error_code = sd_ble_gap_authenticate(m_adapter, m_connection_handle, &m_sec_params);
	// NOTICE: for other devices, check if return NRF_ERROR_NOT_SUPPORTED or NRF_ERROR_NO_MEM?
	//         driver test case uses for passkey auth, refer to testcase_security.cpp
	log_level(LOG_DEBUG, "authenticate return=%d should be %d", error_code, NRF_SUCCESS);

	if (error_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "Authenticate start Failed, code: %d", error_code);
	}

	return error_code;
}

/**@brief Function called upon connecting to BLE peripheral.
 *
 * @details Initiates primary service discovery.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
uint32_t service_discovery_start(uint16_t uuid, uint8_t type)
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	uint32_t   err_code;
	uint16_t   start_handle = 0x01;
	ble_uuid_t srvc_uuid;

	char uuid_string[STRING_BUFFER_SIZE] = { 0 };
	get_uuid_string(uuid, uuid_string);
	log_level(LOG_INFO, "Discovering primary service:0x%04X(%s)", uuid, uuid_string);

	srvc_uuid.type = type;
	srvc_uuid.uuid = uuid;

	// Initiate procedure to find the primary BLE_UUID_HEART_RATE_SERVICE.
	err_code = sd_ble_gattc_primary_services_discover(m_adapter,
		m_connection_handle, start_handle,
		&srvc_uuid/*NULL*/);
	if (err_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "Failed to initiate or continue a GATT Primary Service Discovery procedure");
	}

	return err_code;
}

/**@brief Function called upon discovering a BLE peripheral's primary service(s).
 *
 * @details Initiates service's (m_service) characteristic discovery.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t char_discovery_start(ble_gattc_handle_range_t handle_range)
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	if (handle_range.start_handle == 0 && handle_range.end_handle == 0) {
		handle_range.start_handle = m_service_start_handle;
		handle_range.end_handle = m_service_end_handle;
	}

	log_level(LOG_INFO, "Discovering characteristics, handle range:0x%04X - 0x%04X",
		handle_range.start_handle, handle_range.end_handle);

	return sd_ble_gattc_characteristics_discover(m_adapter, m_connection_handle, &handle_range);
}

/**@brief Function called upon discovering service's characteristics.
 *
 * @details Initiates heart rate monitor (m_hrm_char_handle) characteristic's descriptor discovery.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t descr_discovery_start(ble_gattc_handle_range_t handle_range)
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	if (handle_range.start_handle == 0 && handle_range.end_handle == 0) {
		handle_range.start_handle = m_service_start_handle;
		handle_range.end_handle = m_service_end_handle;
	}

	log_level(LOG_INFO, "Discovering descriptors, handle range:0x%04X - 0x%04X",
		handle_range.start_handle, handle_range.end_handle);

	return sd_ble_gattc_descriptors_discover(m_adapter, m_connection_handle, &handle_range);
}

/*
read device name from GAP which handle_value in m_char_list(handle in desc_list)
*/
static uint32_t read_device_name()
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	uint32_t error_code = 0;
	// use m_device_name_handle or find BLE_UUID_GAP_CHARACTERISTIC_DEVICE_NAME in desc_list
	uint16_t value_handle = m_device_name_handle;
	error_code = sd_ble_gattc_read(
		m_adapter,
		m_connection_handle,
		value_handle, 0
	);
	log_level(LOG_DEBUG, " Read value from handle:0x%04X code:%d", value_handle, error_code);
	return error_code;
}

/*
set cccd notification to handles in m_char_list(or handle in its desc_list),
writting behavior works with on_write_response() and m_service_start_handle
*/
static uint32_t set_cccd_notification(uint16_t handle)
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	ble_gattc_write_params_t write_params;
	uint8_t                  cccd_value[2] = { 1/*enable or disable*/, 0 };

	// a flag indicates enabling nexts
	bool enable_next = false;
	uint32_t error_code = 0;
	for (int i = 0; i < m_char_list.size(); i++) {
		// ignore uuid not cccd
		if (m_char_list[i].cccd_handle == 0)
			continue;
		// ignore if registered
		if (m_char_list[i].cccd_enabled)
			continue;
		// use first handle if given handle is null
		if (m_char_list[i].cccd_handle >= handle) {
			m_char_idx = i;
			write_params.handle = m_char_list[m_char_idx].cccd_handle;
			write_params.len = 2;
			write_params.p_value = cccd_value;
			write_params.write_op = BLE_GATT_OP_WRITE_REQ;
			write_params.offset = 0;
			// write it!
			error_code = sd_ble_gattc_write(m_adapter, m_connection_handle, &write_params);
			log_level(LOG_INFO, " Write to register CCCD handle:0x%04X code:%d",
				m_char_list[m_char_idx].cccd_handle, error_code);
			enable_next = true;
			break;
		}
	}

	if (enable_next == false) {
		log_level(LOG_DEBUG, "List of characteristics with report reference data.");
		uint16_t count = 0;
		for (int i = 0; i < m_char_list.size(); i++) {
			if (m_char_list[i].report_ref_is_read) {
				log_level(LOG_DEBUG, " char:%04X desc:%04X reference data:%02x %02x",
					m_char_list[i].handle, m_char_list[i].report_ref_handle, 
					m_char_list[i].report_ref[0], m_char_list[i].report_ref[1]);
			}

			if (m_char_list[i].report_ref_is_read || m_char_list[i].cccd_enabled) {
				count++;
			}
		}

		m_cond_find.notify_all();

		for (auto &fn : m_callback_fn_list[FN_ON_SERVICE_ENABLED]) {
			// return number of characteristics
			((fn_on_service_enabled)fn)(count);
		}
	}

	return error_code;
}

/*
read hid report reference data from m_char_list(or handle in its desc_list),
reading behavior works with on_read_response()
reference data definition: report_id, report_type are defined by FW
  refer to https://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.sdk5.v15.3.0%2Fstructble__srv__report__ref__t.html
*/
static uint32_t read_report_refs(uint16_t handle)
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	// a flag indicates reading next
	bool read_next = false;
	uint32_t error_code = 0;
	for (int i = 0; i < m_char_list.size(); i++) {
		// ignore uuid not report reference
		if (m_char_list[i].report_ref_handle == 0)
			continue;
		// ignore handle if already read
		if (m_char_list[i].report_ref_is_read)
			continue;
		if (m_char_list[i].report_ref_handle >= handle) {
			m_char_idx = i;
			// read it!, then check on_read_response()
			error_code = sd_ble_gattc_read(
				m_adapter,
				m_connection_handle,
				m_char_list[m_char_idx].report_ref_handle, 0
			);
			log_level(LOG_INFO, " Read value from handle:0x%04X code:%d",
				m_char_list[m_char_idx].report_ref_handle, error_code);
			read_next = true;
			// can only call next handle from read response
			break;
		}
	}

	// if there is no reference to read, set CCCD notification
	if (read_next == false) {
		set_cccd_notification(0);
	}

	return error_code;
}

uint32_t service_enable_start() {

	read_report_refs(0);
	// read_report_refs will also set_cccd_notification

	return 0;
}

uint32_t device_find(uint8_t addr[6], int8_t rssi, const char* passkey, uint16_t timeout) {
	uint32_t error_code = 0;

	error_code = dongle_disconnect();
	if (error_code == NRF_SUCCESS) {
		std::unique_lock<std::mutex> lck{ m_mtx_find };
		auto stat = m_cond_find.wait_for(lck, std::chrono::milliseconds(timeout));
		if (stat == std::cv_status::timeout) {
			return NRF_ERROR_TIMEOUT;
		}
	}

	error_code = scan_stop();

	error_code = scan_start(200, 50, true, 0);
	if (error_code != NRF_SUCCESS) {
		return error_code;
	}

	auto target = m_adv_list.end();
	int32_t elapsed = 0;
	while (elapsed < timeout) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		elapsed += 100;
		target = m_adv_list.begin();
		for (; target != m_adv_list.end(); target++) {
			if (target->second.adv_report.rssi < rssi)
				continue;
			if (addr != NULL) {
				uint64_t addr_num = 0;
				ble_address_to_uint64_convert(addr, &addr_num);
				if (target->first == addr_num) {
					break;
				}
			}
			else {
				// ignore address check
				break;
			}
		}
		if (target != m_adv_list.end()) {
			break;
		}
	}

	error_code = scan_stop();

	if (elapsed >= timeout || target == m_adv_list.end()) {
		return NRF_ERROR_TIMEOUT;
	}

	error_code = conn_start(target->second.adv_report.peer_addr.addr_type, target->second.adv_report.peer_addr.addr);
	if (error_code != NRF_SUCCESS) {
		return error_code;
	}
	// until on_connected
	std::unique_lock<std::mutex> lck{ m_mtx_find };
	auto stat = m_cond_find.wait_for(lck, std::chrono::milliseconds(timeout));
	if (stat == std::cv_status::timeout) {
		return NRF_ERROR_TIMEOUT;
	}

	error_code = auth_start(true, false, 0x2, passkey);
	if (error_code != NRF_SUCCESS) {
		return error_code;
	}
	// until BLE_GAP_EVT_AUTH_STATUS
	stat = m_cond_find.wait_for(lck, std::chrono::milliseconds(timeout));
	if (stat == std::cv_status::timeout) {
		return NRF_ERROR_TIMEOUT;
	}
	
	typedef struct {
		unsigned short uuid;
		unsigned short type;
	} service;

	int service_idx = 0;
	std::vector<service> service_list = {
		{ 0x1800, 0x01 },
		{ 0x180F, 0x01 },
		{ 0x1812, 0x01 }
	};

	for (int i = 0; i < 3; i++) {
		error_code = service_discovery_start(service_list[i].uuid, service_list[i].type);
		if (error_code != NRF_SUCCESS) {
			return error_code;
		}
		// until any of before invoking FN_ON_SERVICE_DISCOVERED callback
		stat = m_cond_find.wait_for(lck, std::chrono::milliseconds(timeout));
		if (stat == std::cv_status::timeout) {
			return NRF_ERROR_TIMEOUT;
		}
	}

	error_code = service_enable_start();
	if (error_code != NRF_SUCCESS) {
		return error_code;
	}
	// until FN_ON_SERVICE_ENABLED
	stat = m_cond_find.wait_for(lck, std::chrono::milliseconds(timeout));
	if (stat == std::cv_status::timeout) {
		return NRF_ERROR_TIMEOUT;
	}

	return NRF_SUCCESS;
}

uint32_t device_find_str(const char* addr_str, int8_t rssi, const char* passkey, uint16_t timeout) {
	uint8_t* addr = NULL;
	uint8_t addr_lsb[6] = { 0 };
	char hex_str[2] = { 0 };
	if (strlen(addr_str) >= 12) {
		for (int i = 0; i < 6; i++) {
			memcpy_s(hex_str, 2, &addr_str[(5 - i) * 2], 2);
			addr_lsb[i] = strtoul(hex_str, NULL, 16);
		}
	}
	if (addr_lsb[0] == 0)
		return NRF_ERROR_INVALID_PARAM;
	uint32_t error_code = device_find(addr_lsb, rssi, passkey, timeout);
	return error_code;
}

uint32_t report_char_list(uint16_t *handle_list, uint8_t *refs_list, uint16_t *len) {
	if (handle_list == 0 || refs_list == 0 || len == 0) {
		return NRF_ERROR_INVALID_PARAM;
	}

	uint16_t count = 0;
	for (int i = 0; i < m_char_list.size() && count < *len; i++) {
		if (m_char_list[i].report_ref_is_read) {
			handle_list[count] = m_char_list[i].handle;
			memcpy_s(&(refs_list[count * 2]), 2, &(m_char_list[i].report_ref[0]), 2);
			count++;
		}
	}
	*len = std::min(*len, count);
	return NRF_SUCCESS;
}

EXTERNC NRFBLEAPI uint32_t data_read_async(uint16_t handle)
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	uint32_t error_code = 0;
	error_code = sd_ble_gattc_read(
		m_adapter,
		m_connection_handle,
		handle, 0
	);
	log_level(LOG_INFO, " Read value from handle:0x%04X code:%d", handle, error_code);

	return error_code;
}

uint32_t data_read(uint16_t handle, uint8_t *data, uint16_t *len, uint16_t timeout)
{
	if (data == NULL || len == NULL || *len == 0)
		return NRF_ERROR_INVALID_PARAM;
	
	uint32_t error_code = data_read_async(handle);
	if (error_code != NRF_SUCCESS) {
		return error_code;
	}

	//std::unique_lock<std::mutex> lck{ m_mtx_read_write };
	auto stat = m_cond_read_write.wait_for(m_lck, std::chrono::milliseconds(timeout));
	if (stat == std::cv_status::timeout) {
		return NRF_ERROR_TIMEOUT;
	}

	if (m_read_data[handle].p_data == NULL || m_read_data[handle].len == 0)
		return NRF_ERROR_INVALID_DATA;
	
	// limited data length by given len
	*len = std::min(*len, m_read_data[handle].len);
	memcpy_s(data, *len, m_read_data[handle].p_data, *len);
	
	return NRF_SUCCESS;
}

uint32_t data_read_by_report_ref(uint8_t *report_ref, uint8_t *data, uint16_t *len, uint16_t timeout)
{
	uint16_t handle = 0;
	for (auto it = m_char_list.begin(); it != m_char_list.end(); it++) {
		if (it->report_ref_handle == 0 || it->report_ref_is_read == false)
			continue;
		if (it->report_ref[0] == report_ref[0] && it->report_ref[1] == report_ref[1]) {
			handle = it->handle;
			break;
		}
	}
	if (handle == 0) {
		return NRF_ERROR_NOT_FOUND;
	}
	sprintf_s(m_log_msg, " Read value handle found: 0x%04X ref:", handle);
	convert_byte_string((char *)report_ref, 2, &(m_log_msg[strlen(m_log_msg)]));
	log_level(LOG_INFO, m_log_msg);
	return data_read(handle, data, len, timeout);
}

EXTERNC NRFBLEAPI uint32_t data_write_async(uint16_t handle, uint8_t* data, uint16_t len)
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	if (data == NULL || len == 0)
		return NRF_ERROR_INVALID_PARAM;

	memset(m_write_data[handle].p_data, 0, DATA_BUFFER_SIZE);
	memcpy_s(m_write_data[handle].p_data, DATA_BUFFER_SIZE, data, len);
	m_write_data[handle].len = len;

	ble_gattc_write_params_t write_params;
	write_params.handle = handle;
	write_params.len = m_write_data[handle].len;
	write_params.p_value = m_write_data[handle].p_data;
	write_params.write_op = BLE_GATT_OP_WRITE_REQ;
	write_params.offset = 0;
	uint32_t error_code = 0;
	error_code = sd_ble_gattc_write(m_adapter, m_connection_handle, &write_params);
	log_level(LOG_INFO, " Write value to handle:0x%04X data:0x%02x %02x code:%d",
		handle, m_write_data[handle].p_data[0], m_write_data[handle].p_data[1], error_code);
	return error_code;
}

uint32_t data_write(uint16_t handle, uint8_t *data, uint16_t len, uint16_t timeout)
{
	if (data == NULL || len == 0)
		return NRF_ERROR_INVALID_PARAM;

	uint32_t error_code = data_write_async(handle, data, len);
	if (error_code != NRF_SUCCESS) {
		return error_code;
	}

	//std::unique_lock<std::mutex> lck{ m_mtx_read_write };
	auto stat = m_cond_read_write.wait_for(m_lck, std::chrono::milliseconds(timeout));
	if (stat == std::cv_status::timeout) {
		return NRF_ERROR_TIMEOUT;
	}
	//DEBUG: check write data?
	return NRF_SUCCESS;
}

uint32_t data_write_by_report_ref(uint8_t *report_ref, uint8_t *data, uint16_t len, uint16_t timeout)
{
	uint16_t handle = 0;
	for (auto it = m_char_list.begin(); it != m_char_list.end(); it++) {
		if (it->report_ref_handle == 0 || it->report_ref_is_read == false)
			continue;
		if (it->report_ref[0] == report_ref[0] && it->report_ref[1] == report_ref[1]) {
			handle = it->handle;
			break;
		}
	}
	if (handle == 0) {
		return NRF_ERROR_NOT_FOUND;
	}
	sprintf_s(m_log_msg, " Write value handle found: 0x%04X ref:", handle);
	convert_byte_string((char *)report_ref, 2, &(m_log_msg[strlen(m_log_msg)]));
	log_level(LOG_INFO, m_log_msg);
	return data_write(handle, data, len, timeout);
}

uint32_t dongle_disconnect() 
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	uint32_t error_code = 0;
	error_code = sd_ble_gap_disconnect(m_adapter, m_connection_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
	log_level(LOG_INFO, "User disconnect, code:%d", error_code);
	connection_cleanup();
	return error_code;
}

uint32_t dongle_reset()
{
	if (m_adapter == NULL)
		return NRF_ERROR_INVALID_STATE;

	auto error_code = sd_rpc_conn_reset(m_adapter, SOFT_RESET);
	sprintf_s(m_log_msg, "RPC reset, code: 0x%02X", error_code);

	if (error_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, m_log_msg);
	}
	else {
		log_level(LOG_INFO, m_log_msg);
	}

	error_code = sd_rpc_close(m_adapter);
	sprintf_s(m_log_msg, "Close nRF BLE Driver. code: 0x%02X", error_code);

	if (error_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, m_log_msg);
	}
	else {
		log_level(LOG_INFO, m_log_msg);
	}

	m_dongle_initialized = false;
	return error_code;
}

#pragma endregion


#pragma region /** Event functions */

/**@brief Function called on BLE_GAP_EVT_ADV_REPORT event.
 *
 * @details Create a connection if received advertising packet corresponds to desired BLE device.
 *
 * @param[in] p_ble_gap_evt Advertising Report Event.
 */
static void on_adv_report(const ble_gap_evt_t * const p_ble_gap_evt)
{
	uint32_t err_code;
	uint8_t  str[STRING_BUFFER_SIZE] = { 0 };

	// only focus on rssi less than -60
	bool near = (p_ble_gap_evt->params.adv_report.rssi > -60);

	uint64_t addr_num = 0;
	if (near) {
		ble_address_to_uint64_convert((uint8_t*)p_ble_gap_evt->params.adv_report.peer_addr.addr, &addr_num);

		// adv list always up-to-date
		bool arrival = (m_adv_list.find(addr_num) == m_adv_list.end());
		if (arrival) {
			adv_data_t adv_data = {
				p_ble_gap_evt->params.adv_report
			};
			m_adv_list.insert_or_assign(addr_num, adv_data);
		}
		//m_adv_list.insert_or_assign(addr_num, p_ble_gap_evt->params.adv_report);

		adv_report_data_slice(&p_ble_gap_evt->params.adv_report, &m_adv_list[addr_num].type_data_list);
		log_level(LOG_TRACE, "Scan addr:%llx parsed advertising data list:%lu", addr_num, m_adv_list[addr_num].type_data_list.size());
	} // end of if(near)
	
	// TODO: caller update if any or rssi changed?
	bool update = true;
	//bool update = m_adv_list[addr_num].adv_report.rssi != p_ble_gap_evt->params.adv_report.rssi;
	if (near && update)
	{
		// Log the Bluetooth device address of advertisement packet received.
		ble_address_to_string_convert(p_ble_gap_evt->params.adv_report.peer_addr, str);

		char name[256] = { 0 };
		// Just get name from m_adv_list[].type_data_list[]
		get_adv_name(&m_adv_list[addr_num].type_data_list, name);
		//get_adv_name(&p_ble_gap_evt->params.adv_report, name);

#if NRF_SD_BLE_API >= 6
		uint8_t  str2[STRING_BUFFER_SIZE] = { 0 };
		ble_address_to_string_convert(p_ble_gap_evt->params.adv_report.direct_addr, str2);
		log_level(LOG_DEBUG, "Received adv report peer:0x%s direct:0x%s rssi:%d type:%d name:%s\r\n \
chidx:%d dataid:%d priphy:%d setid:%d txpwr:%d auxoff:%d auxphy:%d",
			str, str2,
			p_ble_gap_evt->params.adv_report.rssi,
			p_ble_gap_evt->params.adv_report.type,
			name,
			p_ble_gap_evt->params.adv_report.ch_index,
			p_ble_gap_evt->params.adv_report.data_id,
			p_ble_gap_evt->params.adv_report.primary_phy,
			p_ble_gap_evt->params.adv_report.set_id,
			p_ble_gap_evt->params.adv_report.tx_power,
			p_ble_gap_evt->params.adv_report.aux_pointer.aux_offset,
			p_ble_gap_evt->params.adv_report.aux_pointer.aux_phy);

#elif NRF_SD_BLE_API >= 5
		log_level(LOG_DEBUG, "Received adv report address: 0x%s rssi:%d type:%d rsp:%d list:%lu name:%s",
			str, p_ble_gap_evt->params.adv_report.rssi,
			p_ble_gap_evt->params.adv_report.type,
			p_ble_gap_evt->params.adv_report.scan_rsp,
			m_adv_list[addr_num].type_data_list.size(),
			name);

		if (p_ble_gap_evt->params.adv_report.scan_rsp == 0 /*||
			p_ble_gap_evt->params.adv_report.type != BLE_GAP_ADV_TYPE_ADV_IND*/) {
			return;
		}
#endif

		// flag to invoke caller callback for further connection establish
		// or waiting other advertising data (for SD API v6, re-start scan)
		if (m_connection_is_in_progress) {
			log_level(LOG_WARNING, "Connection has been started, ignore rest of discovered devices");
			return;
		}

		// invoke callback to caller with discovered report
		if (m_callback_fn_list[FN_ON_DISCOVERED].size() > 0) {
			std::string name_str = std::string(name);
			std::string addr_str = std::string((char*)str);
			addr_t report;
			report.rssi = p_ble_gap_evt->params.adv_report.rssi;
			report.addr_type = p_ble_gap_evt->params.adv_report.peer_addr.addr_type;
			memcpy_s(&(report.addr[0]), BLE_GAP_ADDR_LEN,
				&(p_ble_gap_evt->params.adv_report.peer_addr.addr[0]), BLE_GAP_ADDR_LEN);
			for (auto& fn : m_callback_fn_list[FN_ON_DISCOVERED]) {
				((fn_on_discovered)fn)(addr_str.c_str(), name_str.c_str(), report.addr_type, report.addr, report.rssi);
			}
		}
	} // end of if(update)

#if NRF_SD_BLE_API >= 6
	//TODO: may need manual stop flag prevent sending command to dongle at the same time
	err_code = sd_ble_gap_scan_start(m_adapter, NULL, &m_adv_report_buffer);

	if (err_code != NRF_SUCCESS)
	{
		sprintf_s(m_log_msg, "Scan re-start failed with error code: %d", err_code);
		log_level(LOG_ERROR, m_log_msg);
	}
	else
	{
		sprintf_s(m_log_msg, "Scan re-started");
		log_level(LOG_DEBUG, m_log_msg);
	}
#endif

}

/**@brief Function called on BLE_GAP_EVT_TIMEOUT event.
 *
 * @param[in] ble_gap_evt_t Timeout Event.
 */
static void on_timeout(const ble_gap_evt_t * const p_ble_gap_evt)
{
	if (p_ble_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN)
	{
		m_connection_is_in_progress = false;
	}
	else if (p_ble_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_SCAN)
	{
		//DEBUG: may not restart scan action
		//scan_start();
	}
	connection_cleanup();
}

/**@brief Function called on BLE_GAP_EVT_CONNECTED event.
 *
 * @details Update connection state and proceed to discovering the peer's GATT services.
 *
 * @param[in] p_ble_gap_evt GAP event.
 */
static void on_connected(const ble_gap_evt_t * const p_ble_gap_evt)
{
	log_level(LOG_INFO, "Connection established role=%d",
		p_ble_gap_evt->params.connected.role); /* BLE_GAP_ROLE_PERIPH 0x1, BLE_GAP_ROLE_CENTRAL 0x2 */
	
	m_connected_devices++;
	m_connection_handle = p_ble_gap_evt->conn_handle;
	m_connection_is_in_progress = false;

	bool match = true;
	for (int i = 0; i < BLE_GAP_ADDR_LEN; i++) {
		if (p_ble_gap_evt->params.connected.peer_addr.addr[i] != m_connected_addr.addr[i]) {
			match = false;
			break;
		}
	}
	m_is_connected = match;

	m_cond_find.notify_all();

	for (auto &fn : m_callback_fn_list[FN_ON_CONNECTED]) {
		((fn_on_connected)fn)(m_connected_addr.addr_type, m_connected_addr.addr);
	}

	// DEBUG: service discovery should wait before param updated event or bond for auth secure param(or passkey)
	//auth_start();
	// than
	//service_discovery_start();
}

/*
called on BLE_GAP_EVT_DISCONNECTED event
caller should check reason code, refer to BLE_HCI_STATUS_CODES
BLE_HCI_CONNECTION_TIMEOUT 0x8
BLE_HCI_LOCAL_HOST_TERMINATED_CONNECTION 0x16
*/
static void on_disconnected(const ble_gap_evt_t *const p_ble_gap_evt) {
	log_level(LOG_INFO, "Disconnected, reason: 0x%02X",
		p_ble_gap_evt->params.disconnected.reason);

	m_connected_devices--;
	m_connection_handle = 0;
	connection_cleanup();

	for (auto &fn : m_callback_fn_list[FN_ON_DISCONNECTED]) {
		((fn_on_disconnected)fn)(p_ble_gap_evt->params.disconnected.reason);
	}
}

/**@brief Function called on BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP event.
 *
 * @details Update service state and proceed to discovering the service's GATT characteristics.
 *
 * @param[in] p_ble_gattc_evt Primary Service Discovery Response Event.
 */
static void on_service_discovery_response(const ble_gattc_evt_t * const p_ble_gattc_evt)
{
	int count;
	int service_index;
	const ble_gattc_service_t * service;

	if (p_ble_gattc_evt->gatt_status != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "Service discovery failed. Error code 0x%X", p_ble_gattc_evt->gatt_status);
		return;
	}

	count = p_ble_gattc_evt->params.prim_srvc_disc_rsp.count;
	log_level(LOG_INFO, "Received service discovery response, service count:%d", count);

	if (count == 0)
	{
		log_level(LOG_WARNING, "Service not found");
		return;
	}

	/* currently service_discovery_start only support 1 service discover in single call */
	service_index = 0;
	service = &(p_ble_gattc_evt->params.prim_srvc_disc_rsp.services[service_index]);

	char uuid_string[STRING_BUFFER_SIZE] = { 0 };
	get_uuid_string(service->uuid.uuid, uuid_string);
	log_level(LOG_DEBUG, "Service discovered UUID: 0x%04X(%s), handle range:0x%04X - 0x%04X",
		service->uuid.uuid, uuid_string,
		service->handle_range.start_handle, service->handle_range.end_handle);

	m_service_start_handle = service->handle_range.start_handle;
	m_service_end_handle = service->handle_range.end_handle;
	m_discovered_handle = service->handle_range.start_handle;
	m_char_idx = m_char_list.size();

	char_discovery_start(service->handle_range);
}

/**@brief Function called on BLE_GATTC_EVT_CHAR_DISC_RSP event.
 *
 * @details Update characteristic state and proceed to discovering the characteristicss descriptors.
 *
 * @param[in] p_ble_gattc_evt Characteristic Discovery Response Event.
 */
static void on_characteristic_discovery_response(const ble_gattc_evt_t * const p_ble_gattc_evt)
{
	int count = p_ble_gattc_evt->params.char_disc_rsp.count;

	if (p_ble_gattc_evt->gatt_status != NRF_SUCCESS || count == 0)
	{
		log_level(LOG_WARNING, " Characteristic discovery failed or empty, code 0x%X count=%d",
			p_ble_gattc_evt->gatt_status, count);

		m_cond_find.notify_all();

		// invoke callback to caller when serviec discovery terminated
		for (auto &fn : m_callback_fn_list[FN_ON_SERVICE_DISCOVERED]) {
			((fn_on_service_discovered)fn)(m_service_start_handle, m_char_list.size());
		}
		return;
	}

	log_level(LOG_INFO, " Received characteristic discovery response, characteristics count: %d", count);

	char uuid_string[STRING_BUFFER_SIZE] = { 0 };
	for (int i = 0; i < count; i++)
	{
		memset(uuid_string, 0, sizeof(uuid_string));
		get_uuid_string(p_ble_gattc_evt->params.char_disc_rsp.chars[i].uuid.uuid, uuid_string);
		log_level(LOG_DEBUG, " Characteristic handle:0x%04X, UUID: 0x%04X(%s) decl:0x%04X prop(LSB):0x%x, r/w/n:%d/%d/%d",
			p_ble_gattc_evt->params.char_disc_rsp.chars[i].handle_value,
			p_ble_gattc_evt->params.char_disc_rsp.chars[i].uuid.uuid,
			uuid_string,
			p_ble_gattc_evt->params.char_disc_rsp.chars[i].handle_decl,
			p_ble_gattc_evt->params.char_disc_rsp.chars[i].char_props,
			p_ble_gattc_evt->params.char_disc_rsp.chars[i].char_props.read,
			p_ble_gattc_evt->params.char_disc_rsp.chars[i].char_props.write,
			p_ble_gattc_evt->params.char_disc_rsp.chars[i].char_props.notify);

		// store characteristic to list
		if (m_char_list.size() > 0) {
			m_char_list[m_char_list.size() - 1].handle_range.end_handle = 
				p_ble_gattc_evt->params.char_disc_rsp.chars[i].handle_decl - 1;
		}
		dev_char_t dev_char;
		// NOTICE: only care the value handle for further usage
		dev_char.handle = p_ble_gattc_evt->params.char_disc_rsp.chars[i].handle_value;
		dev_char.uuid = p_ble_gattc_evt->params.char_disc_rsp.chars[i].uuid.uuid;
		dev_char.handle_decl = p_ble_gattc_evt->params.char_disc_rsp.chars[i].handle_decl;
		dev_char.handle_range.start_handle = dev_char.handle_decl;
		dev_char.handle_range.end_handle = m_service_end_handle;
		// TODO: should check item exists by handle?
		m_char_list.push_back(dev_char);

		auto handle_value = p_ble_gattc_evt->params.char_disc_rsp.chars[i].handle_value;
		// std::map operator[] will create pair if key not exists, and fixed data_t.p_data allocation
		memset(m_read_data[handle_value].p_data, 0, DATA_BUFFER_SIZE);
	}
	
	// NOTICE: m_char_idx increases in on_descriptor_discovery_response
	if (m_char_idx < m_char_list.size())
		descr_discovery_start(m_char_list[m_char_idx].handle_range);
}

/**@brief Function called on BLE_GATTC_EVT_DESC_DISC_RSP event.
 *
 * @details Update CCCD descriptor state and proceed to prompting user to toggle notifications.
 *
 * @param[in] p_ble_gattc_evt Descriptor Discovery Response Event.
 */
static void on_descriptor_discovery_response(const ble_gattc_evt_t * const p_ble_gattc_evt)
{
	int count = p_ble_gattc_evt->params.desc_disc_rsp.count;

	if (p_ble_gattc_evt->gatt_status != NRF_SUCCESS || count == 0)
	{
		log_level(LOG_WARNING, " Descriptor discovery failed or empty, code 0x%X count=%d",
			p_ble_gattc_evt->gatt_status, count);

		m_cond_find.notify_all();

		// invoke callback to caller when serviec discovery terminated
		for (auto &fn : m_callback_fn_list[FN_ON_SERVICE_DISCOVERED]) {
			((fn_on_service_discovered)fn)(m_service_start_handle, m_char_list.size());
		}
		return;
	}

	log_level(LOG_INFO, " Received descriptor discovery response, descriptor count: %d", count);

	char uuid_string[STRING_BUFFER_SIZE] = { 0 };
	for (int i = 0; i < count; i++)
	{
		memset(uuid_string, 0, sizeof(uuid_string));
		get_uuid_string(p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid, uuid_string);
		log_level(LOG_DEBUG, " Descriptor handle: 0x%04X, UUID: 0x%04X(%s)",
			p_ble_gattc_evt->params.desc_disc_rsp.descs[i].handle,
			p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid,
			uuid_string);

		m_discovered_handle = 
			std::max(m_service_start_handle, p_ble_gattc_evt->params.desc_disc_rsp.descs[i].handle);

		// check characteristic index of m_char_list
		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_CHARACTERISTIC)
		{
			auto decl = p_ble_gattc_evt->params.desc_disc_rsp.descs[i].handle;
			auto found = std::find_if(m_char_list.begin(), m_char_list.end(),
				[decl](dev_char_t c) { return c.handle_decl == decl; });

			m_char_idx = std::distance(m_char_list.begin(), found);
			// leave count-loop to call char_discovery_start to build rest of characteristic items
			if (m_char_idx >= m_char_list.size())
				break;
			log_level(LOG_INFO, " Manipulate characteristic list %d of %d", (m_char_idx + 1), m_char_list.size());
		}

		// store descriptor to list
		ble_gattc_desc_t dev_desc = p_ble_gattc_evt->params.desc_disc_rsp.descs[i];
		m_char_list[m_char_idx].desc_list.push_back(dev_desc);

		// set cccd handle, refer to set_cccd_notification();
		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_CCCD)
		{
			m_char_list[m_char_idx].cccd_handle = dev_desc.handle;
			log_level(LOG_DEBUG, " CCCD descriptor save to idx=%d, handle=%x", m_char_idx, dev_desc.handle);
		}
		// set report reference handle, refer to read_report_refs()
		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_REPORT_REF_DESCR)
		{
			m_char_list[m_char_idx].report_ref_handle = dev_desc.handle;
			log_level(LOG_DEBUG, " Report reference descriptor save to idx=%d, handle=%x", m_char_idx, dev_desc.handle);
		}
		// handle represent HID protocol mode(nordic default PROTOCOL_MODE_BOOT 0x00, PROTOCOL_MODE_REPORT 0x01)
		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_PROTOCOL_MODE_CHAR)
		{
			log_level(LOG_DEBUG, " Protocol mode handle=%x", dev_desc.handle);
		}
		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_REPORT_CHAR)
		{
			log_level(LOG_DEBUG, " Report handle=%x", dev_desc.handle);
		}
		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_HID_INFORMATION_CHAR)
		{
			log_level(LOG_DEBUG, " HID info handle=%x", dev_desc.handle);
		}
		// handle represent data for the HID descriptors
		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_REPORT_MAP_CHAR)
		{
			log_level(LOG_DEBUG, " Report map handle=%x", dev_desc.handle);
		}
		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_HID_CONTROL_POINT_CHAR)
		{
			log_level(LOG_DEBUG, " HID control point handle=%x", dev_desc.handle);
		}

		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_BATTERY_LEVEL_CHAR)
		{
			//BLE_GATT_STATUS_ATTERR_INSUF_AUTHENTICATION
			// Authentication required, auth_start()
			//BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED
			// Cannot write hvx enabling notification messages
			m_battery_level_handle = p_ble_gattc_evt->params.desc_disc_rsp.descs[i].handle;
			log_level(LOG_DEBUG, " Battery level handle saved, handle=%x", m_battery_level_handle);
		}

		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_GAP_CHARACTERISTIC_DEVICE_NAME)
		{
			m_device_name_handle = p_ble_gattc_evt->params.desc_disc_rsp.descs[i].handle;
			log_level(LOG_DEBUG, " Device name handle saved");
		}

	}

	// TODO: may check all descrs are responsed? before move to the next char or back to discover char

	if (m_char_idx < m_char_list.size() - 1) {
		// move to find descriptors of the next characteristic
		descr_discovery_start(m_char_list[++m_char_idx].handle_range);
	}
	else {
		// move to find rest of characteristics
		ble_gattc_handle_range_t range{ m_discovered_handle, m_service_end_handle };
		char_discovery_start(range);
	}

}


static void on_read_characteristic_value_by_uuid_response(const ble_gattc_evt_t *const p_ble_gattc_evt)
{
	if (p_ble_gattc_evt->gatt_status != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "Error read char val by uuid operation, error code 0x%x", p_ble_gattc_evt->gatt_status);
		return;
	}

	if (p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.count == 0 ||
		p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.value_len == 0)
	{
		log_level(LOG_WARNING, "Error read char val by uuid operation, no handle count or value length");
		return;
	}

	for (int i = 0; i < p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.count; i++)
	{
		log_level(LOG_DEBUG, "Received read char by uuid, value handle:0x%04X len:%d.",
			p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.handle_value[i],
			p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.value_len);
	}

	//DEBUG: directly use handle from descriptor?
	uint32_t error_code = sd_ble_gattc_read(
		m_adapter,
		m_connection_handle,
		p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.handle_value[0],
		0
	);
	log_level(LOG_DEBUG, " read from handle0:0x%04X", 
		p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.handle_value[0]);
}

static void on_read_characteristic_values_response(const ble_gattc_evt_t *const p_ble_gattc_evt)
{
	if (p_ble_gattc_evt->gatt_status != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "Error read char vals operation, error code 0x%x", p_ble_gattc_evt->gatt_status);
		return;
	}

	if (p_ble_gattc_evt->params.char_vals_read_rsp.len == 0)
	{
		log_level(LOG_WARNING, "Error read char vals operation, no att values length");
		return;
	}

	auto len = p_ble_gattc_evt->params.char_vals_read_rsp.len;
	uint8_t *p_data = (uint8_t *)p_ble_gattc_evt->params.char_vals_read_rsp.values;

	//char read_bytes[DATA_BUFFER_SIZE] = { 0 };
	//memcpy_s(&read_bytes[0], DATA_BUFFER_SIZE, p_data, len);
	sprintf_s(m_log_msg, "Received read char vals len:%d data: ", len);
	convert_byte_string((char *)p_data, len, &(m_log_msg[strlen(m_log_msg)]));
	log_level(LOG_DEBUG, m_log_msg);
}

static void on_read_response(const ble_gattc_evt_t *const p_ble_gattc_evt)
{
	auto rsp_handle = p_ble_gattc_evt->params.read_rsp.handle;

	if (p_ble_gattc_evt->gatt_status != NRF_SUCCESS || 
		p_ble_gattc_evt->params.read_rsp.len == 0)
	{
		// refer to BLE_GATT_STATUS_ATTERR_INSUF_AUTHENTICATION if handle access required authentication
		// refer to BLE_GATT_STATUS_ATTERR_REQUEST_NOT_SUPPORTED if handle property not permitted
		log_level(LOG_ERROR, "Error. Read operation failed or data empty, handle:0x%04X code 0x%x",
			rsp_handle, p_ble_gattc_evt->gatt_status); //TODO: or warning?
		//TODO: do something next if any error occurred

		m_cond_read_write.notify_all();
		return;
	}

	uint8_t* p_data = (uint8_t *)p_ble_gattc_evt->params.read_rsp.data;
	uint16_t offset = p_ble_gattc_evt->params.read_rsp.offset;
	uint16_t len = p_ble_gattc_evt->params.read_rsp.len;

	sprintf_s(m_log_msg, "Received read response handle:0x%04X len:%d data: ", rsp_handle, len);
	convert_byte_string((char *)p_data, len, &(m_log_msg[strlen(m_log_msg)]));
	log_level(LOG_DEBUG, m_log_msg);

	// NOTICE: refer to on_characteristic_discovery_response has pre-allocated memory
	memset(m_read_data[rsp_handle].p_data, 0, DATA_BUFFER_SIZE);
	memcpy_s(m_read_data[rsp_handle].p_data, DATA_BUFFER_SIZE, p_data + offset, len);
	m_read_data[rsp_handle].len = len;
	// release lock for data_read()
	m_cond_read_write.notify_all();

	// check handle is report reference descriptor, to read the next report reference.
	//ASSERT: rsp_handle == m_char_list[m_char_idx].report_ref_handle
	for (int i = 0; i < m_char_list.size(); i++) {
		if (m_char_list[i].report_ref_handle == rsp_handle) {
			memcpy_s(&(m_char_list[i].report_ref[0]), len, p_data + offset, len);
			m_char_list[i].report_ref_is_read = true;
			// read the next report reference
			read_report_refs(0);
			break;
		}
	}
}

/**@brief Function called on BLE_GATTC_EVT_WRITE_RSP event.
 *
 * @param[in] p_ble_gattc_evt Write Response Event.
 */
static void on_write_response(const ble_gattc_evt_t * const p_ble_gattc_evt)
{
	auto rsp_handle = p_ble_gattc_evt->params.write_rsp.handle;

	if (p_ble_gattc_evt->gatt_status != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "Error. Write operation failed or data empty. handle 0x%04X code 0x%X",
			rsp_handle, p_ble_gattc_evt->gatt_status); //TODO: or warning?
		m_cond_read_write.notify_all();
		return;
	}

	uint8_t* p_data = (uint8_t *)p_ble_gattc_evt->params.write_rsp.data;
	uint16_t offset = p_ble_gattc_evt->params.write_rsp.offset;
	uint16_t len = p_ble_gattc_evt->params.write_rsp.len;

	log_level(LOG_DEBUG, "Sent write response handle:0x%04X len:%d data: ...", rsp_handle, len);

	// NOTICE: refer to on_characteristic_discovery_response has pre-allocated memory
	memset(m_write_data[rsp_handle].p_data, 0, DATA_BUFFER_SIZE);
	memcpy_s(m_write_data[rsp_handle].p_data, DATA_BUFFER_SIZE, p_data + offset, len);
	m_write_data[rsp_handle].len = len;
	// release lock for data_write()
	m_cond_read_write.notify_all();

	// check handle is CCCD, to set the next CCCD notification.
	//ASSERT: rsp_handle == m_char_list[m_char_idx].cccd_handle
	for (int i = 0; i < m_char_list.size(); i++) {
		if (m_char_list[i].cccd_handle == rsp_handle) {
			m_char_list[i].cccd_enabled = true;
			// set the next cccd handle
			set_cccd_notification(0);
			break;
		}
	}
}

/**@brief Function called on BLE_GATTC_EVT_HVX event.
 *
 * @details Logs the received data from handle value notification(HVX).
 *
 * @param[in] p_ble_gattc_evt Handle Value Notification/Indication Event.
 */
static void on_hvx(const ble_gattc_evt_t * const p_ble_gattc_evt)
{
	auto hvx_handle = p_ble_gattc_evt->params.hvx.handle;
	auto len = p_ble_gattc_evt->params.hvx.len;
	auto p_data = p_ble_gattc_evt->params.hvx.data;
	
	uint32_t char_idx = -1;
	for (int i = 0; i < m_char_list.size(); i++) {
		if (m_char_list[i].handle == hvx_handle) {
			char_idx = i;
			break;
		}
	}
	if (char_idx == -1) {
		log_level(LOG_WARNING, "Received hvx from handle:0x%04X not in list", hvx_handle);
		return;
	}

	char uuid_string[STRING_BUFFER_SIZE] = { 0 };
	get_uuid_string(m_char_list[char_idx].uuid, uuid_string);
	sprintf_s(m_log_msg, "Received hvx from handle:0x%04X uuid:0x%04X(%s) ",
		hvx_handle, m_char_list[char_idx].uuid, uuid_string);

	auto msg_pos = &(m_log_msg[strlen(m_log_msg)]);
	if (m_char_list[char_idx].report_ref_is_read) {
		msg_pos += sprintf_s(msg_pos, 5, "ref:");
		msg_pos += convert_byte_string((char*)m_char_list[char_idx].report_ref, 2, msg_pos);
	}

	msg_pos += sprintf_s(msg_pos, 16, "len:%d data: ", len);
	convert_byte_string((char*)p_data, len, msg_pos);
	log_level(LOG_DEBUG, m_log_msg);

	// NOTICE: refer to on_characteristic_discovery_response has pre-allocated memory
	memset(m_read_data[hvx_handle].p_data, 0, DATA_BUFFER_SIZE);
	memcpy_s(m_read_data[hvx_handle].p_data, DATA_BUFFER_SIZE, p_data, len);
	m_read_data[hvx_handle].len = len;

	for (auto &fn : m_callback_fn_list[FN_ON_DATA_RECEIVED]) {
		((fn_on_data_received)fn)(hvx_handle, m_read_data[hvx_handle].p_data, m_read_data[hvx_handle].len);
	}
}

/**@brief Function called on BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST event.
 *
 * @details Update GAP connection parameters.
 *
 * @param[in] p_ble_gap_evt Connection Parameter Update Event.
 */
static void on_conn_params_update_request(const ble_gap_evt_t * const p_ble_gap_evt)
{
	auto conn_params = p_ble_gap_evt->
		params.conn_param_update_request.conn_params;
	uint32_t err_code = sd_ble_gap_conn_param_update(m_adapter, m_connection_handle,
		&(conn_params));
	log_level(LOG_DEBUG, "connection update request code=%d min=%d max=%d late=%d timeout=%d",
		err_code,
		(int)(conn_params.min_conn_interval * 1.25),
		(int)(conn_params.max_conn_interval * 1.25),
		conn_params.slave_latency,
		(int)(conn_params.conn_sup_timeout / 100));

	if (err_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "Conn params update failed, err_code %d", err_code);
	}
}

/*
connection parameters responsed from peripheral
*/
static void on_conn_params_update(const ble_gap_evt_t * const p_ble_gap_evt)
{
	auto conn_params = &(p_ble_gap_evt->params.conn_param_update.conn_params);

	log_level(LOG_INFO, "Connection params updated, interval:%d[ms] latency:%d timeout:%d[ms]",
		(int)(conn_params->min_conn_interval * 1.25),
		conn_params->slave_latency,
		(int)(conn_params->conn_sup_timeout * 100));

	uint32_t error_code;
	ble_gap_conn_sec_t conn_sec;
	error_code = sd_ble_gap_conn_sec_get(m_adapter, m_connection_handle, &conn_sec);
	log_level(LOG_DEBUG, " get security code=%d mode=%d level=%d",
		error_code, conn_sec.sec_mode.sm, conn_sec.sec_mode.lv);
}

/*
request security parameters for peripheral authentication
*/
static void on_sec_params_request(const ble_gap_evt_t * const p_ble_gap_evt)
{
	auto peer_params = p_ble_gap_evt->params.sec_params_request.peer_params;

	log_level(LOG_DEBUG, " on security params request, peer: bond=%d io=%d min=%d max=%d ownenc=%d peerenc=%d",
		peer_params.bond, peer_params.io_caps,
		peer_params.min_key_size, peer_params.max_key_size,
		peer_params.kdist_own.enc, peer_params.kdist_peer.enc);

	memcpy_s(m_own_pk.pk, BLE_GAP_LESC_P256_PK_LEN, m_public_key, ECC_P256_PK_LEN);
	sprintf_s(m_log_msg, " own_pk= ");
	convert_byte_string((char*)m_own_pk.pk, BLE_GAP_LESC_P256_PK_LEN, &m_log_msg[strlen(m_log_msg)]);
	log_level(LOG_DEBUG, m_log_msg);

	ble_gap_sec_keyset_t sec_keyset = { 0 };
	sec_keyset.keys_own.p_enc_key = &m_own_enc;
	sec_keyset.keys_own.p_id_key = &m_own_id;
	sec_keyset.keys_own.p_sign_key = &m_own_sign;
	sec_keyset.keys_own.p_pk = &m_own_pk;
	sec_keyset.keys_peer.p_enc_key = &m_peer_enc;
	sec_keyset.keys_peer.p_id_key = &m_peer_id;
	sec_keyset.keys_peer.p_sign_key = &m_peer_sign;
	sec_keyset.keys_peer.p_pk = &m_peer_pk;
	// NOTICE: to the peripheral role, given security_param as null, generate public key to keyset
	uint32_t err_code = sd_ble_gap_sec_params_reply(
		m_adapter, m_connection_handle, BLE_GAP_SEC_STATUS_SUCCESS, 0, &sec_keyset);
	log_level(LOG_DEBUG, " on security params request, return=%d should be %d", err_code, NRF_SUCCESS);
}


#if NRF_SD_BLE_API >= 3
/**@brief Function called on BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST event.
 *
 * @details Replies to an ATT_MTU exchange request by sending an Exchange MTU Response to the client.
 *
 * @param[in] p_ble_gatts_evt Exchange MTU Request Event.
 */
static void on_exchange_mtu_request(const ble_gatts_evt_t* const p_ble_gatts_evt)
{
	uint32_t err_code = sd_ble_gatts_exchange_mtu_reply(
		m_adapter,
		m_connection_handle,
#if NRF_SD_BLE_API < 5
		GATT_MTU_SIZE_DEFAULT);
#else
		BLE_GATT_ATT_MTU_DEFAULT);
#endif

	if (err_code != NRF_SUCCESS)
	{
		printf("MTU exchange request reply failed, err_code %d\n", err_code);
		fflush(stdout);
	}
}

/**@brief Function called on BLE_GATTC_EVT_EXCHANGE_MTU_RSP event.
 *
 * @details Logs the new BLE server RX MTU size.
 *
 * @param[in] p_ble_gattc_evt Exchange MTU Response Event.
 */
static void on_exchange_mtu_response(const ble_gattc_evt_t* const p_ble_gattc_evt)
{
	uint16_t server_rx_mtu = p_ble_gattc_evt->params.exchange_mtu_rsp.server_rx_mtu;

	log_level(LOG_DEBUG, "MTU response received. New ATT_MTU is %d\n", server_rx_mtu);
	fflush(stdout);
}
#endif


#pragma endregion

//TODO: DEDBUG: currently hardcode oob data provided from peripheral for behavior debug
//  originally requirement is for Windows swift pair
/* NOTICE: dummy legacy oob data for debug
* reference: https://devzone.nordicsemi.com/f/nordic-q-a/47932/oob-works-with-mcp-but-fails-with-nrf-connect
*/
static char m_oob_debug[16] = { 0xAA, 0xBB, 0xCC, 0xDD,
									  0xEE, 0xFF, 0x99, 0x88,
									  0x77, 0x66, 0x55, 0x44,
									  0x33, 0x22, 0x11, 0x00 };


/** Event dispatcher */

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in] adapter The transport adapter.
 * @param[in] p_ble_evt Bluetooth stack event.
 */
static void ble_evt_dispatch(adapter_t * adapter, ble_evt_t * p_ble_evt)
{
	if (p_ble_evt == NULL)
	{
		log_level(LOG_WARNING, "Received an empty BLE event");
		return;
	}

	uint32_t err_code = 0;

	switch (p_ble_evt->header.evt_id)
	{
	case BLE_GAP_EVT_CONNECTED:
		on_connected(&(p_ble_evt->evt.gap_evt));
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnected(&(p_ble_evt->evt.gap_evt));
		break;

	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
		on_conn_params_update_request(&(p_ble_evt->evt.gap_evt));
		break;

	case BLE_GAP_EVT_CONN_PARAM_UPDATE:
	{
		on_conn_params_update(&(p_ble_evt->evt.gap_evt));
		// NOTICE: before associate specific services, 
		//         should apply param updated request for authentication(whether passkey and bond)
	}break;

	case BLE_GAP_EVT_SEC_REQUEST:
	{
		log_level(LOG_TRACE, "on sec request");
	}break;

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		// will compute own pk to reply to peripheral
		on_sec_params_request(&(p_ble_evt->evt.gap_evt));
		break;

	case BLE_GAP_EVT_CONN_SEC_UPDATE:
		log_level(LOG_DEBUG, " on conn security updated, mode=%d level=%d",
			p_ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.sm,
			p_ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.lv);
		break;

	case BLE_GAP_EVT_AUTH_STATUS:
	{
		log_level(LOG_DEBUG, " on auth status, status=%d, bond=%d",
			p_ble_evt->evt.gap_evt.params.auth_status.auth_status,
			p_ble_evt->evt.gap_evt.params.auth_status.bonded);

		// check auth status and bonded by given security parameter
		// NOTICE: w/o bond may not have enough privilege interacting most services
		if (p_ble_evt->evt.gap_evt.params.auth_status.auth_status == BLE_GAP_SEC_STATUS_SUCCESS &&
			p_ble_evt->evt.gap_evt.params.auth_status.bonded | ~m_sec_params.bond) {
			
			m_is_authenticated = true;

			m_cond_find.notify_all();
			
			// NOTICE: let caller decide the next action, can wait util conn param updated to discover service
			//         or do immediately after authentication completed
			for (auto &fn : m_callback_fn_list[FN_ON_AUTHENTICATED]) {
				((fn_on_authenticated)fn)(p_ble_evt->evt.gap_evt.params.auth_status.auth_status);
			}
		}
		else {
			if (m_callback_fn_list[FN_ON_FAILED].size() > 0) {
				std::string str = std::string("auth failed: " +
					std::to_string(p_ble_evt->evt.gap_evt.params.auth_status.auth_status));
				for (auto &fn : m_callback_fn_list[FN_ON_FAILED]) {
					((fn_on_failed)fn)(str.c_str());
				}
			}
		}
	}break;

	case BLE_GAP_EVT_LESC_DHKEY_REQUEST:
	{
		// if sd_ble_gap_authenticate lesc = 1
		log_level(LOG_DEBUG, " on lesc dhkey request, oobd_req=%d, peer_pk0=%d",
			p_ble_evt->evt.gap_evt.params.lesc_dhkey_request.oobd_req,
			p_ble_evt->evt.gap_evt.params.lesc_dhkey_request.p_pk_peer->pk[0]);

		// print peer pubkey
		sprintf_s(m_log_msg, " peer_pk= ");
		convert_byte_string((char*)p_ble_evt->evt.gap_evt.params.lesc_dhkey_request.p_pk_peer->pk,
			BLE_GAP_LESC_P256_PK_LEN, &m_log_msg[strlen(m_log_msg)]);
		log_level(LOG_DEBUG, m_log_msg);
		// valid peer pubkey
		int ecc_res = ecc_p256_valid_public_key(p_ble_evt->evt.gap_evt.params.lesc_dhkey_request.p_pk_peer->pk);
		log_level(LOG_DEBUG, " peer_pk valid=%d should be 1", ecc_res);

		// compute share secret from peer pk
		ble_gap_lesc_dhkey_t dhkey = { 0 };
		ecc_p256_compute_sharedsecret(m_private_key, p_ble_evt->evt.gap_evt.params.lesc_dhkey_request.p_pk_peer->pk, dhkey.key);
		sprintf_s(m_log_msg, " compute ss= ");
		convert_byte_string((char*)dhkey.key, BLE_GAP_LESC_DHKEY_LEN, &m_log_msg[strlen(m_log_msg)]);
		log_level(LOG_DEBUG, m_log_msg);

		// sd_ble_gap_lesc_dhkey_reply: reply shared
		err_code = sd_ble_gap_lesc_dhkey_reply(m_adapter, m_connection_handle, &dhkey);
		log_level(LOG_DEBUG, " reply dhkey: %d", err_code);
		
		// sd_ble_gap_lesc_oob_data_get: get own oob
		ble_gap_lesc_p256_pk_t pk_own = { 0 };
		memcpy_s(pk_own.pk, ECC_P256_PK_LEN, m_public_key, ECC_P256_PK_LEN);
		ble_gap_lesc_oob_data_t oob_own = { 0 };
		err_code = sd_ble_gap_lesc_oob_data_get(m_adapter, m_connection_handle, &pk_own, &oob_own);
		log_level(LOG_TRACE, " oob_get: %d", err_code);

		sprintf_s(m_log_msg, "  pk_own= ");
		convert_byte_string((char*)pk_own.pk, BLE_GAP_LESC_P256_PK_LEN, &m_log_msg[strlen(m_log_msg)]);
		log_level(LOG_TRACE, m_log_msg);
		sprintf_s(m_log_msg, "  oob_own.random= ");
		convert_byte_string((char*)oob_own.r, BLE_GAP_SEC_KEY_LEN, &m_log_msg[strlen(m_log_msg)]);
		log_level(LOG_TRACE, m_log_msg);
		sprintf_s(m_log_msg, "  oob_own.confirm= ");
		convert_byte_string((char*)oob_own.c, BLE_GAP_SEC_KEY_LEN, &m_log_msg[strlen(m_log_msg)]);
		log_level(LOG_TRACE, m_log_msg);

		if (p_ble_evt->evt.gap_evt.params.lesc_dhkey_request.oobd_req == 0)
			break;

		// sd_ble_gap_lesc_oob_data_set: set own oob, peer oob
		ble_gap_lesc_oob_data_t oob_peer = { 0 }; // TODO: input required
		err_code = sd_ble_gap_lesc_oob_data_set(m_adapter, m_connection_handle, &oob_own, &oob_peer);
		log_level(LOG_DEBUG, " oob_set: %d", err_code);
	}break;

	case BLE_GAP_EVT_AUTH_KEY_REQUEST:
	{
		uint8_t key_type = p_ble_evt->evt.gap_evt.params.auth_key_request.key_type;
		uint8_t *key = NULL;
		// provide fixed passkey
		if (key_type == BLE_GAP_AUTH_KEY_TYPE_PASSKEY) {
			key = (uint8_t*)&m_passkey[0];
			log_level(LOG_INFO, " use %s for passkey", m_passkey);
		}
		else if (key_type == BLE_GAP_AUTH_KEY_TYPE_OOB) {
			//TODO: DEBUG: currently hardcode oob data provided from peripheral for behavior debug
			//  originally requirement is for Windows swift pair

			key = (uint8_t*)&m_oob_debug[0];
			sprintf_s(m_log_msg, " on auth key req by OOB: ");
			convert_byte_string(m_oob_debug, 16, &m_log_msg[strlen(m_log_msg)]);
			log_level(LOG_DEBUG, m_log_msg);
		}
		err_code = sd_ble_gap_auth_key_reply(m_adapter, m_connection_handle, key_type, key);
		log_level(LOG_DEBUG, " on auth key req, keytype:%d return:%d", key_type, err_code);
		
		// only notify to caller which auth via passkey
		if (key_type != BLE_GAP_AUTH_KEY_TYPE_PASSKEY)
			break;

		if (m_callback_fn_list[FN_ON_PASSKEY_REQUIRED].size() > 0) {
			std::string str = std::string(m_passkey);
			for (auto &fn : m_callback_fn_list[FN_ON_PASSKEY_REQUIRED]) {
				((fn_on_passkey_required)fn)(str.c_str());
			}
		}
	}break;

	case BLE_GAP_EVT_PASSKEY_DISPLAY:
	{
		//TODO: asume only works in passkey type, w/o verified
		uint8_t key[6] = { 0 };
		memcpy_s(&key[0], 6, p_ble_evt->evt.gap_evt.params.passkey_display.passkey, 6);
		sprintf_s(m_log_msg, " on passkey display, key: ");
		for (int i = 0; i < 6 && key[i]; i++)
			strcat_s(m_log_msg, (char*)key[i]);
		log_level(LOG_INFO, m_log_msg);

		err_code = sd_ble_gap_auth_key_reply(m_adapter, m_connection_handle, BLE_GAP_AUTH_KEY_TYPE_PASSKEY, key);
		log_level(LOG_DEBUG, " on passkey display auth reply, code:%d", err_code);

		if (m_callback_fn_list[FN_ON_PASSKEY_REQUIRED].size() > 0) {
			std::string str = std::string((char*)key, 6);
			for (auto &fn : m_callback_fn_list[FN_ON_PASSKEY_REQUIRED]) {
				((fn_on_passkey_required)fn)(str.c_str());
			}
		}
	}break;

	case BLE_GAP_EVT_ADV_REPORT:
		on_adv_report(&(p_ble_evt->evt.gap_evt));
		break;

	case BLE_GAP_EVT_TIMEOUT:
	{
		on_timeout(&(p_ble_evt->evt.gap_evt));

		if (m_callback_fn_list[FN_ON_FAILED].size() > 0) {
			std::string str = std::string("timeout failed: " +
				std::to_string(p_ble_evt->evt.gap_evt.params.timeout.src));
			for (auto &fn : m_callback_fn_list[FN_ON_FAILED]) {
				((fn_on_failed)fn)(str.c_str());
			}
		}
	}break;

	case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP:
		on_service_discovery_response(&(p_ble_evt->evt.gattc_evt));
		break;

	case BLE_GATTC_EVT_CHAR_DISC_RSP:
		on_characteristic_discovery_response(&(p_ble_evt->evt.gattc_evt));
		break;

	case BLE_GATTC_EVT_DESC_DISC_RSP:
		on_descriptor_discovery_response(&(p_ble_evt->evt.gattc_evt));
		break;

	case BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP:
		on_read_characteristic_value_by_uuid_response(&(p_ble_evt->evt.gattc_evt));
		break;
	case BLE_GATTC_EVT_READ_RSP:
		on_read_response(&(p_ble_evt->evt.gattc_evt));
		break;
	case BLE_GATTC_EVT_CHAR_VALS_READ_RSP:
		on_read_characteristic_values_response(&(p_ble_evt->evt.gattc_evt));
		break;

	case BLE_GATTC_EVT_WRITE_RSP:
		on_write_response(&(p_ble_evt->evt.gattc_evt));
		break;

	case BLE_GATTC_EVT_HVX:
		on_hvx(&(p_ble_evt->evt.gattc_evt));
		break;

#if NRF_SD_BLE_API >= 3
	case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
		log_level(LOG_DEBUG, "evt exchange mtu request.");
		on_exchange_mtu_request(&(p_ble_evt->evt.gatts_evt));
		break;

	case BLE_GATTC_EVT_EXCHANGE_MTU_RSP:
		log_level(LOG_DEBUG, "evt exchange mtu response.");
		on_exchange_mtu_response(&(p_ble_evt->evt.gattc_evt));
		break;
#endif

#if NRF_SD_BLE_API >= 5

	case BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE:
		log_level(LOG_DEBUG, "write cmd tx complete.");
		break;

	case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
		log_level(LOG_INFO, "Maximum packet length updated: rx=%d bytes, %d us, tx=%d bytes, %d us",
			p_ble_evt->evt.gap_evt.params.data_length_update.effective_params.max_rx_octets,
			p_ble_evt->evt.gap_evt.params.data_length_update.effective_params.max_rx_time_us,
			p_ble_evt->evt.gap_evt.params.data_length_update.effective_params.max_tx_octets,
			p_ble_evt->evt.gap_evt.params.data_length_update.effective_params.max_tx_time_us);
		break;

	case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
	{
		//https://infocenter.nordicsemi.com/topic/com.nordic.infocenter.s132.api.v5.0.0/s132_msc_overview.html?cp=4_7_3_7_1
		// GAP MSC -> Data Length Update Procedure
		auto peer_params = p_ble_evt->evt.gap_evt.params.data_length_update_request.peer_params;
		log_level(LOG_DEBUG, "evt data len update request rx=%d bytes, %d us, tx=%d bytes, %d us",
			peer_params.max_rx_octets,
			peer_params.max_rx_time_us,
			peer_params.max_tx_octets,
			peer_params.max_tx_time_us);

#define NRF_SDH_BLE_GAP_DATA_LENGTH 251

		ble_gap_data_length_params_t m_data_length = { 0 };
		m_data_length.max_rx_octets = NRF_SDH_BLE_GAP_DATA_LENGTH;
		m_data_length.max_tx_octets = NRF_SDH_BLE_GAP_DATA_LENGTH;
		m_data_length.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO;
		m_data_length.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO;
		ble_gap_data_length_limitation_t m_data_limit = { 0 };
		auto err_code = sd_ble_gap_data_length_update(m_adapter, m_connection_handle, &m_data_length, NULL);
		log_level(LOG_INFO, "Request maximum packet length update=%d: rx=%d bytes, %d us, tx=%d bytes, %d us",
			err_code,
			m_data_length.max_rx_octets, m_data_length.max_rx_time_us,
			m_data_length.max_tx_octets, m_data_length.max_tx_time_us);
		log_level(LOG_INFO, "Request maximum packet length limit: rx=%d bytes, tx=%d bytes, %d us",
			m_data_limit.rx_payload_limited_octets,
			m_data_limit.tx_payload_limited_octets, m_data_limit.tx_rx_time_limited_us);
	}break;

	case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
	{
		log_level(LOG_INFO, "PHY update request.");
		ble_gap_phys_t const phys =
		{
			BLE_GAP_PHY_AUTO, /*tx_phys*/
			BLE_GAP_PHY_AUTO, /*rx_phys*/
		};
		err_code = sd_ble_gap_phy_update(m_adapter, m_connection_handle, &phys);
		if (err_code != NRF_SUCCESS)
		{
			log_level(LOG_ERROR, "PHY update request reply failed, err_code %d", err_code);
		}
	} break;

#endif

	default:
		log_level(LOG_INFO, "Received an un-handled event with ID: %d", p_ble_evt->header.evt_id);
		break;
	}
}

/**@brief Function for initializing the BLE stack.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t ble_stack_init()
{
	uint32_t            err_code;
	uint32_t *          app_ram_base = NULL;

#if NRF_SD_BLE_API <= 3
	ble_enable_params_t ble_enable_params;
	memset(&ble_enable_params, 0, sizeof(ble_enable_params));
#endif

#if NRF_SD_BLE_API == 3
	ble_enable_params.gatt_enable_params.att_mtu = GATT_MTU_SIZE_DEFAULT;
#elif NRF_SD_BLE_API < 3
	ble_enable_params.gatts_enable_params.attr_tab_size = BLE_GATTS_ATTR_TAB_SIZE_DEFAULT;
	ble_enable_params.gatts_enable_params.service_changed = false;
	ble_enable_params.common_enable_params.p_conn_bw_counts = NULL;
	ble_enable_params.common_enable_params.vs_uuid_count = 1;
#endif

#if NRF_SD_BLE_API <= 3
	ble_enable_params.gap_enable_params.periph_conn_count = 1;
	ble_enable_params.gap_enable_params.central_conn_count = 1;
	ble_enable_params.gap_enable_params.central_sec_count = 1;

	err_code = sd_ble_enable(m_adapter, &ble_enable_params, app_ram_base);
#else
	err_code = sd_ble_enable(m_adapter, app_ram_base);
#endif

	switch (err_code) {
	case NRF_SUCCESS:
		break;
	case NRF_ERROR_INVALID_STATE:
		log_level(LOG_WARNING, "BLE stack already enabled");
		break;
	default:
		log_level(LOG_ERROR, "Failed to enable BLE stack. Error code: %d", err_code);
		break;
	}

	return err_code;
}

#if NRF_SD_BLE_API < 5
/**@brief Set BLE option for the BLE role and connection bandwidth.
 *
 * @return NRF_SUCCESS on option set successfully, otherwise an error code.
 */
static uint32_t ble_options_set()
{
#if NRF_SD_BLE_API <= 3
	ble_opt_t        opt;
	ble_common_opt_t common_opt;

	common_opt.conn_bw.role = BLE_GAP_ROLE_CENTRAL;
	common_opt.conn_bw.conn_bw.conn_bw_rx = BLE_CONN_BW_HIGH;
	common_opt.conn_bw.conn_bw.conn_bw_tx = BLE_CONN_BW_HIGH;
	opt.common_opt = common_opt;

	return sd_ble_opt_set(m_adapter, BLE_COMMON_OPT_CONN_BW, &opt);
#else
	return NRF_ERROR_NOT_SUPPORTED;
#endif
}
#endif

#if NRF_SD_BLE_API >= 5
/**@brief Function for setting configuration for the BLE stack.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t ble_cfg_set(uint8_t conn_cfg_tag)
{
	const uint32_t ram_start = 0; // Value is not used by ble-driver
	uint32_t error_code;
	ble_cfg_t ble_cfg;

	// Configure the connection roles.
	memset(&ble_cfg, 0, sizeof(ble_cfg));

#if NRF_SD_BLE_API >= 6
	ble_cfg.gap_cfg.role_count_cfg.adv_set_count = BLE_GAP_ADV_SET_COUNT_DEFAULT;
#endif
	ble_cfg.gap_cfg.role_count_cfg.periph_role_count = 0;
	ble_cfg.gap_cfg.role_count_cfg.central_role_count = 1;
	ble_cfg.gap_cfg.role_count_cfg.central_sec_count = 1;; /*NOTICE: set for sd_ble_gap_authenticate*/

	error_code = sd_ble_cfg_set(m_adapter, BLE_GAP_CFG_ROLE_COUNT, &ble_cfg, ram_start);
	if (error_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "sd_ble_cfg_set() failed when attempting to set BLE_GAP_CFG_ROLE_COUNT. Error code: 0x%02X", error_code);
		return error_code;
	}

	// GAP config code block from nordic_uart_client example
#define NRF_SDH_BLE_GAP_EVENT_LENGTH 8/*320*/
#define NRF_SDH_BLE_GATT_MAX_MTU_SIZE 247

#if NRF_SD_BLE_API >= 5
	memset(&ble_cfg, 0, sizeof(ble_cfg));
	ble_cfg.conn_cfg.conn_cfg_tag = conn_cfg_tag;
	ble_cfg.conn_cfg.params.gap_conn_cfg.conn_count = 1;
	ble_cfg.conn_cfg.params.gap_conn_cfg.event_length = NRF_SDH_BLE_GAP_EVENT_LENGTH;
	error_code = sd_ble_cfg_set(m_adapter, BLE_CONN_CFG_GAP, &ble_cfg, ram_start);
	if (error_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "sd_ble_cfg_set() failed when attempting to set BLE_CONN_CFG_GAP. Error code: 0x%02X", error_code);
		return error_code;
	}
#endif

	//memset(&ble_cfg, 0x00, sizeof(ble_cfg));
	//ble_cfg.conn_cfg.conn_cfg_tag = conn_cfg_tag;
	ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu = NRF_SDH_BLE_GATT_MAX_MTU_SIZE/*150*/;

	error_code = sd_ble_cfg_set(m_adapter, BLE_CONN_CFG_GATT, &ble_cfg, ram_start);
	if (error_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "sd_ble_cfg_set() failed when attempting to set BLE_CONN_CFG_GATT. Error code: 0x%02X", error_code);
		return error_code;
	}

	ble_cfg.conn_cfg.params.gattc_conn_cfg.write_cmd_tx_queue_size = 10;
	error_code = sd_ble_cfg_set(m_adapter, BLE_CONN_CFG_GATTC, &ble_cfg, ram_start);
	if (error_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "sd_ble_cfg_set() failed when attempting to set BLE_CONN_CFG_GATTC. Error code: 0x%02X", error_code);
		return error_code;
	}

	ble_cfg.conn_cfg.params.gatts_conn_cfg.hvn_tx_queue_size = 10;
	error_code = sd_ble_cfg_set(m_adapter, BLE_CONN_CFG_GATTS, &ble_cfg, ram_start);
	if (error_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "sd_ble_cfg_set() failed when attempting to set BLE_CONN_CFG_GATTS. Error code: 0x%02X", error_code);
		return error_code;
	}

	ble_cfg.conn_cfg.params.l2cap_conn_cfg.ch_count = BLE_L2CAP_CH_COUNT_MAX;
	ble_cfg.conn_cfg.params.l2cap_conn_cfg.rx_mps = BLE_L2CAP_MPS_MIN;
	ble_cfg.conn_cfg.params.l2cap_conn_cfg.tx_mps = BLE_L2CAP_MPS_MIN;
	ble_cfg.conn_cfg.params.l2cap_conn_cfg.rx_queue_size = 10;
	ble_cfg.conn_cfg.params.l2cap_conn_cfg.tx_queue_size = 10;
	error_code = sd_ble_cfg_set(m_adapter, BLE_CONN_CFG_L2CAP, &ble_cfg, ram_start);
	if (error_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "sd_ble_cfg_set() failed when attempting to set BLE_CONN_CFG_L2CAP. Error code: 0x%02X", error_code);
		return error_code;
	}

	return NRF_SUCCESS;
}
#endif

/**@brief Function for initializing serial communication with the target nRF5 Bluetooth slave.
 *
 * @param[in] serial_port The serial port the target nRF5 device is connected to.
 *
 * @return The new transport adapter.
 */
static adapter_t * adapter_init(char * serial_port, uint32_t baud_rate)
{
	physical_layer_t  * phy;
	data_link_layer_t * data_link_layer;
	transport_layer_t * transport_layer;

	phy = sd_rpc_physical_layer_create_uart(serial_port,
		baud_rate,
		SD_RPC_FLOW_CONTROL_NONE,
		SD_RPC_PARITY_NONE);
	data_link_layer = sd_rpc_data_link_layer_create_bt_three_wire(phy, 250);
	transport_layer = sd_rpc_transport_layer_create(data_link_layer, 1500);
	return sd_rpc_adapter_create(transport_layer);
}

uint32_t callback_add(fn_callback_id_t fn_id, void* fn) {
	m_callback_fn_list[fn_id].push_back(fn);

	return 0;
}

/* init Nordic connectiviy dongle and register event for rpc*/
uint32_t dongle_init(char* serial_port, uint32_t baud_rate)
{
	if (m_dongle_initialized) {
		sprintf_s(m_log_msg, "Dongle must be reset before re-initialize(re-plug dongle is recommanded)");
		log_level(LOG_ERROR, m_log_msg);
		return NRF_ERROR_INVALID_STATE;
	}

	// init ecc and generate keypair for later usage?
	ecc_init();
	int ecc_res = ecc_p256_gen_keypair(m_private_key, m_public_key);
	uint8_t test_pubkey[ECC_P256_PK_LEN] = { 0 };
	ecc_res = ecc_p256_compute_pubkey(m_private_key, test_pubkey);
	// validate pubkey
	ecc_res = ecc_p256_valid_public_key(test_pubkey);
	log_level(LOG_DEBUG, "uECC check key pair: %d should be 1", ecc_res);

	uint32_t error_code;
	uint8_t  cccd_value = 0;
	
	log_level(LOG_DEBUG, "Serial port used: %s Baud rate used: %d", serial_port, baud_rate);

	m_adapter = adapter_init(serial_port, baud_rate);
#ifdef _DEBUG
	sd_rpc_log_handler_severity_filter_set(m_adapter, SD_RPC_LOG_INFO);
#else
	sd_rpc_log_handler_severity_filter_set(m_adapter, SD_RPC_LOG_INFO);
#endif
	error_code = sd_rpc_open(m_adapter, status_handler, ble_evt_dispatch, log_handler);

	if (error_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "Failed to open nRF BLE Driver. Error code: 0x%02X", error_code);
		return error_code;
	}

#if NRF_SD_BLE_API >= 5
	error_code = ble_cfg_set(m_config_id);
#endif

	error_code = ble_stack_init();

	if (error_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "Failed to init BLE stack. Error code: 0x%02X", error_code);
		return error_code;
	}

#if NRF_SD_BLE_API < 5
	error_code = ble_options_set();

	if (error_code != NRF_SUCCESS)
	{
		return error_code;
	}
#endif

	m_dongle_initialized = true;

	ble_version_t ver = { 0 };
	error_code = sd_ble_version_get(m_adapter, &ver);
	if (error_code != NRF_SUCCESS)
	{
		log_level(LOG_ERROR, "Failed to get connectivity FW versions. Error code: 0x%02X", error_code);
		return error_code;
	}
	else {
		log_level(LOG_INFO, "Connectivity FW Version: %d, Company: %d, SubVersion: %d", ver.version_number, ver.company_id, ver.subversion_number);
	}

	return error_code;
}

