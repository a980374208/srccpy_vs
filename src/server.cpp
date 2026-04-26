#include "server.hpp"
#include "adb/adb.h"
#include <cassert>
#include <memory>
#include "adb/adb_device.h"

sc_server::sc_server(sc_server &&other) noexcept
{
}

sc_server &sc_server::operator=(sc_server &&other) noexcept
{
	if (this != &other)
	{
		// TODO: 实现移动赋值运算符
	}
	return *this;
}

sc_server::sc_server() : m_serial(nullptr),
						 m_device_socket_name(nullptr),
						 m_stopped(false),
						 m_video_socket(SC_SOCKET_NONE),
						 m_audio_socket(SC_SOCKET_NONE),
						 m_control_socket(SC_SOCKET_NONE)
{
}

sc_server::~sc_server()
{
}

bool sc_server::server_init(const sc_server_params *params, const sc_server_callbacks *cbs, void *cbs_userdata)
{
	this->m_params = *params;

	bool ok = sc_adb_init();
	if (!ok)
	{
		return false;
	}

	assert(cbs);
	assert(cbs->on_connection_failed);
	assert(cbs->on_connected);
	assert(cbs->on_disconnected);

	m_cbs = cbs;
	m_cbs_userdata = cbs_userdata;

	return true;
}


bool sc_server::server_start()
{
	bool ok =
		sc_thread_create(&this->m_thread, run_server, "scrcpy-server", this);
	if (!ok)
	{
		//LOGE("Could not create server thread");
		return false;
	}

	return true;
}

int sc_server::run_server(void *data)
{
	sc_server* server = (sc_server*)data;
	bool ok = sc_adb_start_server(&server->m_intr, 0);
	struct sc_adb_device_selector selector;
	const char* env_serial = getenv("ANDROID_SERIAL");
	if (env_serial)
	{
		//LOGI("Using ANDROID_SERIAL: %s", env_serial);
		selector.type = SC_ADB_DEVICE_SELECT_SERIAL;
		selector.serial = env_serial;
	}
	else
	{
		selector.type = SC_ADB_DEVICE_SELECT_ALL;
	}

	struct sc_adb_device device;
	// Enumerate devices (including connection status, authorization state, device ID, etc.)
	ok = sc_adb_select_device(server->m_intr, &selector, 0, &device);







	return 0;
}
