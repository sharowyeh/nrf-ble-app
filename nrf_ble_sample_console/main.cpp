#include <string>
#include <iostream>
#include <vector>
#include <condition_variable>
#include <mutex>

#include "dongle.h"

bool discovered = false;
// argv[3]
std::string target_addr = "0";
// argv[4], default no limitation(any dBm greater than -128)
int8_t target_rssi = INT8_MIN;
bool connected = false;
bool authenticated = false;

void on_dev_discovered(const char *addr_str, const char *name, uint8_t addr_type, uint8_t *addr, int8_t rssi)
{
	printf("[main] discovered address %s and rssi %d\n", addr_str, rssi);

	if (discovered)
		return;
	if (rssi < target_rssi)
		return;
	if (target_addr.length() < 12 || std::string(addr_str).compare(target_addr) == 0)
	{
		scan_stop();
		discovered = true;
		printf("[main] requirement match, connect start\n");
		conn_start(addr_type, addr);
	}
}

void on_dev_connected(uint8_t addr_type, uint8_t *addr)
{
	connected = true;
	printf("[main] auth start\n");
	// NOTICE: service discovery should wait before param updated event or bond for auth secure param(or passkey)
	auth_start(true, false, 0x2);
	// than
	//service_discovery_start();
}

void on_dev_passkey_required(const char *passkey)
{
	printf("[main] show passkey %s\n", passkey);
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

void on_dev_authenticated(uint8_t status)
{
	if (status != 0) {
		printf("[main] auth error code:%d\n", status);
		return;
	}
	authenticated = true;
	service_idx = 0;
	printf("[main] service discovery 0 start\n");
	service_discovery_start(service_list[0].uuid, service_list[0].type);
}

void on_dev_service_discovered(uint16_t last_handle, uint16_t count)
{
	if (++service_idx < service_list.size()) {
		printf("[main] service discovery %d start\n", service_idx);
		service_discovery_start(service_list[service_idx].uuid, service_list[service_idx].type);
	}
	else {
		// read report ref and enable cccd notification
		printf("[main] service enable start for total %d characteristics\n", count);
		service_enable_start();
	}
}

void on_dev_service_enabled(uint16_t count)
{
	printf("[main] service enabled with %d descriptors\n", count);
	fflush(stdout);
	printf("[main] === ready to inteact via report characteristic(use hex) ===\n");
	printf("[main] input \"write xx xx yy yy yy..\" write data to target, which xx is report reference, yy is payload data\n");
	printf("[main] input \"read xx xx\" read data from target, which xx is report reference data\n");
	fflush(stdout);

	uint16_t *handle_list = (uint16_t*)calloc(count, sizeof(uint16_t));
	uint8_t *refs_list = (uint8_t*)calloc(count * 2, sizeof(uint8_t));
	uint16_t len = count;
	report_char_list(handle_list, refs_list, &len);
	for (int i = 0; i < len; i++) {
		printf("[main] report char handle:%04X refs:%02x %02x\n", handle_list[i], refs_list[i * 2], refs_list[i * 2 + 1]);
	}
	fflush(stdout);
}

std::mutex mtx;
std::condition_variable cond;

uint32_t on_dev_disconnected(uint8_t reason) {
	// 0x16: close by user terminate, 0x8: close by stack timeout(power off)
	printf("[main] disconnected reason:%x\n", reason);
	discovered = false;
	connected = false;
	cond.notify_all();
	return reason;
}

void on_dev_failed(const char *stage)
{
	printf("[main] on failed %s\n", stage);
}

void on_dev_data_received(uint16_t handle, uint8_t *data, uint16_t len)
{
	printf("[main] data received 0x%X ", handle);
	for (int i = 0; i < len; i++) {
		printf("%02x ", (unsigned char)data[i]);
	}
	printf("\n");
}

void on_dev_data_sent(uint16_t handle, uint8_t *data, uint16_t len)
{
	printf("[main] data sent 0x%X", handle);
}

std::vector<std::string> split(std::string str, std::string delimiter) {
	std::vector<std::string> result;
	size_t pos = 0;
	while (pos <= str.size()) {
		auto token = str.substr(pos, str.find(delimiter, pos) - pos);
		result.push_back(token);
		pos += token.length() + delimiter.length();
	}
	return result;
}

void parse_write_command(std::vector<std::string> split_cmd) {
	auto data_len = split_cmd.size() - 3;
	if (data_len <= 0) {
		return;
	}
	uint8_t ref[2] = { 0 };
	ref[0] = strtoul(split_cmd[1].c_str(), nullptr, 16);
	ref[1] = strtoul(split_cmd[2].c_str(), nullptr, 16);
	uint8_t data[DATA_BUFFER_SIZE] = { 0 };
	for (int i = 0; i < data_len; i++) {
		data[i] = strtoul(split_cmd[i + 3].c_str(), nullptr, 16);
	}

	uint32_t err = data_write_by_report_ref(ref, data, data_len);
	printf("[main] write data, code:%d\n", err);
	fflush(stdout);
}

void parse_read_command(std::vector<std::string> split_cmd) {
	if (split_cmd.size() < 3) {
		return;
	}
	uint8_t ref[2] = { 0 };
	ref[0] = strtoul(split_cmd[1].c_str(), nullptr, 16);
	ref[1] = strtoul(split_cmd[2].c_str(), nullptr, 16);
	uint8_t data[DATA_BUFFER_SIZE] = { 0 };
	uint16_t data_len = DATA_BUFFER_SIZE;

	uint32_t err = data_read_by_report_ref(ref, &data[0], &data_len);
	printf("[main] read data, code:%d data:", err);
	for (int i = 0; i < data_len; i++) {
		printf("%02x ", data[i]);
	}
	printf("\n");
}

void print_usage() {
	printf("[main] supported input command:\n" \
		"    help        :Show this message\n" \
		"    scan        :Start to advertising devices\n" \
		"    disconnect  :Disconnect current connected device\n" \
		"    write xx xx yy yy  :Write data to report characteristic by report reference\n" \
		"    read xx xx         :Read data from report characteristic by report reference\n" \
		"    q(or Q)     :Quit app\n");
}

void parse_input_command(std::string &line) {
	auto sp = split(line, " ");
	if (sp[0].compare("write") == 0) {
		parse_write_command(sp);
	}
	else if (sp[0].compare("read") == 0) {
		parse_read_command(sp);
	}
	else if (sp[0].compare("disconnect") == 0) {
		dongle_disconnect();
	}
	else if (sp[0].compare("scan") == 0) {
		scan_start(200, 50, true, 0);
	}
	else if (sp[0].compare("help") == 0) {
		print_usage();
	}
}

int main(int argc, char * argv[])
{	
	callback_add(FN_ON_DISCOVERED, &on_dev_discovered);
	callback_add(FN_ON_CONNECTED, &on_dev_connected);
	callback_add(FN_ON_PASSKEY_REQUIRED, &on_dev_passkey_required);
	callback_add(FN_ON_AUTHENTICATED, &on_dev_authenticated);
	callback_add(FN_ON_SERVICE_DISCOVERED, &on_dev_service_discovered);
	callback_add(FN_ON_SERVICE_ENABLED, &on_dev_service_enabled);
	callback_add(FN_ON_DISCONNECTED, &on_dev_disconnected);
	callback_add(FN_ON_FAILED, &on_dev_failed);
	callback_add(FN_ON_DATA_RECEIVED, &on_dev_data_received);

	uint32_t error_code;
	char     serial_port[10] = "COM3";
	uint32_t baud_rate = 1000000;

	// given serial port string
	if (argc > 1)
		strcpy_s(serial_port, argv[1]);
	// given baud rate
	if (argc > 2)
		baud_rate = atoi(argv[2]);
	// given address hex string(upper case) for peripheral connection
	if (argc > 3)
		target_addr = std::string(argv[3]);
	// given rssi limitation for peripheral connection
	if (argc > 4)
		target_rssi = atoi(argv[4]);


	error_code = dongle_init(serial_port, baud_rate);

	error_code = scan_start(200, 50, true, 0);

	if (error_code != 0)
	{
		return error_code;
	}

	// Endlessly loop.
	for (;;)
	{
		std::string line;
		std::getline(std::cin, line);
		//char c = (char)getchar();
		//if (c == 'q' || c == 'Q')
		if (line.compare("q") == 0 || line.compare("Q") == 0)
		{
			scan_stop();

			uint32_t err = 0;
			if (connected) {
				err = dongle_disconnect();
				// if success, wait until dongle_disconnect() finished;
				if (err = 0) {
					std::unique_lock<std::mutex> lck{ mtx };
					cond.wait(lck);
				}
			}
			
			dongle_reset();

			return 0;
		}

		parse_input_command(line);
	}
}

