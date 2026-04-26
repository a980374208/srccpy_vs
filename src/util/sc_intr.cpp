#include "sc_intr.h"
#include <assert.h>
#include "net.h"


sc_intr::sc_intr():
    socket(SC_SOCKET_NONE),
	process(SC_PROCESS_NONE),
    interrupted(false)
{
}

bool sc_intr::set_socket(sc_socket socket)
{
    assert(this->process == SC_PROCESS_NONE);

    std::lock_guard<sc_mutex> lock(this->mutex);
    bool interrupted =
        atomic_load_explicit(&this->interrupted, std::memory_order_relaxed);
    if (!interrupted) {
        this->socket = socket;
    }

    return !interrupted;
}

void sc_intr::intr_interrupt()
{
    std::lock_guard<sc_mutex> lock(this->mutex);

    atomic_store_explicit(&this->interrupted, true, std::memory_order_relaxed);

    // No more than one component to interrupt
    assert(this->socket == SC_SOCKET_NONE ||
        this->process == SC_PROCESS_NONE);

    if (this->socket != SC_SOCKET_NONE) {
        //LOGD("Interrupting socket");
        net_interrupt(this->socket);
        this->socket = SC_SOCKET_NONE;
    }
    if (this->process != SC_PROCESS_NONE) {
        //LOGD("Interrupting process");
        sc_process_terminate(this->process);
        this->process = SC_PROCESS_NONE;
    }
}

bool sc_intr::is_interrupted()
{
    return atomic_load_explicit(&this->interrupted, std::memory_order_relaxed);
}

bool sc_intr::net_listen_intr(sc_socket server_socket, uint32_t addr, uint16_t port, int backlog)
{
    if (!this->set_socket(server_socket)) {
        // Already interrupted
        return false;
    }

    bool ret = net_listen(server_socket, addr, port, backlog);

	this->set_socket(SC_SOCKET_NONE);
    return ret;
}

bool sc_intr::net_connect_intr(sc_intr& intr, sc_socket socket, uint32_t addr, uint16_t port)
{
    if (!intr.set_socket(socket)) {
        // Already interrupted
        return false;
    }

    bool ret = net_connect(socket, addr, port);

    intr.set_socket(SC_SOCKET_NONE);
    return ret;
}

bool sc_intr::set_process(sc_pid pid)
{
    assert(this->socket == SC_SOCKET_NONE);

    std::lock_guard<sc_mutex> lock(this->mutex);
    bool interrupted =
        atomic_load_explicit(&this->interrupted, std::memory_order_relaxed);
    if (!interrupted) {
        this->process = pid;
    }
    return !interrupted;
}


