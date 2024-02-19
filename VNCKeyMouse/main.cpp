#include "VNCKeyMouse.h"
#include <cstdio>
#include <Windows.h>


int main()
{
	VNCKeyMouse* vkm = new VNCKeyMouse("127.0.0.1", 5900);
	if (vkm->get_is_connected()) {
		puts("连接成功");
	}
	Sleep(1000);
	
	for (size_t i = 0; i < 6; i++) {
		//vkm->key_down('Q');
		vkm->key_press(VK_LWIN);		//VK_LWIN
		Sleep(100);
	}

	vkm->mouse_move(500, 500, MOUSE_MOVE_ABSOLUTE);
	Sleep(50);
	vkm->left_button_down();
	for (size_t i = 0; i < 300; i++) {
		vkm->mouse_move(1, 0);
		Sleep(3);
	}
	vkm->left_button_up();
	
	vkm->all_key_up();

	system("pause");
	return 0;
}
