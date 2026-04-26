#include "adb_tunnel.h"

sc_adb_tunnel::sc_adb_tunnel() : 
    m_enabled(false), 
    m_forward(false), 
    m_server_socket(SC_SOCKET_NONE), 
    m_local_port(0)
{
}


