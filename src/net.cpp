#include "net.h"
#include <assert.h>


#ifdef _WIN32
static int ws_initialized = 0;
#endif

// 网络初始化
int net_init(void) {
#ifdef _WIN32
    if (!ws_initialized) {
        WSADATA wsa_data;
        int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (result != 0) {
            return -1;
        }
        ws_initialized = 1;
    }
#endif
    return 0;
}

// 网络清理
void net_cleanup(void) {
#ifdef _WIN32
    if (ws_initialized) {
        WSACleanup();
        ws_initialized = 0;
    }
#endif
}

static inline bool
sc_raw_socket_close(sc_raw_socket raw_sock) {
#ifndef _WIN32
    return !close(raw_sock);
#else
    return !closesocket(raw_sock);
#endif
}

// 创建 TCP 服务器 socket
socket_t net_create_server_socket(int port) {
    socket_t server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }

    // 设置地址重用
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    // 绑定地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)port);

    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        CLOSE_SOCKET(server_sock);
        return INVALID_SOCKET;
    }

    // 监听
    if (listen(server_sock, 5) == SOCKET_ERROR) {
        CLOSE_SOCKET(server_sock);
        return INVALID_SOCKET;
    }

    return server_sock;
}

// 接受客户端连接
socket_t net_accept_client(socket_t server_socket) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    socket_t client_sock = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
    return client_sock;
}

// 发送数据
int net_send(socket_t sock, const char *data, int len) {
    if (sock == INVALID_SOCKET || data == NULL || len <= 0) {
        return -1;
    }
    return send(sock, data, len, 0);
}

// 接收数据
int net_recv(socket_t sock, char *buffer, int len) {
    if (sock == INVALID_SOCKET || buffer == NULL || len <= 0) {
        return -1;
    }
    return recv(sock, buffer, len, 0);
}

// 关闭 socket
void net_close(socket_t sock) {
    if (sock != INVALID_SOCKET) {
        CLOSE_SOCKET(sock);
    }
}

static inline sc_raw_socket
unwrap(sc_socket socket) {
#ifdef SC_SOCKET_CLOSE_ON_INTERRUPT
    if (socket == SC_SOCKET_NONE) {
        return SC_RAW_SOCKET_NONE;
    }

    return socket->socket;
#else
    return socket;
#endif
}



bool
net_interrupt(sc_socket socket) {
    assert(socket != SC_SOCKET_NONE);

    sc_raw_socket raw_sock = unwrap(socket);

#ifdef SC_SOCKET_CLOSE_ON_INTERRUPT
    if (!socket->closed.test_and_set(std::memory_order_acquire)) {
        return sc_raw_socket_close(raw_sock);
    }
    return true;
#else
    return !shutdown(raw_sock, SHUT_RDWR);
#endif
}