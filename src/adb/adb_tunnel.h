#pragma once
#include "net.h"

class sc_adb_tunnel {
public:
    bool m_enabled;
    bool m_forward; // use "adb forward" instead of "adb reverse" 暂不实现forward模式
    sc_socket m_server_socket; // only used if !forward
    uint16_t m_local_port;

    sc_adb_tunnel();

};


