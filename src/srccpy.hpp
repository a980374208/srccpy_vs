#pragma once
#include "stdint.h"
#include "server.hpp"
#include "codec/demuxer.h"
#include <memory>

enum scrcpy_exit_code {
	// Normal program termination
	SCRCPY_EXIT_SUCCESS = 0,

	// No connection could be established
	SCRCPY_EXIT_FAILURE,

	// Device was disconnected while running
	SCRCPY_EXIT_DISCONNECTED,
};

class srccpy {
public:
	srccpy();
	srccpy(const srccpy &) = delete;
	~srccpy();
	int srccpy_init(const scrcpy_options &options);

	static void sc_server_on_connection_failed(sc_server &server, void *userdata);

	static void sc_server_on_connected(sc_server &server, void *userdata);

	static void sc_server_on_disconnected(sc_server &server, void *userdata);

private:
	uint32_t generate_scid();

public:
	sc_server server;
	sc_demuxer video_demuxer;
	sc_demuxer audio_demuxer;
};
