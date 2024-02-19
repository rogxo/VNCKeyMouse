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
		//vkm->key_down('A');
		vkm->key_press(VK_LWIN);		//VK_LWIN
		Sleep(100);
	}
	
	//vkm->mouse_move(0, 0, MOUSE_MOVE_ABSOLUTE);
	//for (size_t i = 0; i < 5; i++) {
	//	vkm->mouse_move(100, 100);
	//	Sleep(500);
	//}

	vkm->mouse_move(500, 500, MOUSE_MOVE_ABSOLUTE);
	Sleep(50);
	vkm->left_button_down();
	for (size_t i = 0; i < 100; i++) {
		vkm->mouse_move(1, 1);
		Sleep(3);
	}
	vkm->left_button_up();

	vkm->all_key_up();

	delete vkm;

	//system("pause");
	return 0;
}
