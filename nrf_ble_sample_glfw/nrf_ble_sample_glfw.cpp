// nrf_ble_sample_glfw.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <mutex>
#include <condition_variable>
// filesystem requires cpp17+
#include <filesystem>
#include <fstream>

#include "dongle.h"

#include <GLFW/glfw3.h>
//for GLFW compiling runtime library config /MT[d] or /MD[d]
#ifdef _DLL
#pragma comment(lib, "glfw3.lib")
#else
#pragma comment(lib, "glfw3_mt.lib")
#endif
#ifdef _WIN32
#undef APIENTRY
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h> // for glfwGetWin32Window on windows platform
#endif
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

//overloads for ImGui namespace
#include "nrf_ble_widget.h"

#include "helper.h"

#pragma region widget status

// widget status:
static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

static bool show_msg_window = false;
static const char* msg_window_text = "";

static bool show_auth_window = false;
static const char* auth_window_text = "";
static bool auth_bond_state = true;
static bool auth_lesc_state = true;
static bool auth_oob_state = false;
static bool auth_mitm_state = false;
static bool auth_keypress_state = false;
// Allocate 8 but only 6 are used in library
static char auth_passkey[8] = "123456";
// BLE_GAP_IO_CAPS_XXXX
static std::vector<const char*> auth_iocaps_list = {
	"DISPLAY_ONLY",
	"DISPLAY_YESNO",
	"KEYBOARD_ONLY",
	"NONE",
	"KEYBOARD_DISPLAY"
};
static unsigned char auth_iocaps = 0x2;
static const char* auth_iocaps_item = auth_iocaps_list[auth_iocaps];

static bool auth_keydist_enc = true;
static bool auth_keydist_id = false;

static char serial_port[16] = "COM4";
static int baud_rate = 1000000;
typedef enum {
	NONE = 0,
	RUNNING = 1,
	SUCCESS = 2,
	FAILED = 3
}ACT_STATES;
static ACT_STATES init_state = ACT_STATES::NONE;
static ACT_STATES scan_state = ACT_STATES::NONE;

static bool renew_keypair = false;

typedef struct _device_t {
	std::string addr_str;
	std::string name;
	int8_t rssi;
	uint8_t addr[6] = { 0 };
	uint8_t addr_type;
	const char* label = NULL; // addr_str + name + rssi, for preview text
} device;

// discovered devices for combo box
static std::vector<device> device_items;
static int device_selected = -1;
static const char* device_item = NULL;
static ACT_STATES conn_state = ACT_STATES::NONE;

typedef struct _report_t {
	uint16_t handle;
	uint8_t report_refs[2] = { 0 };
	std::string handle_str;
	const char* label = NULL; // handle + refs[0] + refs[1], for preview text
} report;

// enabled hid report characteristics for combo box
static std::vector<report> report_items;
static int write_report_selected = -1;
static const char* write_report_item = NULL;
static int read_report_selected = -1;
static const char* read_report_item = NULL;

typedef struct _cmdlet_t {
	std::string caption;
	std::string report_char;
	std::string direction;
	std::string hex_string;
	uint8_t report_refs[2] = {0};
	const char* label = NULL; // for imgui preview text, or useless?
} cmdlet;

std::vector<cmdlet> cmdlet_items;
static bool show_cmdlet_window = false;
static std::vector<std::string> cmdlet_files;
static const char* cmdlet_file = NULL;

#pragma endregion

#pragma region nrf ble status

bool discovered = false;
// argv[3]
char target_addr[16] = "0";
// argv[4], default no limitation(any dBm greater than -128)
int8_t target_rssi = -50;//INT8_MIN;
bool connected = false;
bool authenticated = false;

#pragma endregion

#pragma region nrf ble event callback

void on_dev_discovered(const char* addr_str, const char* name, uint8_t addr_type, uint8_t* addr, int8_t rssi)
{
	bool exists = false;
	for (auto it = device_items.begin(); it != device_items.end(); it++) {
		if (std::string(addr_str).compare(it->addr_str) == 0) {
			it->rssi = rssi;
			if (std::string(name).empty() == false)
				it->name = std::string(name);
			sprintf_s((char*)it->label, 128, "%s %s %d", addr_str, name, rssi);
			exists = true;
			break;
		}
	}
	if (exists == false) {
		device dev;
		dev.addr_str = std::string(addr_str);
		dev.name = std::string(name);
		dev.rssi = rssi;
		memcpy_s(dev.addr, 6, addr, 6);
		dev.addr_type = addr_type;
		dev.label = (const char*)calloc(128, sizeof(char));
		if (dev.label != NULL)
			sprintf_s((char*)dev.label, 128, "%s %s %d", addr_str, name, rssi);
		device_items.push_back(dev);
	}

	if (discovered)
		return;
	if (rssi < target_rssi)
		return;
	if (std::string(target_addr).length() < 12 || std::string(addr_str).compare(target_addr) == 0)
	{
		scan_stop();
		discovered = true;
		scan_state = ACT_STATES::SUCCESS;
		//printf("[main] requirement match, connect start\n");
		//conn_start(addr_type, addr);
	}
}

void on_dev_connected(uint8_t addr_type, uint8_t* addr)
{
	connected = true;
	
	printf("[main] auth set params with key dist\n");
	// default disable oob and mitm options for target device compatible
	auth_set_params(auth_lesc_state, auth_oob_state, auth_mitm_state,
		0, auth_keydist_enc, auth_keydist_id, false, false); // key dist for owner
	auth_set_params(auth_lesc_state, auth_oob_state, auth_mitm_state,
		1, auth_keydist_enc, auth_keydist_id, false, false); // key dist for peer
	
	printf("[main] auth start\n");
	// NOTICE: service discovery should wait before param updated event or bond for auth secure param(or passkey)
	//         io_caps: use library default BLE_GAP_IO_CAPS_KEYBOARD_ONLY
	auth_start(auth_bond_state, auth_keypress_state, auth_iocaps, auth_passkey);
	// than
	//service_discovery_start();
}

bool show_passkey_msg = false;
char passkey_msg[64] = { 0 };

void on_dev_passkey_required(const char* passkey)
{
	printf("[main] show passkey %s\n", passkey);

	sprintf_s(passkey_msg, "type passkey %s on keyboard", passkey);
	show_passkey_msg = true;
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
	show_passkey_msg = false; //disable passkey message after authenticated
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

	uint16_t* handle_list = (uint16_t*)calloc(count, sizeof(uint16_t));
	uint8_t* refs_list = (uint8_t*)calloc(count * 2, sizeof(uint8_t));
	uint16_t len = count;
	report_char_list(handle_list, refs_list, &len);
	// cleanup and ready to update UI dataset
	write_report_item = NULL;
	write_report_selected = -1;
	read_report_item = NULL;
	read_report_selected = -1;
	report_items.clear();
	char tmp[64] = { 0 };
	for (int i = 0; i < len; i++) {
		printf("[main] report char handle:%04X refs:%02x %02x\n", handle_list[i], refs_list[i * 2], refs_list[i * 2 + 1]);
		
		report rep;
		rep.handle = handle_list[i];
		rep.report_refs[0] = refs_list[i * 2];
		rep.report_refs[1] = refs_list[i * 2 + 1];
		sprintf_s(tmp, "0x%x", rep.handle);
		rep.handle_str = std::string(tmp);
		rep.label = (const char*)calloc(128, sizeof(char));
		if (rep.label != NULL) {
			if (rep.report_refs[0] == 0 && rep.report_refs[1] == 0)
				sprintf_s((char*)rep.label, 128, "0x%04x", rep.handle);
			else
				sprintf_s((char*)rep.label, 128, "0x%04x 0x%02x 0x%02x", rep.handle, rep.report_refs[0], rep.report_refs[1]);
		}
		report_items.push_back(rep);
	}
	fflush(stdout);

	conn_state = ACT_STATES::SUCCESS;
}

std::mutex mtx;
std::condition_variable cond;

uint32_t on_dev_disconnected(uint8_t reason) {
	// 0x16: close by user terminate, 0x8: close by stack timeout(power off)
	printf("[main] disconnected reason:%x\n", reason);
	discovered = false;
	connected = false;
	cond.notify_all();

	// cleanup device and ready to update UI dataset
	device_items.clear();
	device_selected = -1;
	device_item = "";

	// cleanup report and ready to update UI dataset
	write_report_item = NULL;
	write_report_selected = -1;
	read_report_item = NULL;
	read_report_selected = -1;
	report_items.clear();
	conn_state = ACT_STATES::NONE;
	return reason;
}

void on_dev_failed(const char* stage)
{
	printf("[main] on failed %s\n", stage);

	// update UI result text
	conn_state = ACT_STATES::FAILED;
}

void on_dev_data_received(uint16_t handle, uint8_t* data, uint16_t len)
{
	printf("[main] data received 0x%X ", handle);
	for (int i = 0; i < len; i++) {
		printf("%02x ", (unsigned char)data[i]);
	}
	printf("\n");
}

void on_dev_data_sent(uint16_t handle, uint8_t* data, uint16_t len)
{
	printf("[main] data sent 0x%X", handle);
}

#pragma endregion

static void glfw_error_callback(int error, const char* desc) {
	printf("GLFW error:%d %s\n", error, desc);
}

static void draw_msg_widget() {
	if (show_msg_window == false)
		return;

	// Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
	ImGui::Begin("Message Window", &show_msg_window);
	ImGui::Text(msg_window_text);
	if (ImGui::Button("Close"))
		show_msg_window = false;
	ImGui::End();
}

static void draw_auth_widget() {
	ImGui::Checkbox("Show auth params widget", &show_auth_window);

	if (show_auth_window == false)
		return;

	ImGui::Begin("Auth Params Widget", &show_auth_window);

	ImGui::Checkbox("Bond", &auth_bond_state);
	ImGui::SameLine();
	ImGui::Checkbox("LESC", &auth_lesc_state);
	ImGui::SameLine();
	ImGui::Checkbox("OOB", &auth_oob_state);

	ImGui::Checkbox("MITM", &auth_mitm_state);
	ImGui::SameLine();
	ImGui::Checkbox("Keypress", &auth_keypress_state);

	ImGui::Text("Passkey");
	ImGui::SameLine();
	ImGui::InputText("##passkey", auth_passkey, IM_ARRAYSIZE(auth_passkey));

	ImGui::Text("IO Caps");
	ImGui::SameLine();
	if (ImGui::BeginCombo("##iocaps", auth_iocaps_item)) {
		for (size_t i = 0; i < auth_iocaps_list.size(); i++) {
			const bool is_selected = (i == auth_iocaps);
			if (ImGui::Selectable(auth_iocaps_list[i], is_selected)) {
				auth_iocaps = i;
				auth_iocaps_item = auth_iocaps_list[i];
			}

			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Text("Keydist");
	ImGui::SameLine();
	ImGui::Checkbox("Enc", &auth_keydist_enc);
	ImGui::SameLine();
	ImGui::Checkbox("Id", &auth_keydist_id);

	if (ImGui::Button("Close"))
		show_auth_window = false;
	ImGui::End();
}

static void draw_init_layout() {

	ImGui::Text("Serial port");
	ImGui::SameLine();
	ImGui::PushItemWidth(100);
	ImGui::InputText("##serialport", serial_port, IM_ARRAYSIZE(serial_port));

	ImGui::Text("Baud rate  ");
	ImGui::SameLine();
	ImGui::InputInt("##baudrate", &baud_rate, 1, 100);
	ImGui::PopItemWidth();

	if (ImGui::Button("Initialize")) {
		if (init_state != ACT_STATES::RUNNING) {
			init_state = ACT_STATES::RUNNING; // NOTICE: dongle_init is a sync function, running state may never shows up
			uint32_t ble_code = dongle_init(serial_port, baud_rate);
			init_state = (ble_code == 0) ? ACT_STATES::SUCCESS : ACT_STATES::FAILED;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset")) {
		dongle_disconnect();
		dongle_reset();
		// cleanup device and ready to update UI dataset
		device_items.clear();
		device_selected = -1;
		device_item = "";

		// cleanup report and ready to update UI dataset
		write_report_item = NULL;
		write_report_selected = -1;
		read_report_item = NULL;
		read_report_selected = -1;
		report_items.clear();
		init_state = ACT_STATES::NONE;
	}

	ImGui::SameLine();
	switch (init_state)
	{
	case ACT_STATES::NONE:
		ImGui::Text("ready");
		break;
	case ACT_STATES::RUNNING:
		ImGui::Text("wait...");
		break;
	case ACT_STATES::SUCCESS:
		ImGui::Text("ok");
		break;
	case ACT_STATES::FAILED:
		ImGui::Text("fail");
		break;
	default:
		break;
	}

	if (ImGui::Button("Renew kaypair")) {
		keypair_init(true);
		renew_keypair = true;
	}
	ImGui::SameLine();
	if (init_state == ACT_STATES::SUCCESS) {
		if (renew_keypair == false)
			ImGui::Text("Note: key store will be changed");
		else
			ImGui::Text("renew ok");
	}
	else {
		ImGui::Text("init required");
	}
}

static void draw_scan_layout() {

	if (ImGui::Button("Scan start")) {
		if (scan_state != ACT_STATES::RUNNING) {
			auto ble_state = scan_start(200, 50, true, 0);
			scan_state = (ble_state == 0) ? ACT_STATES::RUNNING : ACT_STATES::FAILED;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Scan stop")) {
		if (scan_state == ACT_STATES::RUNNING) {
			auto ble_state = scan_stop();
			scan_state = (ble_state == 0) ? ACT_STATES::SUCCESS : ACT_STATES::FAILED;
		}
	}

	ImGui::SameLine();
	switch (scan_state) {
	case ACT_STATES::NONE:
		if (init_state == ACT_STATES::SUCCESS)
			ImGui::Text("ready");
		else
			ImGui::Text("init required");
		break;
	case ACT_STATES::RUNNING:
		ImGui::Text("scan...");
		break;
	case ACT_STATES::SUCCESS:
		ImGui::Text("ok");
		break;
	case ACT_STATES::FAILED:
		ImGui::Text("fail");
		break;
	default:
		break;
	}
}

static void draw_conn_layout() {

	ImGui::Text("Addr");
	ImGui::SameLine();
	ImGui::PushItemWidth(100);
	ImGui::InputText("##addr", target_addr, IM_ARRAYSIZE(serial_port));

	ImGui::Text("Rssi");
	ImGui::SameLine();
	ImGui::InputInt8("##rssi", &target_rssi, 1, 5);
	ImGui::PopItemWidth();

	ImGui::Text("Discovered:   ");
	ImGui::SameLine();
	if (ImGui::BeginCombo("##discovered", device_item)) {
		for (int i = 0; i < device_items.size(); i++) {
			const bool is_selected = (device_selected == i);
			if (ImGui::Selectable(device_items[i].addr_str, device_items[i].label, is_selected)) {
				device_selected = i;
				device_item = device_items[i].label;
			}

			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("Connect")) {
		if (device_selected == -1 || device_item == NULL || strlen(device_item) == 0) {
			msg_window_text = "No item selected";
			show_msg_window = true;
		}
		else {
			if (scan_state == ACT_STATES::RUNNING)
				scan_state = ACT_STATES::NONE;
			auto ble_code = conn_start(
				device_items[device_selected].addr_type,
				device_items[device_selected].addr);
			conn_state = (ble_code == 0) ? ACT_STATES::RUNNING : ACT_STATES::FAILED;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Disconnect")) {
		auto ble_code = dongle_disconnect();
		conn_state = ACT_STATES::NONE;
	}

	ImGui::SameLine();
	switch (conn_state) {
	case ACT_STATES::NONE:
		ImGui::Text("disconnected");
		break;
	case ACT_STATES::RUNNING:
		ImGui::Text("connect...");
		break;
	case ACT_STATES::SUCCESS:
		ImGui::Text("ok");
		break;
	case ACT_STATES::FAILED:
		ImGui::Text("fail");
		break;
	default:
		break;
	}

	if (show_passkey_msg) {
		ImGui::Text(passkey_msg);
	}
}

static char write_data_str[128] = "00 00 00 00";
static char read_data_str[128] = "00 00 00 00";

static void report_button_write_clicked()
{
	if (write_report_selected == -1 || write_report_item == NULL || strlen(write_report_item) == 0) {
		msg_window_text = "No endpoint selected";
		show_msg_window = true;
	}
	else {
		std::string data = std::string(write_data_str);
		auto sp = split(data, " ");
		if (sp.size() > 0) {
			uint8_t* bytes = (uint8_t*)calloc(sp.size(), sizeof(uint8_t));
			if (bytes != NULL) {
				for (size_t i = 0; i < sp.size(); i++)
					bytes[i] = strtoul(sp[i].c_str(), NULL, 16);
				auto ble_code = data_write(report_items[write_report_selected].handle, bytes, sp.size(), 2000);
			}
		}
	}
}

static void report_button_read_clicked()
{
	if (read_report_selected == -1 || read_report_item == NULL || strlen(read_report_item) == 0) {
		msg_window_text = "No endpoint selected";
		show_msg_window = true;
	}
	else {
		uint8_t bytes[256] = { 0 };
		uint16_t len = 256;
		auto ble_code = data_read(report_items[read_report_selected].handle, bytes, &len, 2000);
		if (ble_code == 0) {
			for (int i = 0; i < len && i * 3 < 128; i++)
				sprintf_s(&read_data_str[i * 3], 4, "%02x ", bytes[i]);
		}
	}
}

static void draw_report_layout() {

	ImGui::Text("Report write: ");
	ImGui::SameLine();
	if (ImGui::BeginCombo("##reportwrite", write_report_item)) {
		for (int i = 0; i < report_items.size(); i++) {
			const bool is_selected = (write_report_selected == i);
			if (ImGui::Selectable(report_items[i].handle_str, report_items[i].label, is_selected)) {
				write_report_selected = i;
				write_report_item = report_items[i].label;
			}

			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("Write")) {
		report_button_write_clicked();
	}
	ImGui::SameLine();
	//TODO: try play with ImGuiInputTextFlags_CharsHexadecimal?
	ImGui::InputText("##weitedata", write_data_str, IM_ARRAYSIZE(write_data_str));

	ImGui::Text("Report read:  ");
	ImGui::SameLine();
	if (ImGui::BeginCombo("##reportread", read_report_item)) {
		for (int i = 0; i < report_items.size(); i++) {
			const bool is_selected = (read_report_selected == i);
			if (ImGui::Selectable(report_items[i].handle_str, report_items[i].label, is_selected)) {
				read_report_selected = i;
				read_report_item = report_items[i].label;
			}

			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (ImGui::Button("Read")) {
		report_button_read_clicked();
	}
	ImGui::SameLine();
	ImGui::InputText("##readdata", read_data_str, IM_ARRAYSIZE(read_data_str));
}

static void read_cmdlet_file() {
	
	if (cmdlet_file == NULL)
		return;

	cmdlet_items.clear();

	std::fstream file;
	file.open(cmdlet_file, std::ios::in);
	std::string line;
	while (std::getline(file, line)) {
		trim(line);
		if (line.rfind("#", 0) == 0)
			continue;
		auto sp = split(line, ",");
		if (sp.size() < 4)
			continue;

		// report char string to refs bytes
		trim(sp[1]);
		auto refsp = split(sp[1], " ");
		if (refsp.size() < 2)
			continue;

		cmdlet c = {
			.caption = sp[0], 
			.report_char = sp[1],
			.direction = sp[2],
			.hex_string = sp[3] };
		c.report_refs[0] = strtoul(refsp[0].c_str(), NULL, 16);
		c.report_refs[1] = strtoul(refsp[1].c_str(), NULL, 16);
		cmdlet_items.push_back(c);
	}
	file.close();
}

static void draw_cmdlet_layout() {
	
	ImGui::Checkbox("Show cmdlet widget", &show_cmdlet_window);
	if (cmdlet_file == NULL)
		show_cmdlet_window = false;

	ImGui::SameLine();
	if (ImGui::BeginCombo("##cmdletfiles", cmdlet_file)) {
		for (int i = 0; i < cmdlet_files.size(); i++) {
			const bool is_selected = (cmdlet_file != NULL && _stricmp(cmdlet_file, cmdlet_files[i].c_str()) == 0);
			if (ImGui::Selectable(cmdlet_files[i], cmdlet_files[i].c_str(), is_selected)) {
				cmdlet_file = cmdlet_files[i].c_str();
				read_cmdlet_file();
			}

			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (show_cmdlet_window == false)
		return;

	if (cmdlet_items.empty())
		return;

	ImGui::Begin("Cmdlet Widget", &show_cmdlet_window);

	for (auto it = cmdlet_items.begin(); it != cmdlet_items.end(); it++) {
		if (ImGui::Button(it->caption.c_str())) {
			for (int i = 0; i < report_items.size(); i++) {
				if (report_items[i].report_refs[0] == it->report_refs[0] &&
					report_items[i].report_refs[1] == it->report_refs[1]) {
					if (_stricmp(it->direction.c_str(), "READ") == 0) {
						read_report_selected = i;
						read_report_item = report_items[i].label;
						strcpy_s(read_data_str, it->hex_string.c_str());
						// perform read button click
						report_button_read_clicked();
					}
					else {
						write_report_selected = i;
						write_report_item = report_items[i].label;
						strcpy_s(write_data_str, it->hex_string.c_str());
						// perform write button click
						report_button_write_clicked();
					}
				}
			}
		}
		ImGui::SameLine();
		ImGui::Text("%s %s %s", it->report_char.c_str(), it->direction.c_str(), it->hex_string.c_str());
	}

	if (ImGui::Button("Close"))
		show_cmdlet_window = false;
	ImGui::End();
}

static void draw_main_widget() {
	static float f = 0.0f;
	static int counter = 0;

	ImGui::Begin("Yay", NULL, ImGuiWindowFlags_NoCollapse);                          // Create a window and append into it.
	
	draw_init_layout();
	ImGui::Separator();
	draw_scan_layout();
	ImGui::Separator();
	draw_auth_widget();
	ImGui::Separator();
	draw_conn_layout();
	ImGui::Separator();
	draw_report_layout();
	draw_cmdlet_layout();

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();
}

int main()
{
	uint32_t ble_code = 0;
	callback_add(FN_ON_DISCOVERED, &on_dev_discovered);
	callback_add(FN_ON_CONNECTED, &on_dev_connected);
	callback_add(FN_ON_PASSKEY_REQUIRED, &on_dev_passkey_required);
	callback_add(FN_ON_AUTHENTICATED, &on_dev_authenticated);
	callback_add(FN_ON_SERVICE_DISCOVERED, &on_dev_service_discovered);
	callback_add(FN_ON_SERVICE_ENABLED, &on_dev_service_enabled);
	callback_add(FN_ON_DISCONNECTED, &on_dev_disconnected);
	callback_add(FN_ON_FAILED, &on_dev_failed);
	callback_add(FN_ON_DATA_RECEIVED, &on_dev_data_received);
	//target_addr = std::string("112233445566");
	//target_rssi = -50;

    glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return -1;

	GLFWwindow* window = glfwCreateWindow(800, 480, "nRF BLE sample", NULL, NULL);
	if (window == NULL)
		return -1;

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // vsync

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();


	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 130");

	//debug add combo items
	//combo_items.push_back(std::string("test1"));
	// default first window size, TODO: or read from imgui.ini?
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(500, 300));

	// get cmdlet file list from working directory, filesystem requires cpp17+
	for (const auto& entry : std::filesystem::directory_iterator(".")) {
		if (entry.is_regular_file() && entry.path().extension().compare(".csv") == 0)
			cmdlet_files.push_back(entry.path().string());
	}

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		// Start ImGui backend frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		draw_main_widget();
		draw_msg_widget();

		// Rendering
		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	ble_code = dongle_disconnect();
	ble_code = dongle_reset();

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

