#pragma once
#include <cstdint>


class VNCKeyMouse
{
private:
	uintptr_t sock;
	char ip[0x10];
	int port;
	bool is_connected;
	uint16_t width;
	uint16_t height;
	//uint8_t old_key_byte_0;
	uint16_t last_mouse_x;
	uint16_t last_mouse_y;
	uint8_t old_button_mask;

public:
	VNCKeyMouse();
	VNCKeyMouse(const char* ip, uint16_t port);
	~VNCKeyMouse();

private:
	bool send_bytes(void* bytes, uint32_t count);
	bool recv_bytes(void* bytes, uint32_t count);
	int randint(int min, int max);
	uint32_t convert_to_rfb(uint32_t vkcode);
	bool initialize_vnc();
	bool pointer_event(uint16_t x, uint16_t y, uint8_t button_mask);
	bool key_event(uint32_t key, uint8_t down);

public:
	bool connect_server(const char* ip, uint16_t port);
	bool reconnect_server();
	bool disconnect();
	bool get_is_connected();
	void test();

public:
	bool mouse_move(int x, int y, int flag = 0);
	bool mouse_wheel(int wheel);
	bool left_button_down();
	bool left_button_up();
	bool right_button_down();
	bool right_button_up();
	bool key_down(int key);
	bool key_up(int key);
	bool key_press(int key);
	bool all_key_up();
};
