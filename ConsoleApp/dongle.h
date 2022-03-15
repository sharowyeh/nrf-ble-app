#pragma once
#include "ble.h"
#include "sd_rpc.h"

#include <string>

typedef uint32_t(*fn_on_discovered)(ble_gap_evt_adv_report_t &, std::string &, std::string &);
typedef uint32_t(*fn_on_connected)(ble_gap_evt_connected_t &);
typedef uint32_t(*fn_on_disconnected)(ble_gap_evt_disconnected_t &);
typedef uint32_t(*fn_on_authenticated)(ble_gap_evt_auth_status_t &);
typedef uint32_t(*fn_on_srvc_discovered)(uint16_t &last_handle);

uint32_t set_callback(std::string fn_name, void* fn);

uint32_t dongle_init(char*, uint32_t);

uint32_t scan_start(ble_gap_scan_params_t);
uint32_t conn_start(ble_gap_addr_t peer_addr);
uint32_t bond_start(ble_gap_sec_params_t);
uint32_t service_discovery_start(uint16_t uuid, uint8_t type);
uint32_t enable_service_start();
uint32_t dongle_disconnect();
uint32_t dongle_close();