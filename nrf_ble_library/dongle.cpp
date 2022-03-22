#include "dongle.h"
#include "ble.h"
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

enum
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
#define MAX_CONNECTION_INTERVAL         MSEC_TO_UNITS(7.5, UNIT_1_25_MS) /**< Determines maximum connection interval in milliseconds. */
#define SLAVE_LATENCY                   0                                /**< Slave Latency in number of connection events. */
#define CONNECTION_SUPERVISION_TIMEOUT  MSEC_TO_UNITS(4000, UNIT_10_MS)  /**< Determines supervision time-out in units of 10 milliseconds. */

// service
#define BLE_UUID_BATTERY_SRV 0x180F
#define BLE_UUID_HID_SRV 0x1812
// characteristic, refer to SDK https://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.s132.api.v2.0.0%2Fgroup__ble__types.html
#define BLE_UUID_BATTERY_LEVEL_CHAR   0x2A19
#define BLE_UUID_PROTOCOL_MODE_CHAR   0x2A4E
#define BLE_UUID_REPORT_CHAR   0x2A4D
#define BLE_UUID_REPORT_REF_DESCR 0x2908

#define BLE_UUID_CCCD                        0x2902
#define BLE_CCCD_NOTIFY                      0x01

// refer to SDK https://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.s132.api.v2.0.0%2Fgroup___b_l_e___h_c_i___s_t_a_t_u_s___c_o_d_e_s.html
#define BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION 0x13
#define BLE_HCI_LOCAL_HOST_TERMINATED_CONNECTION 0x16


/** Global variables */
static addr_t      m_connected_addr = { 0 }; /* intent or connected peripheral address */
static bool        m_is_connected = false; /* peripheral address has been connected(BLE_GAP_EVT_DISCONNECTED) */
static bool	       m_is_authenticated = false; /* peripheral address has been authenticated(BLE_GAP_EVT_AUTH_STATUS) */
static uint8_t     m_connected_devices = 0; /* number of connected devices */
static uint16_t    m_connection_handle = 0;
static uint16_t    m_service_start_handle = 0;
static uint16_t    m_service_end_handle = 0;
static uint16_t    m_device_name_handle = 0;
static uint16_t    m_battery_level_handle = 0;
static bool        m_connection_is_in_progress = false;
static adapter_t * m_adapter = NULL;

/* Callback functions from caller */
static std::map<fn_callback_id_t, std::vector<void*>> m_callback_fn_list;

/* Advertising addresses */
std::map<uint64_t, ble_gap_evt_adv_report_t> m_adv_list; /*addr, report*/

/* Discovered characteristic data structure */
typedef struct {
	uint16_t handle = 0; /* ble_gattc_char_t::handle_value */
	uint16_t uuid = 0; /* ble_gattc_char_t::uuid */
	ble_gattc_handle_range_t handle_range = { 0, 0 }; /* handle range of descs */
	uint16_t report_ref_handle = 0; /* none zero which has BLE_UUID_REPORT_REF_DESCR desc */
	unsigned char report_ref[32] = { 0 }; /* reference value of BLE_UUID_REPORT_REF_DESCR desc */
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

std::mutex m_mtx;
std::condition_variable m_cond;

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
	(uint8_t)0, /*lesc*/
	(uint8_t)0, /*keypress*/
	(uint8_t)BLE_GAP_IO_CAPS_KEYBOARD_ONLY, /*io_caps*/
	(uint8_t)0, /*oob*/
	(uint8_t)7, /*min_key_size, header default from driver test_util_adapater_wrapper*/
	(uint8_t)16, /*max_key_size, header default from driver test_util_adapater_wrapper*/
	{ /*kdist_own, default from pc-nrfconnect-ble/lib/reducer/securityReducer.js*/
		(uint8_t)1, /** < enc, Long Term Key and Master Identification. */
		(uint8_t)0, /** < id, Identity Resolving Key and Identity Address Information. */
		(uint8_t)0, /** < sign, Connection Signature Resolving Key. */
		(uint8_t)0 /** < link, Derive the Link Key from the LTK. */
	},
	{ /*kdist_peer*/
		1, 0, 0, 0
	}
};

// key pair for security params update, refer to security.h/cpp, duplicated from pc-ble-driver-js\src\driver_uecc.cpp
static uint8_t m_private_key[ECC_P256_SK_LEN] = { 0 };
static uint8_t m_public_key[ECC_P256_PK_LEN] = { 0 };

// encryption data for authentication
static ble_gap_enc_key_t m_own_enc = { 0 };
static ble_gap_id_key_t m_own_id = { 0 };
static ble_gap_sign_info_t m_own_sign = { 0 };
static ble_gap_lesc_p256_pk_t m_own_pk = { 0 };
static ble_gap_enc_key_t m_peer_enc = { 0 };
static ble_gap_id_key_t m_peer_id = { 0 };
static ble_gap_sign_info_t m_peer_sign = { 0 };
static ble_gap_lesc_p256_pk_t m_peer_pk = { 0 };


/** Global functions */

/**@brief Function for handling error message events from sd_rpc.
 *
 * @param[in] adapter The transport adapter.
 * @param[in] code Error code that the error message is associated with.
 * @param[in] message The error message that the callback is associated with.
 */
static void status_handler(adapter_t * adapter, sd_rpc_app_status_t code, const char * message)
{
	printf("Status: %d, message: %s\n", (uint32_t)code, message);
	fflush(stdout);
}

/**@brief Function for handling the log message events from sd_rpc.
 *
 * @param[in] adapter The transport adapter.
 * @param[in] severity Level of severity that the log message is associated with.
 * @param[in] message The log message that the callback is associated with.
 */
static void log_handler(adapter_t * adapter, sd_rpc_log_severity_t severity, const char * message)
{
	switch (severity)
	{
	case SD_RPC_LOG_ERROR:
		printf("Error: %s\n", message);
		fflush(stdout);
		break;

	case SD_RPC_LOG_WARNING:
		printf("Warning: %s\n", message);
		fflush(stdout);
		break;

	case SD_RPC_LOG_INFO:
		printf("Info: %s\n", message);
		fflush(stdout);
		break;

	default:
		printf("Log: %s\n", message);
		fflush(stdout);
		break;
	}
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

static void ble_address_to_uint64_convert(ble_gap_addr_t address, uint64_t *number)
{
	for (int i = BLE_GAP_ADDR_LEN - 1; i >= 0; --i)
	{
		*number += (uint64_t)address.addr[i] << (i * 8);
	}
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
static uint32_t adv_report_parse(uint8_t type, data_t * p_advdata, data_t * p_typedata)
{
	uint32_t  index = 0;
	uint8_t * p_data;

	p_data = p_advdata->p_data;

	while (index < p_advdata->data_len)
	{
		uint8_t field_length = p_data[index];
		uint8_t field_type = p_data[index + 1];

		if (field_type == type)
		{
			p_typedata->p_data = &p_data[index + 2];
			p_typedata->data_len = field_length - 1;
			return NRF_SUCCESS;
		}
		index += field_length + 1;
	}
	return NRF_ERROR_NOT_FOUND;
}

static bool get_adv_name(const ble_gap_evt_adv_report_t *p_adv_report, char * name)
{
	uint32_t err_code;
	data_t   adv_data;
	data_t   dev_name;

	// Initialize advertisement report for parsing
#if NRF_SD_BLE_API >= 6
	adv_data.p_data = (uint8_t *)p_adv_report->data.p_data;
	adv_data.data_len = p_adv_report->data.len;
#else
	adv_data.p_data = (uint8_t *)p_adv_report->data;
	adv_data.data_len = p_adv_report->dlen;
#endif

	//search for advertising names
	err_code = adv_report_parse(BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
		&adv_data,
		&dev_name);
	if (err_code == NRF_SUCCESS)
	{
		memcpy(name, dev_name.p_data, dev_name.data_len);
		return true;
	}

	// Look for the short local name if it was not found as complete
	err_code = adv_report_parse(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME,
		&adv_data,
		&dev_name);
	if (err_code == NRF_SUCCESS)
	{
		memcpy(name, dev_name.p_data, dev_name.data_len);
		return true;
	}

	// Look for the manufacturing data if it was not found as complete
	err_code = adv_report_parse(BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA,
		&adv_data,
		&dev_name);
	if (err_code == NRF_SUCCESS)
	{
		memcpy(name, dev_name.p_data, dev_name.data_len);
		return true;
	}

	return false;
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
}

#pragma endregion


#pragma region /** Start actions */

/**@brief Start scanning (GAP Discovery procedure, Observer Procedure).
 * *
 * @return NRF_SUCCESS on successfully initiating scanning procedure, otherwise an error code.
 */
uint32_t scan_start(float interval, float window, bool active, uint16_t timeout)
{
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

	if (error_code != NRF_SUCCESS)
	{
		printf("Scan start failed with error code: %d\n", error_code);
		fflush(stdout);
	}
	else
	{
		printf("Scan started\n");
		fflush(stdout);
	}

	return error_code;
}

uint32_t scan_stop() {

	uint32_t error_code = 0;
	error_code = sd_ble_gap_scan_stop(m_adapter);

	if (error_code != NRF_SUCCESS) {
		printf("Scan stop failed, code:%d\n", error_code);
		fflush(stdout);
	}
	else {
		printf("Scan stopped\n");
		fflush(stdout);
	}

	return error_code;
}

uint32_t conn_start(addr_t peer_addr)
{
	// cleanup previous data
	connection_cleanup();

	ble_gap_addr_t addr;
	addr.addr_id_peer = 1;
	addr.addr_type = peer_addr.addr_type;
	memcpy_s(addr.addr, BLE_GAP_ADDR_LEN, peer_addr.addr, BLE_GAP_ADDR_LEN);
	memcpy_s(&m_connected_addr, sizeof(addr_t), &peer_addr, sizeof(addr_t));

	m_connection_param.min_conn_interval = MIN_CONNECTION_INTERVAL;
	m_connection_param.max_conn_interval = MAX_CONNECTION_INTERVAL;
	m_connection_param.slave_latency = 10;
	m_connection_param.conn_sup_timeout = CONNECTION_SUPERVISION_TIMEOUT;
	printf("DEBUG: conn start, conn params min=%d max=%d late=%d timeout=%d\n",
		(int)(m_connection_param.min_conn_interval * 1.25),
		(int)(m_connection_param.max_conn_interval * 1.25),
		m_connection_param.slave_latency,
		(int)(m_connection_param.conn_sup_timeout / 100));

	uint32_t err_code;
	err_code = sd_ble_gap_connect(m_adapter,
		&(addr),
		&m_scan_param,
		&m_connection_param
#if NRF_SD_BLE_API >= 5
		, m_config_id
#endif
	);
	if (err_code != NRF_SUCCESS)
	{
		printf("Connection Request Failed, reason %d\n", err_code);
		fflush(stdout);
		return err_code;
	}

	m_connection_is_in_progress = true;

	return err_code;
}

uint32_t auth_start(bool bond, bool keypress, uint8_t io_caps)
{
	uint32_t error_code;

	// try to get security mode before authenticate
	ble_gap_conn_sec_t conn_sec;
	error_code = sd_ble_gap_conn_sec_get(m_adapter, m_connection_handle, &conn_sec);
	printf("DEBUG: get security, return=%d mode=%d level=%d\n", error_code, conn_sec.sec_mode.sm, conn_sec.sec_mode.lv);
	fflush(stdout);

	m_sec_params.bond = bond ? 1 : 0;
	m_sec_params.keypress = keypress ? 1 : 0;
	m_sec_params.io_caps = io_caps;

	// NOTICE: refer to driver, testcase_security.cpp, we'll use the default security params
	error_code = sd_ble_gap_authenticate(m_adapter, m_connection_handle, &m_sec_params);
	// NOTICE: for other devices, check if return NRF_ERROR_NOT_SUPPORTED or NRF_ERROR_NO_MEM?
	//         driver test case uses for passkey auth, refer to testcase_security.cpp
	printf("DEBUG: authenticate return=%d should be %d\n", error_code, NRF_SUCCESS);
	fflush(stdout);

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
	uint32_t   err_code;
	uint16_t   start_handle = 0x01;
	ble_uuid_t srvc_uuid;

	char uuid_string[STRING_BUFFER_SIZE] = { 0 };
	get_uuid_string(uuid, uuid_string);
	printf("Discovering primary service:0x%04X(%s)\n", uuid, uuid_string);
	fflush(stdout);

	srvc_uuid.type = type;
	srvc_uuid.uuid = uuid;

	// Initiate procedure to find the primary BLE_UUID_HEART_RATE_SERVICE.
	err_code = sd_ble_gattc_primary_services_discover(m_adapter,
		m_connection_handle, start_handle,
		&srvc_uuid/*NULL*/);
	if (err_code != NRF_SUCCESS)
	{
		printf("Failed to initiate or continue a GATT Primary Service Discovery procedure\n");
		fflush(stdout);
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
	if (handle_range.start_handle == 0 && handle_range.end_handle == 0) {
		handle_range.start_handle = m_service_start_handle;
		handle_range.end_handle = m_service_end_handle;
	}

	printf("Discovering characteristics, handle range:0x%04X - 0x%04X\n",
		handle_range.start_handle, handle_range.end_handle);
	fflush(stdout);

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
	if (handle_range.start_handle == 0 && handle_range.end_handle == 0) {
		handle_range.start_handle = m_service_start_handle;
		handle_range.end_handle = m_service_end_handle;
	}

	/*if (m_hrm_char_handle == 0)
	{
		printf("No heart rate measurement characteristic handle found\n");
		fflush(stdout);
		return NRF_ERROR_INVALID_STATE;
	}*/

	printf("Discovering descriptors, handle range:0x%04X - 0x%04X\n",
		handle_range.start_handle, handle_range.end_handle);
	fflush(stdout);

	return sd_ble_gattc_descriptors_discover(m_adapter, m_connection_handle, &handle_range);
}

/*
read device name from GAP which handle_value in m_char_list(handle in desc_list)
*/
static uint32_t read_device_name()
{
	uint32_t error_code = 0;
	// use m_device_name_handle or find BLE_UUID_GAP_CHARACTERISTIC_DEVICE_NAME in desc_list
	uint16_t value_handle = m_device_name_handle;
	error_code = sd_ble_gattc_read(
		m_adapter,
		m_connection_handle,
		value_handle, 0
	);
	printf(" Read value from handle:0x%04X code:%d\n", value_handle, error_code);
	fflush(stdout);
	return error_code;
}

/*
set cccd notification to handles in m_char_list(or handle in its desc_list),
writting behavior works with on_write_response() and m_service_start_handle
*/
static uint32_t set_cccd_notification(uint16_t handle)
{
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
			printf(" Write to register CCCD handle:0x%04X code:%d\n", m_char_list[m_char_idx].cccd_handle, error_code);
			fflush(stdout);
			enable_next = true;
			break;
		}
	}

	if (enable_next == false) {
		printf("List of characteristics with report reference data.\n");
		fflush(stdout);
		for (int i = 0; i < m_char_list.size(); i++) {
			if (m_char_list[i].report_ref_is_read) {
				printf(" char:%04X desc:%04X reference data:%02x %02x\n", 
					m_char_list[i].handle, m_char_list[i].report_ref_handle, 
					m_char_list[i].report_ref[0], m_char_list[i].report_ref[1]);
			}
		}
		for (auto &fn : m_callback_fn_list[FN_ON_SERVICE_ENABLED]) {
			((fn_on_service_enabled)fn)();
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
			printf(" Read value from handle:0x%04X code:%d\n", m_char_list[m_char_idx].report_ref_handle, error_code);
			fflush(stdout);
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

/* read all report reference and set CCCD notification */
uint32_t service_enable_start() {

	read_report_refs(0);

	return 0;
}

EXTERNC NRFBLEAPI uint32_t data_read(uint16_t handle, data_t * data)
{
	if (data == 0)
		return NRF_ERROR_INVALID_PARAM;
	if (m_read_data[handle].data_len == 0)
		return NRF_ERROR_NOT_FOUND;
	
	uint32_t error_code = 0;
	error_code = sd_ble_gattc_read(
		m_adapter,
		m_connection_handle,
		handle, 0
	);
	printf(" Read value from handle:0x%04X code:%d\n", handle, error_code);
	fflush(stdout);
	if (error_code != NRF_SUCCESS) {
		return error_code;
	}

	std::unique_lock<std::mutex> lck{ m_mtx };
	auto stat = m_cond.wait_for(lck, std::chrono::milliseconds(2000));
	if (stat == std::cv_status::timeout) {
		return NRF_ERROR_TIMEOUT;
	}

	memcpy_s(data->p_data, data->data_len, m_read_data[handle].p_data, m_read_data[handle].data_len);
	data->data_len = m_read_data[handle].data_len;

	return NRF_SUCCESS;
}

EXTERNC NRFBLEAPI uint32_t data_read_by_report_ref(uint8_t *report_ref, data_t *data)
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
	printf(" Read value handle found: 0x%04X ref:", handle);
	fflush(stdout);
	print_byte_string((char *)(report_ref), 2);
	printf("\n");
	fflush(stdout);
	return data_read(handle, data);
}

EXTERNC NRFBLEAPI uint32_t data_write(uint16_t handle, data_t data)
{
	if (m_write_data[handle].data_len == 0)
		m_write_data[handle].p_data = (uint8_t*)calloc(DATA_BUFFER_SIZE, sizeof(uint8_t));

	memcpy_s(m_write_data[handle].p_data, DATA_BUFFER_SIZE, data.p_data, data.data_len);
	m_write_data[handle].data_len = data.data_len;

	ble_gattc_write_params_t write_params;
	write_params.handle = handle;
	write_params.len = m_write_data[handle].data_len;
	write_params.p_value = m_write_data[handle].p_data;
	write_params.write_op = BLE_GATT_OP_WRITE_REQ;
	write_params.offset = 0;
	uint32_t error_code = 0;
	error_code = sd_ble_gattc_write(m_adapter, m_connection_handle, &write_params);
	printf(" Write value to handle:0x%04X code:%d\n", handle, error_code);
	fflush(stdout);
	if (error_code != NRF_SUCCESS) {
		return error_code;
	}

	std::unique_lock<std::mutex> lck{ m_mtx };
	auto stat = m_cond.wait_for(lck, std::chrono::milliseconds(2000));
	if (stat == std::cv_status::timeout) {
		return NRF_ERROR_TIMEOUT;
	}
	//DEBUG: check write data?
	return NRF_SUCCESS;
}

EXTERNC NRFBLEAPI uint32_t data_write_by_report_ref(uint8_t *report_ref, data_t data)
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
	printf(" Write value handle found: 0x%04X ref:", handle);
	fflush(stdout);
	print_byte_string((char *)(report_ref), 2);
	printf("\n");
	fflush(stdout);
	return data_write(handle, data);
}

/* disconnect action will response status BLE_HCI_LOCAL_HOST_TERMINATED_CONNECTION from BLE_GAP_EVT_DISCONNECTED */
uint32_t dongle_disconnect() 
{
	uint32_t error_code = 0;
	error_code = sd_ble_gap_disconnect(m_adapter, m_connection_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
	printf("DEBUG: disconnect code:%d\n", error_code);
	return error_code;
}

/* reset connectivity dongle
refer to https://infocenter.nordicsemi.com/index.jsp?topic=%2Fps_nrf52840%2Fpower.html&anchor=concept_res_behav
refer to https://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.sdk5.v15.3.0%2Fserialization_codecs.html
*/
uint32_t dongle_reset() {
	auto error_code = sd_rpc_conn_reset(m_adapter, SOFT_RESET);

	if (error_code != NRF_SUCCESS)
	{
		printf("Failed to reset, code: 0x%02X\n", error_code);
		fflush(stdout);
		return error_code;
	}

	printf("Reset\n");
	fflush(stdout);
	return error_code;
}

uint32_t dongle_close() {
	m_callback_fn_list.clear();

	auto error_code = sd_rpc_close(m_adapter);

	if (error_code != NRF_SUCCESS)
	{
		printf("Failed to close nRF BLE Driver. Error code: 0x%02X\n", error_code);
		fflush(stdout);
		return error_code;
	}

	printf("Closed\n");
	fflush(stdout);
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

	// ignore rssi less than -60
	if (p_ble_gap_evt->params.adv_report.rssi < -60)
		return;

	uint64_t addr_num = 0;
	ble_address_to_uint64_convert(p_ble_gap_evt->params.adv_report.peer_addr, &addr_num);

	// set flag if new arrival, and list always up-to-date
	bool new_arrival = (m_adv_list.find(addr_num) == m_adv_list.end());
	m_adv_list.insert_or_assign(addr_num, p_ble_gap_evt->params.adv_report);

	if (new_arrival)
	{
		// Log the Bluetooth device address of advertisement packet received.
		ble_address_to_string_convert(p_ble_gap_evt->params.adv_report.peer_addr, str);

		char name[256] = { 0 };
		get_adv_name(&p_ble_gap_evt->params.adv_report, name);
		printf("Received adv report address: 0x%s rssi:%d rsp:%d name:%s\n",
			str, p_ble_gap_evt->params.adv_report.rssi, p_ble_gap_evt->params.adv_report.scan_rsp, name);
		fflush(stdout);

		if (m_connection_is_in_progress) {
			printf("Connection has been started, ignore rest of discovered devices\n");
			fflush(stdout);
			return;
		}

		// invoke callback to caller with discovered report
		if (m_callback_fn_list[FN_ON_DISCOVERED].size() > 0) {
			std::string name_str = std::string(name);
			std::string addr_str = std::string((char*)str);
			addr_t report;
			report.rssi = p_ble_gap_evt->params.adv_report.rssi;
			report.addr_type = p_ble_gap_evt->params.adv_report.peer_addr.addr_type;
			memcpy_s(report.addr, BLE_GAP_ADDR_LEN, p_ble_gap_evt->params.adv_report.peer_addr.addr, BLE_GAP_ADDR_LEN);
			for (auto &fn : m_callback_fn_list[FN_ON_DISCOVERED]) {
				((fn_on_discovered)fn)(addr_str, name_str, report);
			}
		}
	}

#if NRF_SD_BLE_API >= 6
	else {
		err_code = sd_ble_gap_scan_start(m_adapter, NULL, &m_adv_report_buffer);

		if (err_code != NRF_SUCCESS)
		{
			printf("Scan start failed with error code: %d\n", err_code);
			fflush(stdout);
		}
		else
		{
			printf("Scan started\n");
			fflush(stdout);
		}
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
}

/**@brief Function called on BLE_GAP_EVT_CONNECTED event.
 *
 * @details Update connection state and proceed to discovering the peer's GATT services.
 *
 * @param[in] p_ble_gap_evt GAP event.
 */
static void on_connected(const ble_gap_evt_t * const p_ble_gap_evt)
{
	printf("Connection established\n");
	fflush(stdout);

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

	for (auto &fn : m_callback_fn_list[FN_ON_CONNECTED]) {
		((fn_on_connected)fn)(m_connected_addr);
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
	printf("Disconnected, reason: 0x%02X\n",
		p_ble_gap_evt->params.disconnected.reason);
	fflush(stdout);
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
		printf("Service discovery failed. Error code 0x%X\n", p_ble_gattc_evt->gatt_status);
		fflush(stdout);
		return;
	}

	count = p_ble_gattc_evt->params.prim_srvc_disc_rsp.count;
	printf("Received service discovery response, service count:%d\n", count);
	fflush(stdout);

	if (count == 0)
	{
		printf("Service not found\n");
		fflush(stdout);
		return;
	}

	/* currently service_discovery_start only support 1 service discover in single call */
	service_index = 0;
	service = &(p_ble_gattc_evt->params.prim_srvc_disc_rsp.services[service_index]);

	char uuid_string[STRING_BUFFER_SIZE] = { 0 };
	get_uuid_string(service->uuid.uuid, uuid_string);
	printf("Service discovered UUID: 0x%04X(%s), handle range:0x%04X - 0x%04X\n", service->uuid.uuid, uuid_string,
		service->handle_range.start_handle, service->handle_range.end_handle);
	fflush(stdout);

	m_service_start_handle = service->handle_range.start_handle;
	m_service_end_handle = service->handle_range.end_handle;

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
		printf(" Characteristic discovery failed or empty, code 0x%X count=%d\n",
			p_ble_gattc_evt->gatt_status, count);
		fflush(stdout);
		// invoke callback to caller when serviec discovery terminated
		for (auto &fn : m_callback_fn_list[FN_ON_SERVICE_DISCOVERED]) {
			((fn_on_service_discovered)fn)(m_service_start_handle);
		}
		return;
	}

	printf(" Received characteristic discovery response, characteristics count: %d\n", count);
	fflush(stdout);

	char uuid_string[STRING_BUFFER_SIZE] = { 0 };
	for (int i = 0; i < count; i++)
	{
		memset(uuid_string, 0, sizeof(uuid_string));
		get_uuid_string(p_ble_gattc_evt->params.char_disc_rsp.chars[i].uuid.uuid, uuid_string);
		printf(" Characteristic handle:0x%04X, UUID: 0x%04X(%s) decl:0x%04X prop(LSB):0x%x\n",
			p_ble_gattc_evt->params.char_disc_rsp.chars[i].handle_value,
			p_ble_gattc_evt->params.char_disc_rsp.chars[i].uuid.uuid,
			uuid_string,
			p_ble_gattc_evt->params.char_disc_rsp.chars[i].handle_decl,
			p_ble_gattc_evt->params.char_disc_rsp.chars[i].char_props);
		fflush(stdout);

		// store characteristic to list
		if (m_char_list.size() > 0) {
			m_char_list[m_char_list.size() - 1].handle_range.end_handle = 
				p_ble_gattc_evt->params.char_disc_rsp.chars[i].handle_decl - 1;
		}
		dev_char_t dev_char;
		// NOTICE: only care the value handle for further usage
		dev_char.handle = p_ble_gattc_evt->params.char_disc_rsp.chars[i].handle_value;
		dev_char.uuid = p_ble_gattc_evt->params.char_disc_rsp.chars[i].uuid.uuid;
		dev_char.handle_range.start_handle = p_ble_gattc_evt->params.char_disc_rsp.chars[i].handle_decl;
		dev_char.handle_range.end_handle = m_service_end_handle;
		m_char_list.push_back(dev_char);

		// pre-allocate for read data, reduce effort on_hvx
		auto handle_value = p_ble_gattc_evt->params.char_disc_rsp.chars[i].handle_value;
		if (m_read_data[handle_value].p_data == nullptr)
			m_read_data[handle_value].p_data = (uint8_t*)calloc(DATA_BUFFER_SIZE, sizeof(uint8_t));
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
		printf(" Descriptor discovery failed or empty, code 0x%X count=%d\n", p_ble_gattc_evt->gatt_status, count);
		fflush(stdout);
		// invoke callback to caller when serviec discovery terminated
		for (auto &fn : m_callback_fn_list[FN_ON_SERVICE_DISCOVERED]) {
			((fn_on_service_discovered)fn)(m_service_start_handle);
		}
		return;
	}

	printf(" Received descriptor discovery response, descriptor count: %d\n", count);
	fflush(stdout);

	uint16_t last_handle = 0;
	char uuid_string[STRING_BUFFER_SIZE] = { 0 };
	for (int i = 0; i < count; i++)
	{
		memset(uuid_string, 0, sizeof(uuid_string));
		get_uuid_string(p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid, uuid_string);
		printf(" Descriptor handle: 0x%04X, UUID: 0x%04X(%s)\n",
			p_ble_gattc_evt->params.desc_disc_rsp.descs[i].handle,
			p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid,
			uuid_string);
		fflush(stdout);

		// store descriptor to list
		ble_gattc_desc_t dev_desc = p_ble_gattc_evt->params.desc_disc_rsp.descs[i];
		m_char_list[m_char_idx].desc_list.push_back(dev_desc);

		//DEBUG: set cccd handle, refer to set_cccd_notification();
		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_CCCD)
		{
			m_char_list[m_char_idx].cccd_handle = dev_desc.handle;
			printf("DEBUG: CCCD descriptor saved, handle=%x\n", dev_desc.handle);
			fflush(stdout);
		}
		//DEBUG: set report reference handle, refer to read_report_refs()
		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_REPORT_REF_DESCR)
		{
			m_char_list[m_char_idx].report_ref_handle = dev_desc.handle;
			printf("DEBUG: Report reference descriptor saved, handle=%x\n", dev_desc.handle);
			fflush(stdout);
		}

		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_BATTERY_LEVEL_CHAR)
		{
			//BLE_GATT_STATUS_ATTERR_INSUF_AUTHENTICATION
			// Authentication required, auth_start()
			//BLE_GATT_STATUS_ATTERR_WRITE_NOT_PERMITTED
			// Cannot write hvx enabling notification messages
			m_battery_level_handle = p_ble_gattc_evt->params.desc_disc_rsp.descs[i].handle;
			printf("DEBUG: Battery level handle saved, handle=%x\n", m_battery_level_handle);
			fflush(stdout);
		}

		if (p_ble_gattc_evt->params.desc_disc_rsp.descs[i].uuid.uuid == BLE_UUID_GAP_CHARACTERISTIC_DEVICE_NAME)
		{
			m_device_name_handle = p_ble_gattc_evt->params.desc_disc_rsp.descs[i].handle;
			printf("DEBUG: Device name handle saved\n");
			fflush(stdout);
		}

		m_service_start_handle = std::max(m_service_start_handle, p_ble_gattc_evt->params.desc_disc_rsp.descs[i].handle);
	}

	if (++m_char_idx < m_char_list.size()) {
		// move next characteristic
		descr_discovery_start(m_char_list[m_char_idx].handle_range);
	}
	else if (last_handle < m_service_end_handle) {
		ble_gattc_handle_range_t range{ m_service_start_handle, m_service_end_handle };
		char_discovery_start(range);
	}
	else {
		// invoke callback to caller when all characteristics discovered
		for (auto &fn : m_callback_fn_list[FN_ON_SERVICE_DISCOVERED]) {
			((fn_on_service_discovered)fn)(m_service_start_handle);
		}
	}

}


static void on_read_characteristic_value_by_uuid_response(const ble_gattc_evt_t *const p_ble_gattc_evt)
{
	if (p_ble_gattc_evt->gatt_status != NRF_SUCCESS)
	{
		printf("Error read char val by uuid operation, error code 0x%x\n", p_ble_gattc_evt->gatt_status);
		fflush(stdout);
		return;
	}

	if (p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.count == 0 ||
		p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.value_len == 0)
	{
		printf("Error read char val by uuid operation, no handle count or value length\n");
		fflush(stdout);
		return;
	}

	for (int i = 0; i < p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.count; i++)
	{
		printf("Received read char by uuid, value handle:0x%04X len:%d.\n",
			p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.handle_value[i],
			p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.value_len);
		fflush(stdout);
	}

	//DEBUG: directly use handle from descriptor?
	uint32_t error_code = sd_ble_gattc_read(
		m_adapter,
		m_connection_handle,
		p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.handle_value[0],
		0
	);
	printf(" DEBUG: read from handle0:0x%04X\n", p_ble_gattc_evt->params.char_val_by_uuid_read_rsp.handle_value[0]);
	fflush(stdout);
}

static void on_read_characteristic_values_response(const ble_gattc_evt_t *const p_ble_gattc_evt)
{
	if (p_ble_gattc_evt->gatt_status != NRF_SUCCESS)
	{
		printf("Error read char vals operation, error code 0x%x\n", p_ble_gattc_evt->gatt_status);
		fflush(stdout);
		return;
	}

	if (p_ble_gattc_evt->params.char_vals_read_rsp.len == 0)
	{
		printf("Error read char vals operation, no att values length\n");
		fflush(stdout);
		return;
	}

	auto len = p_ble_gattc_evt->params.char_vals_read_rsp.len;
	uint8_t *p_data = (uint8_t *)p_ble_gattc_evt->params.char_vals_read_rsp.values;

	char read_bytes[DATA_BUFFER_SIZE] = { 0 };
	memcpy_s(&read_bytes[0], DATA_BUFFER_SIZE, p_data, len);
	printf("Received read char vals len:%d data: ", len);
	print_byte_string(read_bytes, len);
	printf("\n");
	fflush(stdout);
}

static void on_read_response(const ble_gattc_evt_t *const p_ble_gattc_evt)
{
	auto rsp_handle = p_ble_gattc_evt->params.read_rsp.handle;

	printf("Received read response handle:0x%04X ", rsp_handle);
	fflush(stdout);

	if (p_ble_gattc_evt->gatt_status != NRF_SUCCESS || 
		p_ble_gattc_evt->params.read_rsp.len == 0)
	{
		// refer to BLE_GATT_STATUS_ATTERR_INSUF_AUTHENTICATION if handle access required authentication
		// refer to BLE_GATT_STATUS_ATTERR_REQUEST_NOT_SUPPORTED if handle property not permitted
		printf("Read operation failed or data empty, error code 0x%x\n", p_ble_gattc_evt->gatt_status);
		fflush(stdout);
		//TODO: do something next if any error occurred

		m_cond.notify_all();
		return;
	}

	uint8_t* p_data = (uint8_t *)p_ble_gattc_evt->params.read_rsp.data;
	uint16_t offset = p_ble_gattc_evt->params.read_rsp.offset;
	uint16_t len = p_ble_gattc_evt->params.read_rsp.len;

	char read_bytes[DATA_BUFFER_SIZE] = { 0 };
	memcpy_s(&read_bytes[0], DATA_BUFFER_SIZE, p_data + offset, len);
	printf("len:%d data: ", len);
	print_byte_string(read_bytes, len);
	printf("\n");
	fflush(stdout);

	// NOTICE: refer to on_characteristic_discovery_response has pre-allocated memory
	if (m_read_data[rsp_handle].p_data == nullptr)
		m_read_data[rsp_handle].p_data = (uint8_t*)calloc(DATA_BUFFER_SIZE, sizeof(uint8_t));
	memcpy_s(m_read_data[rsp_handle].p_data, DATA_BUFFER_SIZE, p_data, len);
	m_read_data[rsp_handle].data_len = len;
	// release lock for data_read()
	m_cond.notify_all();

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

	printf("Received write response from handle:0x%04X.\n", rsp_handle);
	fflush(stdout);

	if (p_ble_gattc_evt->gatt_status != NRF_SUCCESS)
	{
		printf("Error. Write operation failed. Error code 0x%X\n", p_ble_gattc_evt->gatt_status);
		fflush(stdout);
		m_cond.notify_all();
		return;
	}

	uint8_t* p_data = (uint8_t *)p_ble_gattc_evt->params.write_rsp.data;
	uint16_t offset = p_ble_gattc_evt->params.write_rsp.offset;
	uint16_t len = p_ble_gattc_evt->params.write_rsp.len;

	// NOTICE: refer to on_characteristic_discovery_response has pre-allocated memory
	if (m_write_data[rsp_handle].p_data == nullptr)
		m_write_data[rsp_handle].p_data = (uint8_t*)calloc(DATA_BUFFER_SIZE, sizeof(uint8_t));
	memcpy_s(m_write_data[rsp_handle].p_data, DATA_BUFFER_SIZE, p_data + offset, len);
	m_write_data[rsp_handle].data_len = len;
	// release lock for data_write()
	m_cond.notify_all();

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
		printf("Received hvx from handle:0x%04X not in list ", hvx_handle);
		fflush(stdout);
	}
	else {
		char uuid_string[STRING_BUFFER_SIZE] = { 0 };
		get_uuid_string(m_char_list[char_idx].uuid, uuid_string);
		printf("Received hvx from handle:0x%04X uuid:0x%04X(%s) ",
			hvx_handle, m_char_list[char_idx].uuid, uuid_string);
		fflush(stdout);
		if (m_char_list[char_idx].report_ref_is_read) {
			printf("ref:");
			print_byte_string((char *)(m_char_list[char_idx].report_ref), 2);
			fflush(stdout);
		}
	}

	if (len == 0) {
		printf("no data present.\n");
		fflush(stdout);
		return;
	}

	printf("len:%d data: ", len);
	print_byte_string((char*)p_data, len);
	printf("\n");
	fflush(stdout);

	// NOTICE: refer to on_characteristic_discovery_response has pre-allocated memory
	if (m_read_data[hvx_handle].p_data == nullptr)
		m_read_data[hvx_handle].p_data = (uint8_t*)calloc(DATA_BUFFER_SIZE, sizeof(uint8_t));
	memcpy_s(m_read_data[hvx_handle].p_data, DATA_BUFFER_SIZE, p_data, len);
	m_read_data[hvx_handle].data_len = len;

	for (auto &fn : m_callback_fn_list[FN_ON_DATA_RECEIVED]) {
		((fn_on_data_received)fn)(hvx_handle, m_read_data[hvx_handle]);
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
	printf("DEBUG: connection update request code=%d min=%d max=%d late=%d timeout=%d\n",
		err_code,
		(int)(conn_params.min_conn_interval * 1.25),
		(int)(conn_params.max_conn_interval * 1.25),
		conn_params.slave_latency,
		(int)(conn_params.conn_sup_timeout / 100));

	if (err_code != NRF_SUCCESS)
	{
		printf("Conn params update failed, err_code %d\n", err_code);
		fflush(stdout);
	}
}

/*
connection parameters responsed from peripheral
*/
static void on_conn_params_update(const ble_gap_evt_t * const p_ble_gap_evt)
{
	auto conn_params = &(p_ble_gap_evt->params.conn_param_update.conn_params);

	printf("Connection parameters updated. New parameters:\n");
	printf("Connection interval : %d [ms]\n", (int)(conn_params->min_conn_interval * 1.25));
	printf("Slave latency : %d\n", conn_params->slave_latency);
	printf("Supervision timeout : %d [s]\n", (int)(conn_params->conn_sup_timeout / 100));
	fflush(stdout);

	uint32_t error_code;
	ble_gap_conn_sec_t conn_sec;
	error_code = sd_ble_gap_conn_sec_get(m_adapter, m_connection_handle, &conn_sec);
	printf("DEBUG: get security code=%d mode=%d level=%d\n", error_code, conn_sec.sec_mode.sm, conn_sec.sec_mode.lv);
}

/*
request security parameters for peripheral authentication
*/
static void on_sec_params_request(const ble_gap_evt_t * const p_ble_gap_evt)
{
	auto peer_params = p_ble_gap_evt->params.sec_params_request.peer_params;

	printf("DEBUG: on security params request, peer: bond=%d io=%d min=%d max=%d ownenc=%d peerenc=%d\n",
		peer_params.bond, peer_params.io_caps,
		peer_params.min_key_size, peer_params.max_key_size,
		peer_params.kdist_own.enc, peer_params.kdist_peer.enc);

	memcpy_s(m_own_pk.pk, BLE_GAP_LESC_P256_PK_LEN, m_public_key, ECC_P256_PK_LEN);

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
	printf("DEBUG: on security params request, return=%d should be %d\n", err_code, NRF_SUCCESS);
	fflush(stdout);
}


#pragma endregion


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
		printf("Received an empty BLE event\n");
		fflush(stdout);
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

	case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
		on_sec_params_request(&(p_ble_evt->evt.gap_evt));
		break;

	case BLE_GAP_EVT_CONN_SEC_UPDATE:
		printf("DEBUG: on conn security updated, mode=%d level=%d\n",
			p_ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.sm,
			p_ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.lv);
		fflush(stdout);
		break;

	case BLE_GAP_EVT_AUTH_STATUS:
	{
		printf("DEBUG: on auth status, status=%d, bond=%d\n",
			p_ble_evt->evt.gap_evt.params.auth_status.auth_status,
			p_ble_evt->evt.gap_evt.params.auth_status.bonded);
		fflush(stdout);
		if (p_ble_evt->evt.gap_evt.params.auth_status.auth_status == BLE_GAP_SEC_STATUS_SUCCESS &&
			p_ble_evt->evt.gap_evt.params.auth_status.bonded == 1) {
			
			m_is_authenticated = true;

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
					((fn_on_failed)fn)(str);
				}
			}
		}
	}break;

	case BLE_GAP_EVT_AUTH_KEY_REQUEST:
	{
		uint8_t key_type = p_ble_evt->evt.gap_evt.params.auth_key_request.key_type;
		uint8_t *key = NULL;
		// provide fixed passkey
		if (key_type == BLE_GAP_AUTH_KEY_TYPE_PASSKEY) {
			uint8_t passkey[6] = { '1','2','3','4','5','6' };
			key = &passkey[0];
			printf("DEBUG: generate 123456 for passkey\n");
			fflush(stdout);
		}
		else if (key_type == BLE_GAP_AUTH_KEY_TYPE_OOB) {
			//TODO: not implemented
			printf("DEBUG: on auth key req by OOB not implemented\n");
			fflush(stdout);
			break;
		}
		err_code = sd_ble_gap_auth_key_reply(m_adapter, m_connection_handle, key_type, key);
		printf("DEBUG: on auth key req, keytype:%d return:%d\n", key_type, err_code);
		fflush(stdout);

		if (m_callback_fn_list[FN_ON_PASSKEY_REQUIRED].size() > 0) {
			std::string str = std::string((char*)key, 6);
			for (auto &fn : m_callback_fn_list[FN_ON_PASSKEY_REQUIRED]) {
				((fn_on_passkey_required)fn)(str);
			}
		}
	}break;

	case BLE_GAP_EVT_PASSKEY_DISPLAY:
	{
		//DEBUG: asume only works in passkey type, w/o verified
		uint8_t key[6] = { 0 };
		memcpy_s(key, 6, p_ble_evt->evt.gap_evt.params.passkey_display.passkey, 6);
		err_code = sd_ble_gap_auth_key_reply(m_adapter, m_connection_handle, BLE_GAP_AUTH_KEY_TYPE_PASSKEY, key);
		printf("DEBUG: on passkey display, key: ");
		for (int i = 0; i < 6; i++)
			printf("%c ", key[i]);
		printf("return:%d\n", err_code);
		fflush(stdout);

		if (m_callback_fn_list[FN_ON_PASSKEY_REQUIRED].size() > 0) {
			std::string str = std::string((char*)key, 6);
			for (auto &fn : m_callback_fn_list[FN_ON_PASSKEY_REQUIRED]) {
				((fn_on_passkey_required)fn)(str);
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
				((fn_on_failed)fn)(str);
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
		//on_exchange_mtu_request(&(p_ble_evt->evt.gatts_evt));
		break;

	case BLE_GATTC_EVT_EXCHANGE_MTU_RSP:
		//on_exchange_mtu_response(&(p_ble_evt->evt.gattc_evt));
		break;
#endif

#if NRF_SD_BLE_API >= 5

	case BLE_GATTC_EVT_WRITE_CMD_TX_COMPLETE:
		break;

	case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
		printf("Maximum radio packet length updated to %d bytes.\n",
			p_ble_evt->evt.gap_evt.params.data_length_update.effective_params.max_tx_octets);
		break;

	case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
		sd_ble_gap_data_length_update(m_adapter, m_connection_handle, NULL, NULL);
		break;

	case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
	{
		printf("PHY update request.\n");
		ble_gap_phys_t const phys =
		{
			BLE_GAP_PHY_AUTO, /*tx_phys*/
			BLE_GAP_PHY_AUTO, /*rx_phys*/
		};
		err_code = sd_ble_gap_phy_update(m_adapter, m_connection_handle, &phys);
		if (err_code != NRF_SUCCESS)
		{
			printf("PHY update request reply failed, err_code %d\n", err_code);
		}
		fflush(stdout);
	} break;

#endif

	default:
		printf("Received an un-handled event with ID: %d\n", p_ble_evt->header.evt_id);
		fflush(stdout);
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
		printf("BLE stack already enabled\n");
		fflush(stdout);
		break;
	default:
		printf("Failed to enable BLE stack. Error code: %d\n", err_code);
		fflush(stdout);
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
	ble_cfg.gap_cfg.role_count_cfg.central_sec_count = 1; /*NOTICE: set for sd_ble_gap_authenticate*/

	error_code = sd_ble_cfg_set(m_adapter, BLE_GAP_CFG_ROLE_COUNT, &ble_cfg, ram_start);
	if (error_code != NRF_SUCCESS)
	{
		printf("sd_ble_cfg_set() failed when attempting to set BLE_GAP_CFG_ROLE_COUNT. Error code: 0x%02X\n", error_code);
		fflush(stdout);
		return error_code;
	}

	memset(&ble_cfg, 0x00, sizeof(ble_cfg));
	ble_cfg.conn_cfg.conn_cfg_tag = conn_cfg_tag;
	ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu = 150;

	error_code = sd_ble_cfg_set(m_adapter, BLE_CONN_CFG_GATT, &ble_cfg, ram_start);
	if (error_code != NRF_SUCCESS)
	{
		printf("sd_ble_cfg_set() failed when attempting to set BLE_CONN_CFG_GATT. Error code: 0x%02X\n", error_code);
		fflush(stdout);
		return error_code;
	}

	// GAP config code block from nordic_uart_client example
#if NRF_SD_BLE_API >= 5
#define NRF_SDH_BLE_GAP_EVENT_LENGTH 320
#define NRF_SDH_BLE_GATT_MAX_MTU_SIZE 247
	memset(&ble_cfg, 0, sizeof(ble_cfg));
	ble_cfg.conn_cfg.conn_cfg_tag = conn_cfg_tag;
	ble_cfg.conn_cfg.params.gap_conn_cfg.conn_count = 1;
	ble_cfg.conn_cfg.params.gap_conn_cfg.event_length = NRF_SDH_BLE_GAP_EVENT_LENGTH;
	error_code = sd_ble_cfg_set(m_adapter, BLE_CONN_CFG_GAP, &ble_cfg, ram_start);
	if (error_code != NRF_SUCCESS)
	{
		printf("sd_ble_cfg_set() failed when attempting to set BLE_CONN_CFG_GAP. Error code: 0x%02X\n", error_code);
		fflush(stdout);
		return error_code;
	}
#endif

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
uint32_t dongle_init(char* serial_port, uint32_t baud_rate) {
	// init ecc and generate keypair for later usage?
	ecc_init();
	ecc_p256_gen_keypair(m_private_key, m_public_key);
	uint8_t test_pubkey[ECC_P256_PK_LEN] = { 0 };
	ecc_p256_compute_pubkey(m_private_key, test_pubkey);
	// ASSERT: test_pubkey should be the same with m_public_key

	uint32_t error_code;
	uint8_t  cccd_value = 0;
	
	printf("Serial port used: %s\n", serial_port);
	printf("Baud rate used: %d\n", baud_rate);
	fflush(stdout);

	m_adapter = adapter_init(serial_port, baud_rate);
	sd_rpc_log_handler_severity_filter_set(m_adapter, SD_RPC_LOG_INFO);
	error_code = sd_rpc_open(m_adapter, status_handler, ble_evt_dispatch, log_handler);

	if (error_code != NRF_SUCCESS)
	{
		printf("Failed to open nRF BLE Driver. Error code: 0x%02X\n", error_code);
		fflush(stdout);
		return error_code;
	}

#if NRF_SD_BLE_API >= 5
	error_code = ble_cfg_set(m_config_id);
#endif

	error_code = ble_stack_init();

	if (error_code != NRF_SUCCESS)
	{
		printf("Failed to init BLE stack. Error code: 0x%02X\n", error_code);
		fflush(stdout);
		return error_code;
	}

#if NRF_SD_BLE_API < 5
	error_code = ble_options_set();

	if (error_code != NRF_SUCCESS)
	{
		return error_code;
	}
#endif

	ble_version_t ver = { 0 };
	error_code = sd_ble_version_get(m_adapter, &ver);
	if (error_code != NRF_SUCCESS)
	{
		printf("Failed to get connectivity FW versions. Error code: 0x%02X\n", error_code);
		fflush(stdout);
		return error_code;
	}
	else {
		printf("Version: %d, Company: %d, SubVersion: %d\n", ver.version_number, ver.company_id, ver.subversion_number);
		fflush(stdout);
	}

	return error_code;
}
