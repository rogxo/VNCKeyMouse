#include "VNCKeyMouse.h"
#include <WS2tcpip.h>
#include <winsock2.h>
#include <unordered_map>
#include <random>
#include <rfb/rfbproto.h>
#include <rfb/keysym.h>

#pragma comment(lib, "ws2_32.lib")

#ifdef _DEBUG
#include <stdio.h>
#define dbg(__x__)  printf __x__
#else
#define dbg(__x__)
#endif


VNCKeyMouse::VNCKeyMouse()
{
    memset(this->ip, 0, 16);
    this->port = 0;
    this->sock = -1;
    this->old_button_mask = 0;
    this->last_mouse_x = 0;
    this->last_mouse_y = 0;

    WSADATA WSAData;
    int result = WSAStartup(MAKEWORD(2, 2), &WSAData);
    if (result != 0) {
        dbg(("[VNCKeyMouse] WSAStartup error : %d\n", WSAGetLastError()));
    }
}

VNCKeyMouse::VNCKeyMouse(const char* ip, uint16_t port)
{
    memset(this->ip, 0, 0x10);
    this->port = 0;
    this->sock = -1;
    this->old_button_mask = 0;
    this->last_mouse_x = 0;
    this->last_mouse_y = 0;

    WSADATA WSAData;
    int result = WSAStartup(MAKEWORD(2, 2), &WSAData);
    if (result != 0) {
        dbg(("[VNCKeyMouse] WSAStartup error : %d\n", WSAGetLastError()));
    }

    if (!ip || !port) {
        return;
    }
    if (!this->connect_server(ip, port)) {
        return;
    }
}

VNCKeyMouse::~VNCKeyMouse()
{
    this->disconnect();
    int result = WSACleanup();
    if (result != 0) {
        dbg(("[VNCKeyMouse] WSACleanup error : %d\n", WSAGetLastError()));
    }
}

bool VNCKeyMouse::send_bytes(void* bytes, uint32_t count)
{
    if (this->sock < 0) {
        return false;
    }
    int send_count = send(this->sock, (const char*)bytes, count, 0);
    if (send_count < 0) {
        dbg(("[VNCKeyMouse] send error : %d\n", WSAGetLastError()));
        reconnect_server();
        this->is_connected = false;
        return false;
    }
    dbg(("[VNCKeyMouse] send bytes: "));
    for (size_t i = 0; i < count; i++) {
        dbg(("%02X ", ((uint8_t*)bytes)[i]));
    }
    dbg(("\n"));
    return true;
}

bool VNCKeyMouse::recv_bytes(void* bytes, uint32_t count)
{
    int recv_count = recv(this->sock, (char*)bytes, count, 0);  // 接收服务器的回显数据
    if (recv_count < 0) {
        return false;
    }
    dbg(("[VNCKeyMouse] recv bytes: "));
    for (size_t i = 0; i < recv_count; i++) {
        dbg(("%02X ", ((uint8_t*)bytes)[i]));
    }
    dbg(("\n"));
    return true;
}

int VNCKeyMouse::randint(int min, int max)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(min, max);
    return dis(gen);
}

static std::unordered_map<uint32_t, uint32_t> key_map = {
    { VK_RETURN,        XK_Return },
    { VK_ESCAPE,        XK_Escape },
    { VK_BACK,          XK_BackSpace },
    { VK_TAB,           XK_Tab },
    { VK_SPACE,         XK_space },

    { VK_CAPITAL,       XK_Caps_Lock },
    { VK_LSHIFT,        XK_Shift_L },
    { VK_LCONTROL,      XK_Control_L },
    { VK_LMENU,         XK_Meta_L },
    { VK_LWIN,          XK_Super_L },
};

uint32_t VNCKeyMouse::convert_to_rfb(uint32_t vkcode)
{
    //if (key_map.find(vkcode) != key_map.end()) {
    //    return key_map[vkcode];
    //}
    if (vkcode >= VK_F1 && vkcode <= VK_F12) {
        return XK_F1 + vkcode - VK_F1;
    }
    if (vkcode >= 'A' && vkcode <= 'Z') {
        return vkcode;
    }
    if (vkcode >= '0' && vkcode <= '9') {
        return vkcode;
    }
    return key_map[vkcode];
}

bool VNCKeyMouse::initialize_vnc()
{
    constexpr int buf_size = 0x100;
    char buf[buf_size] = { 0 };

    // get server protocol
    if (!this->recv_bytes(buf, sz_rfbProtocolVersionMsg)) {
        return false;
    }
    //RFB xxx.yyy
    if (buf[0] != 'R' || buf[1] != 'F' || buf[2] != 'B' || buf[3] != ' ' || buf[7] != '.') {
        dbg(("[VNCKeyMouse] invalid protocol\n"));
        this->disconnect();
        return false;
    }
    dbg(("[VNCKeyMouse] server protocol: %s\n", buf));

    // send protocol we wanted
    if (!this->send_bytes(buf, sz_rfbProtocolVersionMsg)) {
        return false;
    }

    // get rfbSecType...
    if (!this->recv_bytes(buf, buf_size)) {
        return false;
    }

    uint8_t sec_type_count = buf[0];
    if (!sec_type_count) {
        return false;
    }
    uint8_t sec_type = buf[1];  // default use first sec type.
    dbg(("[VNCKeyMouse] sec_type[0]: %d\n", sec_type));
    if (sec_type == rfbSecTypeInvalid) {
        dbg(("[VNCKeyMouse] invalid sec_type\n"));
        return false;
    }

    // comfirm sec_type to server
    if (!this->send_bytes(&sec_type, sizeof(sec_type))) {
        return false;
    }
    dbg(("[VNCKeyMouse] comfirm sec_type: %d\n", sec_type));

    if (sec_type == rfbSecTypeNone) {
        // do nothing
    }
    else if (sec_type == rfbSecTypeVncAuth) {
        // TODO
        // verify password...
    }

    // get authentication result
    if (!this->recv_bytes(buf, buf_size)) {
        return false;
    }
    uint32_t auth_result = *(uint32_t*)&buf;
    switch (auth_result) {
    case rfbVncAuthFailed:
        dbg(("[VNCKeyMouse] rfbVncAuthFailed\n"));
        return false;
    case rfbVncAuthTooMany:
        dbg(("[VNCKeyMouse] rfbVncAuthTooMany\n"));
        return false;
    case rfbVncAuthOK:
        dbg(("[VNCKeyMouse] rfbVncAuthOK\n"));
        break;
    default:
        dbg(("[VNCKeyMouse] unknown auth_result\n"));
        return false;
    }

    rfbClientInitMsg cl;
    cl.shared = 1;
    if (!this->send_bytes(&cl, sz_rfbClientInitMsg)) {
        return false;
    }

    rfbServerInitMsg si;
    if (!this->recv_bytes(&si, sz_rfbServerInitMsg)) {
        return false;
    }
    this->width = ntohs(si.framebufferWidth);
    this->height = ntohs(si.framebufferHeight);
    uint32_t name_length = ntohl(si.nameLength);
    char* name_buffer = (char*)malloc(name_length + 1ull);
    if (!recv_bytes(name_buffer, name_length)) {
        return false;
    }
    name_buffer[name_length] = 0;
    dbg(("[VNCKeyMouse] VNC Server:\n"));
    dbg(("[VNCKeyMouse]   Name: %s\n", name_buffer));
    dbg(("[VNCKeyMouse]   Resolution: %d*%d\n", this->width, this->height));
    free(name_buffer);

    // ignore set format and encodings if just use it for keyboard and mouse control
    //...

    // send update request
    rfbFramebufferUpdateRequestMsg urq = { 0 };
    urq.type = rfbFramebufferUpdateRequest;
    urq.incremental = 0;
    urq.x = htons(0);
    urq.y = htons(0);
    urq.w = htons(width);
    urq.h = htons(height);
    if (!this->send_bytes(&urq, sz_rfbFramebufferUpdateRequestMsg)) {
        return false;
    }

    //if (!this->recv_bytes(buf, buf_size)) {
    //    return false;
    //}
    //dbg(("[VNCKeyMouse] recv: %s\n", buf));

    return true;
}

bool VNCKeyMouse::pointer_event(uint16_t x, uint16_t y, uint8_t button_mask)
{
    rfbPointerEventMsg pe;

    pe.type = rfbPointerEvent;
    pe.buttonMask = button_mask;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > width) x = width;
    if (y > height) y = height;
    pe.x = htons(x);
    pe.y = htons(y);

    if (!this->send_bytes(&pe, sz_rfbPointerEventMsg)) {
        return false;
    }
    return true;
}

bool VNCKeyMouse::key_event(uint32_t key, uint8_t down)
{
    rfbKeyEventMsg ke;

    ke.type = rfbKeyEvent;
    ke.down = down;
    ke.key = htonl(key);

    if (!this->send_bytes(&ke, sz_rfbKeyEventMsg)) {
        return false;
    }
    return true;
}

bool VNCKeyMouse::connect_server(const char* ip, uint16_t port)
{
    uintptr_t cfd = 0;
    struct sockaddr_in serv_addr;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr.s_addr);

    cfd = socket(AF_INET, SOCK_STREAM, 0);
    if (cfd == -1) {
        dbg(("[VNCKeyMouse] socket error : %d\n", WSAGetLastError()));
        return false;
    }
    memcpy(this->ip, ip, 16);
    this->port = port;
    this->sock = cfd;

    int ret = connect(cfd, (SOCKADDR*)&serv_addr, sizeof(serv_addr));
    if (ret != 0) {
        dbg(("[VNCKeyMouse] connect error : %d\n", WSAGetLastError()));
        return false;
    }

    if (!initialize_vnc()) {
        dbg(("[VNCKeyMouse] fail to initialize vnc\n"));
        this->disconnect();
        return false;
    }
    this->is_connected = true;

    return true;
}

bool VNCKeyMouse::reconnect_server()
{
    return this->connect_server(this->ip, this->port);
}

bool VNCKeyMouse::disconnect()
{
    if (this->sock < 0) {
        return false;
    }

    int ret = shutdown(this->sock, SD_BOTH);
    if (ret != 0) {
        dbg(("[VNCKeyMouse] shutdown error\n"));
        return false;
    }

    ret = closesocket(this->sock);
    if (ret != 0) {
        dbg(("[VNCKeyMouse] closesocket error\n"));
        return false;
    }

    this->sock = -1;
    this->is_connected = false;
    return true;
}

bool VNCKeyMouse::get_is_connected()
{
    return this->is_connected;
}

void VNCKeyMouse::test()
{
    

}

bool VNCKeyMouse::mouse_move(int x, int y, int flag)
{
    switch (flag)
    {
    case MOUSE_MOVE_RELATIVE:
        last_mouse_x += x;
        last_mouse_y += y;
        break;
    case MOUSE_MOVE_ABSOLUTE:
        last_mouse_x = x;
        last_mouse_y = y;
        break;
    default:
        return false;
        break;
    }
    return this->pointer_event(last_mouse_x, last_mouse_y, old_button_mask);
}

bool VNCKeyMouse::mouse_wheel(int wheel)
{
    return false;
}

bool VNCKeyMouse::left_button_down()
{
    this->old_button_mask |= 1;     // bit 00000001
    return this->pointer_event(last_mouse_x, last_mouse_y, old_button_mask);
}

bool VNCKeyMouse::left_button_up()
{
    this->old_button_mask &= 0xFE;
    return this->pointer_event(last_mouse_x, last_mouse_y, old_button_mask);
}

bool VNCKeyMouse::right_button_down()
{
    this->old_button_mask |= 4;     // bit 00000100
    return this->pointer_event(last_mouse_x, last_mouse_y, old_button_mask);
}

bool VNCKeyMouse::right_button_up()
{
    this->old_button_mask &= 0xFB;
    return this->pointer_event(last_mouse_x, last_mouse_y, old_button_mask);
}

bool VNCKeyMouse::key_down(int key)
{
    return this->key_event(convert_to_rfb(key), 1);
}

bool VNCKeyMouse::key_up(int key)
{
    return this->key_event(convert_to_rfb(key), 0);
}

bool VNCKeyMouse::key_press(int key)
{
    if (!this->key_down(key)) {
        return false;
    }
    Sleep(randint(80, 120));
    if (!this->key_up(key)) {
        return false;
    }
    return true;
}

bool VNCKeyMouse::all_key_up()
{
    for (char c = 'A'; c <= 'Z'; c++) {
        key_up(c);
    }

    for (char c = VK_F1; c <= VK_F12; c++) {
        key_up(c);
    }

    key_up(VK_SPACE);
    key_up(VK_ESCAPE);
    key_up(VK_RETURN);
    key_up(VK_BACK);
    key_up(VK_TAB);
    key_up(VK_SPACE);
    key_up(VK_CAPITAL);
    key_up(VK_LSHIFT);
    key_up(VK_LCONTROL);
    key_up(VK_LMENU);
    key_up(VK_LWIN);

    left_button_up();
    right_button_up();

    return true;
}
