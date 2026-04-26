#include "srccpy.hpp"
#include "rand.h"
#include "server.hpp"

srccpy::srccpy()
{
}

int srccpy::srccpy_init()
{
    uint32_t scid = generate_scid();

    static const struct sc_server_callbacks cbs = {
        &srccpy::sc_server_on_connection_failed,
        &srccpy::sc_server_on_connected,
        &srccpy::sc_server_on_disconnected};
    struct sc_server_params *pm = (struct sc_server_params *)malloc(sizeof(struct sc_server_params));
    server.server_init(pm, &cbs, this);
    server.server_start();

    return 0;
}

void srccpy::sc_server_on_connection_failed(sc_server &server, void *userdata)
{
}

void srccpy::sc_server_on_connected(sc_server &server, void *userdata)
{
}

void srccpy::sc_server_on_disconnected(sc_server &server, void *userdata)
{
}

uint32_t srccpy::generate_scid()
{
    sc_rand rand;
    // Only use 31 bits to avoid issues with signed values on the Java-side
    return rand.sc_rand_u32() & 0x7FFFFFFF;
}
