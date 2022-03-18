#include <string>
#include <vector>
#include <condition_variable>
#include <mutex>

#include "dongle.h"

bool discovered = false;

void on_dev_discovered(std::string addr_str, std::string name, addr_t addr)
{
	printf("[main] discovered address %s\n", addr_str.c_str());
	if (addr_str.compare("112233445566") == 0 ||
		addr_str.compare("ED156EBFA2CB") == 0 ||
		addr_str.compare("D79214092F0D") == 0 ||
		addr_str.compare("EE45D8C454B4") == 0)
	{
		if (discovered)
			return;
		else
			discovered = true;

		printf("[main] connect start\n");
		conn_start(addr);
	}
}

void on_dev_connected(addr_t addr)
{
	printf("[main] auth start\n");
	// NOTICE: service discovery should wait before param updated event or bond for auth secure param(or passkey)
	auth_start(true, false, 0x2);
	// than
	//service_discovery_start();
}

void on_dev_passkey_required(std::string passkey)
{
	printf("[main] show passkey %s\n", passkey.c_str());
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
	service_idx = 0;
	printf("[main] service discovery 0 start\n");
	service_discovery_start(service_list[0].uuid, service_list[0].type);
}

void on_dev_service_discovered(uint16_t last_handle)
{
	if (++service_idx < service_list.size()) {
		printf("[main] service discovery %d start\n", service_idx);
		service_discovery_start(service_list[service_idx].uuid, service_list[service_idx].type);
	}
	else {
		// read report ref and enable cccd notification
		printf("[main] service enable start\n");
		service_enable_start();
	}
}

void on_dev_service_enabled()
{
	printf("[main] service enabled\n");
}

void on_dev_failed(std::string stage)
{
	printf("[main] on failed %s\n", stage.c_str());
}

void on_dev_data_received(uint16_t handle, data_t data)
{
	printf("[main] data received 0x%X ", handle);
	for (int i = 0; i < data.data_len; i++) {
		printf("%02x ", (unsigned char)data.p_data[i]);
	}
	printf("\n");
}

std::mutex mtx;
std::condition_variable cond;

uint32_t on_dev_disconnected(uint8_t reason) {
	// 0x16: close by user terminate, 0x8: close by stack timeout(power off)
	//if (disconn.reason == 0x16) {
	cond.notify_all();
	//}

	return 0;
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
	uint32_t baud_rate = 10000;
	uint8_t  cccd_value = 0;

	if (argc > 1)
	{
		strcpy_s(serial_port, argv[1]);
	}

	error_code = dongle_init(serial_port, baud_rate);

	error_code = scan_start(200, 50, true, 0);

	if (error_code != 0)
	{
		return error_code;
	}

	// Endlessly loop.
	for (;;)
	{
		char c = (char)getchar();
		if (c == 'q' || c == 'Q')
		{
			auto err = dongle_disconnect();
			// if success, wait until dongle_disconnect() finished;
			if (err = 0) {
				std::unique_lock<std::mutex> lck{ mtx };
				cond.wait(lck);
			}
			dongle_reset();
			dongle_close();

			return 0;
		}

	}
}

