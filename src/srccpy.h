#pragma once
#include "stdint.h"
#include "server.hpp"

class srccpy
{
public:
   srccpy();
   srccpy(const srccpy &) = delete;
   int srccpy_init();

   static void
   sc_server_on_connection_failed(sc_server &server, void *userdata);

   static void
   sc_server_on_connected(sc_server &server, void *userdata);

   static void
   sc_server_on_disconnected(sc_server &server, void *userdata);

private:
   uint32_t generate_scid();

private:
   sc_server server;
};
