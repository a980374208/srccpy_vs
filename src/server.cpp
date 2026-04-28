#include "server.hpp"
#include "adb/adb.h"
#include <cassert>
#include <memory>
#include "adb/adb_device.h"
#include "util/sc_file.h"
#include <sstream>
#include <sys/types.h>
#include "sc_log.h"

#define SC_SERVER_FILENAME "scrcpy-server"

#define SC_SERVER_PATH_DEFAULT "/server/" SC_SERVER_FILENAME
#define SC_DEVICE_SERVER_PATH "/data/local/tmp/scrcpy-server"

#define SC_ADB_PORT_DEFAULT 5555
#define SC_SOCKET_NAME_PREFIX "scrcpy_"

#define SCRCPY_VERSION "3.3.4"

static const char *log_level_to_server_string(enum sc_log_level level)
{
	switch (level) {
	case SC_LOG_LEVEL_VERBOSE:
		return "verbose";
	case SC_LOG_LEVEL_DEBUG:
		return "debug";
	case SC_LOG_LEVEL_INFO:
		return "info";
	case SC_LOG_LEVEL_WARN:
		return "warn";
	case SC_LOG_LEVEL_ERROR:
		return "error";
	default:
		assert(!"unexpected log level");
		return NULL;
	}
}

static const char *sc_server_get_codec_name(enum sc_codec codec)
{
	switch (codec) {
	case SC_CODEC_H264:
		return "h264";
	case SC_CODEC_H265:
		return "h265";
	case SC_CODEC_AV1:
		return "av1";
	case SC_CODEC_OPUS:
		return "opus";
	case SC_CODEC_AAC:
		return "aac";
	case SC_CODEC_FLAC:
		return "flac";
	case SC_CODEC_RAW:
		return "raw";
	default:
		assert(!"unexpected codec");
		return NULL;
	}
}

static const char *sc_server_get_camera_facing_name(enum sc_camera_facing camera_facing)
{
	switch (camera_facing) {
	case SC_CAMERA_FACING_FRONT:
		return "front";
	case SC_CAMERA_FACING_BACK:
		return "back";
	case SC_CAMERA_FACING_EXTERNAL:
		return "external";
	default:
		assert(!"unexpected camera facing");
		return NULL;
	}
}

static const char *sc_server_get_audio_source_name(enum sc_audio_source audio_source)
{
	switch (audio_source) {
	case SC_AUDIO_SOURCE_OUTPUT:
		return "output";
	case SC_AUDIO_SOURCE_MIC:
		return "mic";
	case SC_AUDIO_SOURCE_PLAYBACK:
		return "playback";
	case SC_AUDIO_SOURCE_MIC_UNPROCESSED:
		return "mic-unprocessed";
	case SC_AUDIO_SOURCE_MIC_CAMCORDER:
		return "mic-camcorder";
	case SC_AUDIO_SOURCE_MIC_VOICE_RECOGNITION:
		return "mic-voice-recognition";
	case SC_AUDIO_SOURCE_MIC_VOICE_COMMUNICATION:
		return "mic-voice-communication";
	case SC_AUDIO_SOURCE_VOICE_CALL:
		return "voice-call";
	case SC_AUDIO_SOURCE_VOICE_CALL_UPLINK:
		return "voice-call-uplink";
	case SC_AUDIO_SOURCE_VOICE_CALL_DOWNLINK:
		return "voice-call-downlink";
	case SC_AUDIO_SOURCE_VOICE_PERFORMANCE:
		return "voice-performance";
	default:
		assert(!"unexpected audio source");
		return NULL;
	}
}

static const char *sc_server_get_display_ime_policy_name(enum sc_display_ime_policy policy)
{
	switch (policy) {
	case SC_DISPLAY_IME_POLICY_LOCAL:
		return "local";
	case SC_DISPLAY_IME_POLICY_FALLBACK:
		return "fallback";
	case SC_DISPLAY_IME_POLICY_HIDE:
		return "hide";
	default:
		assert(!"unexpected display IME policy");
		return NULL;
	}
}

static void sc_server_on_terminated(void *userdata)
{
	sc_server *server = (sc_server *)userdata;

	// If the server process dies before connecting to the server socket,
	// then the client will be stuck forever on accept(). To avoid the problem,
	// wake up the accept() call (or any other) when the server dies, like on
	// stop() (it is safe to call interrupt() twice).
	server->m_intr.intr_interrupt();

	server->m_cbs->on_disconnected(*server, server->m_cbs_userdata);

	//LOGD("Server terminated");
}

static bool connect_and_read_byte(sc_intr &intr, sc_socket socket, uint32_t tunnel_host, uint16_t tunnel_port)
{
	bool ok = intr.net_connect_intr(socket, tunnel_host, tunnel_port);
	if (!ok) {
		return false;
	}

	char byte;
	// the connection may succeed even if the server behind the "adb tunnel"
	// is not listening, so read one byte to detect a working connection
	if (intr.net_recv_intr(socket, &byte, 1) != 1) {
		// the server is not listening yet behind the adb tunnel
		return false;
	}

	return true;
}

static bool device_read_info(sc_intr &intr, sc_socket device_socket, struct sc_server_info &info)
{
	uint8_t buf[SC_DEVICE_NAME_FIELD_LENGTH];
	ssize_t r = intr.net_recv_intr(device_socket, buf, sizeof(buf));
	if (r < SC_DEVICE_NAME_FIELD_LENGTH) {
		//LOGE("Could not retrieve device information");
		return false;
	}
	// in case the client sends garbage
	buf[SC_DEVICE_NAME_FIELD_LENGTH - 1] = '\0';
	memcpy(info.device_name, (char *)buf, sizeof(info.device_name));

	return true;
}

sc_server::sc_server(sc_server &&other) noexcept {}

sc_server &sc_server::operator=(sc_server &&other) noexcept
{
	if (this != &other) {
		// TODO: 实现移动赋值运算符
	}
	return *this;
}

sc_server::sc_server()
	: m_serial(""),
	  m_device_socket_name(""),
	  m_stopped(false),
	  m_video_socket(SC_SOCKET_NONE),
	  m_audio_socket(SC_SOCKET_NONE),
	  m_control_socket(SC_SOCKET_NONE)
{
}

sc_server::~sc_server() {}

bool sc_server::server_init(const sc_server_params *params, const sc_server_callbacks *cbs, void *cbs_userdata)
{
	this->m_params = *params;

	bool ok = sc_adb_init();
	if (!ok) {
		return false;
	}

	sc_cond_init(this->m_cond_stopped);

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
	bool ok = sc_thread_create(this->m_thread, run_server, "scrcpy-server", this);
	if (!ok) {
		//LOGE("Could not create server thread");
		return false;
	}

	return true;
}

bool sc_server::push_server(sc_intr &intr, const std::string &serial)
{
	std::string server_path = get_server_path();
	if (server_path.empty()) {
		return false;
	}
	if (!sc_file_is_regular(server_path)) {
		//LOGE("'%s' does not exist or is not a regular file\n", server_path);
		return false;
	}
	bool ok = sc_adb_push(intr, serial, server_path, SC_DEVICE_SERVER_PATH, 0);
	return ok;
}

std::string sc_server::get_server_path()
{
	const char *env_path = std::getenv("SCRCPY_SERVER_PATH");

	if (env_path) {
		// if the envvar is set, use it
		//LOGD("Using SCRCPY_SERVER_PATH: %s", env_path);
		return std::string(env_path);
	}

#ifndef PORTABLE
	//LOGD("Using server: " SC_SERVER_PATH_DEFAULT);
	std::string path = SC_SERVER_PATH_DEFAULT;
	path = get_app_relative_path(path).string();
	return path;
#else
	auto local_path = sc_file_get_local_path(SC_SERVER_FILENAME);
	if (!local_path) {
		LOGE("Could not get local file path, "
		     "using " SC_SERVER_FILENAME " from current directory");
		return std::string(SC_SERVER_FILENAME);
	}
	LOGD("Using server (portable): %s", local_path);
	return std::string(local_path);
#endif
}

static inline bool validate_string(const std::string &s)
{
	// The parameters values are passed as command line arguments to adb, so
	// they must either be properly escaped, or they must not contain any
	// special shell characters.
	// Since they are not properly escaped on Windows anyway (see
	// sys/win/process.c), just forbid special shell characters.
	static const char *invalid = " ;'\"*$?&`#\\|<>[]{}()!~\r\n";
	if (s.find_first_of(invalid) != std::string::npos) {
		//LOGE("Invalid server param: [%s]", s.c_str());
		return false;
	}
	return true;
}

sc_pid sc_server::execute_server(const sc_server_params &params)
{
	sc_pid pid = SC_PROCESS_NONE;

	std::string serial = this->m_serial;
	assert(!serial.empty());

	std::vector<std::string> cmd;
	cmd.reserve(128);
	unsigned count = 0;
	cmd.push_back(sc_adb_get_executable());
	cmd.push_back("-s");
	cmd.push_back(serial);
	cmd.push_back("shell");
	cmd.push_back("CLASSPATH=" SC_DEVICE_SERVER_PATH);
	cmd.push_back("app_process");

#ifdef SERVER_DEBUGGER
	uint16_t sdk_version = sc_adb_get_device_sdk_version(&server->intr, serial);
	if (!sdk_version) {
		LOGE("Could not determine SDK version");
		return 0;
	}

#define SERVER_DEBUGGER_PORT "5005"
	const char *dbg;
	if (sdk_version < 28) {
		// Android < 9
		dbg = "-agentlib:jdwp=transport=dt_socket,suspend=y,server=y,address=" SERVER_DEBUGGER_PORT;
	} else if (sdk_version < 30) {
		// Android >= 9 && Android < 11
		dbg = "-XjdwpProvider:internal -XjdwpOptions:transport=dt_socket,"
		      "suspend=y,server=y,address=" SERVER_DEBUGGER_PORT;
	} else {
		// Android >= 11
		// Contrary to the other methods, this does not suspend on start.
		dbg = "-XjdwpProvider:adbconnection";
	}
	cmd.push_back(dbg);
#endif

	cmd.push_back("/"); // unused
	cmd.push_back("com.genymobile.scrcpy.Server");
	cmd.push_back(SCRCPY_VERSION);

	unsigned dyn_idx = count; // from there, the strings are allocated
	auto add_param = [&cmd](auto &&...args) {
		std::ostringstream ss;
		(ss << ... << args); // fold expression，逐个拼接参数
		cmd.push_back(ss.str());
	};

	auto add_param_hex = [&cmd](std::string_view key, uint32_t value) {
		std::ostringstream ss;
		ss << key << std::hex << value << std::dec; // fold expression，逐个拼接参数
		cmd.push_back(ss.str());
	};

#define VALIDATE_STRING(s)       \
    do                           \
    {                            \
        if (!validate_string(s)) \
        {                        \
            goto end;            \
        }                        \
    } while (0)

	add_param_hex("scid=", params.scid);
	add_param("log_level=", log_level_to_server_string(params.log_level));

	if (!params.video) {
		add_param("video=false");
	}
	if (params.video_bit_rate) {
		add_param("video_bit_rate=", params.video_bit_rate);
	}
	if (!params.audio) {
		add_param("audio=false");
	}
	if (params.audio_bit_rate) {
		add_param("audio_bit_rate=", params.audio_bit_rate);
	}
	if (params.video_codec != SC_CODEC_H264) {
		add_param("video_codec=", sc_server_get_codec_name(params.video_codec));
	}
	if (params.audio_codec != SC_CODEC_OPUS) {
		add_param("audio_codec=", sc_server_get_codec_name(params.audio_codec));
	}
	if (params.video_source != SC_VIDEO_SOURCE_DISPLAY) {
		assert(params.video_source == SC_VIDEO_SOURCE_CAMERA);
		add_param("video_source=camera");
	}
	// If audio is enabled, an "auto" audio source must have been resolved
	assert(params.audio_source != SC_AUDIO_SOURCE_AUTO || !params.audio);
	if (params.audio_source != SC_AUDIO_SOURCE_OUTPUT && params.audio) {
		add_param("audio_source=", sc_server_get_audio_source_name(params.audio_source));
	}
	if (params.audio_dup) {
		add_param("audio_dup=true");
	}
	if (params.max_size) {
		add_param("max_size=", params.max_size);
	}
	if (params.max_fps) {
		VALIDATE_STRING(params.max_fps);
		add_param("max_fps=", params.max_fps);
	}
	if (params.angle) {
		VALIDATE_STRING(params.angle);
		add_param("angle=", params.angle);
	}
	if (params.capture_orientation_lock != SC_ORIENTATION_UNLOCKED ||
	    params.capture_orientation != SC_ORIENTATION_0) {
		if (params.capture_orientation_lock == SC_ORIENTATION_LOCKED_INITIAL) {
			add_param("capture_orientation=@");
		} else {
			const char *orient = sc_orientation_get_name(params.capture_orientation);
			bool locked = params.capture_orientation_lock != SC_ORIENTATION_UNLOCKED;
			add_param("capture_orientation=", locked ? "@" : "", orient);
		}
	}
	if (this->m_tunnel.m_forward) {
		add_param("tunnel_forward=true");
	}
	if (params.crop) {
		VALIDATE_STRING(params.crop);
		add_param("crop=%s", params.crop);
	}
	if (!params.control) {
		// By default, control is true
		add_param("control=false");
	}
	if (params.display_id) {
		add_param("display_id=", params.display_id);
	}
	if (params.camera_id) {
		VALIDATE_STRING(params.camera_id);
		add_param("camera_id=", params.camera_id);
	}
	if (params.camera_size) {
		VALIDATE_STRING(params.camera_size);
		add_param("camera_size=", params.camera_size);
	}
	if (params.camera_facing != SC_CAMERA_FACING_ANY) {
		add_param("camera_facing=", sc_server_get_camera_facing_name(params.camera_facing));
	}
	if (params.camera_ar) {
		VALIDATE_STRING(params.camera_ar);
		add_param("camera_ar=", params.camera_ar);
	}
	if (params.camera_fps) {
		add_param("camera_fps=", params.camera_fps);
	}
	if (params.camera_high_speed) {
		add_param("camera_high_speed=true");
	}
	if (params.show_touches) {
		add_param("show_touches=true");
	}
	if (params.stay_awake) {
		add_param("stay_awake=true");
	}
	if (params.screen_off_timeout != -1) {
		assert(params.screen_off_timeout >= 0);
		uint64_t ms = SC_TICK_TO_MS(params.screen_off_timeout);
		add_param("screen_off_timeout=", ms);
	}
	if (params.video_codec_options) {
		VALIDATE_STRING(params.video_codec_options);
		add_param("video_codec_options=", params.video_codec_options);
	}
	if (params.audio_codec_options) {
		VALIDATE_STRING(params.audio_codec_options);
		add_param("audio_codec_options=", params.audio_codec_options);
	}
	if (params.video_encoder) {
		VALIDATE_STRING(params.video_encoder);
		add_param("video_encoder=", params.video_encoder);
	}
	if (params.audio_encoder) {
		VALIDATE_STRING(params.audio_encoder);
		add_param("audio_encoder=", params.audio_encoder);
	}
	if (params.power_off_on_close) {
		add_param("power_off_on_close=true");
	}
	if (!params.clipboard_autosync) {
		// By default, clipboard_autosync is true
		add_param("clipboard_autosync=false");
	}
	if (!params.downsize_on_error) {
		// By default, downsize_on_error is true
		add_param("downsize_on_error=false");
	}
	if (!params.cleanup) {
		// By default, cleanup is true
		add_param("cleanup=false");
	}
	if (!params.power_on) {
		// By default, power_on is true
		add_param("power_on=false");
	}
	if (params.new_display) {
		VALIDATE_STRING(params.new_display);
		add_param("new_display=", params.new_display);
	}
	if (params.display_ime_policy != SC_DISPLAY_IME_POLICY_UNDEFINED) {
		add_param("display_ime_policy=", sc_server_get_display_ime_policy_name(params.display_ime_policy));
	}
	if (!params.vd_destroy_content) {
		add_param("vd_destroy_content=false");
	}
	if (!params.vd_system_decorations) {
		add_param("vd_system_decorations=false");
	}
	if (params.list & SC_OPTION_LIST_ENCODERS) {
		add_param("list_encoders=true");
	}
	if (params.list & SC_OPTION_LIST_DISPLAYS) {
		add_param("list_displays=true");
	}
	if (params.list & SC_OPTION_LIST_CAMERAS) {
		add_param("list_cameras=true");
	}
	if (params.list & SC_OPTION_LIST_CAMERA_SIZES) {
		add_param("list_camera_sizes=true");
	}
	if (params.list & SC_OPTION_LIST_APPS) {
		add_param("list_apps=true");
	}

#ifdef SERVER_DEBUGGER
	LOGI("Server debugger listening%s...", sdk_version < 30 ? " on port " SERVER_DEBUGGER_PORT : "");
	// For Android < 11, from the computer:
	//     - run `adb forward tcp:5005 tcp:5005`
	// For Android >= 11:
	//     - execute `adb jdwp` to get the jdwp port
	//     - run `adb forward tcp:5005 jdwp:XXXX` (replace XXXX)
	//
	// Then, from Android Studio: Run > Debug > Edit configurations...
	// On the left, click on '+', "Remote", with:
	//     Host: localhost
	//     Port: 5005
	// Then click on "Debug"
#endif
	// Inherit both stdout and stderr (all server logs are printed to stdout)
	pid = sc_adb_execute(cmd, 0);

end:

	return pid;
}

bool sc_server::sc_server_connect_to(sc_server_info &info)
{
	// struct sc_adb_tunnel* tunnel = &server->tunnel;

	assert(this->m_tunnel.m_enabled);

	const char *serial = this->m_serial.c_str();
	assert(serial);

	bool video = this->m_params.video;
	bool audio = this->m_params.audio;
	bool control = this->m_params.control;

	sc_socket video_socket = SC_SOCKET_NONE;
	sc_socket audio_socket = SC_SOCKET_NONE;
	sc_socket control_socket = SC_SOCKET_NONE;
	if (!this->m_tunnel.m_forward) {
		if (video) {
			video_socket = this->m_intr.net_accept_intr(this->m_tunnel.m_server_socket);
			if (video_socket == SC_SOCKET_NONE) {
				goto fail;
			}
		}

		if (audio) {
			audio_socket = this->m_intr.net_accept_intr(this->m_tunnel.m_server_socket);
			if (audio_socket == SC_SOCKET_NONE) {
				goto fail;
			}
		}

		if (control) {
			control_socket = this->m_intr.net_accept_intr(this->m_tunnel.m_server_socket);
			if (control_socket == SC_SOCKET_NONE) {
				goto fail;
			}
		}
	} else {
		uint32_t tunnel_host = this->m_params.tunnel_host;
		if (!tunnel_host) {
			tunnel_host = IPV4_LOCALHOST;
		}

		uint16_t tunnel_port = this->m_params.tunnel_port;
		if (!tunnel_port) {
			tunnel_port = this->m_tunnel.m_local_port;
		}

		unsigned attempts = 100;
		sc_tick delay = SC_TICK_FROM_MS(100);
		sc_socket first_socket = this->connect_to_server(attempts, delay, tunnel_host, tunnel_port);
		if (first_socket == SC_SOCKET_NONE) {
			goto fail;
		}

		if (video) {
			video_socket = first_socket;
		}

		if (audio) {
			if (!video) {
				audio_socket = first_socket;
			} else {
				audio_socket = net_socket();
				if (audio_socket == SC_SOCKET_NONE) {
					goto fail;
				}
				bool ok = this->m_intr.net_connect_intr(audio_socket, tunnel_host, tunnel_port);
				if (!ok) {
					goto fail;
				}
			}
		}

		if (control) {
			if (!video && !audio) {
				control_socket = first_socket;
			} else {
				control_socket = net_socket();
				if (control_socket == SC_SOCKET_NONE) {
					goto fail;
				}
				bool ok = this->m_intr.net_connect_intr(control_socket, tunnel_host, tunnel_port);
				if (!ok) {
					goto fail;
				}
			}
		}
	}

	if (control_socket != SC_SOCKET_NONE) {
		// Disable Nagle's algorithm for the control socket
		// (it only impacts the sending side, so it is useless to set it
		// for the other sockets)
		bool ok = net_set_tcp_nodelay(control_socket, true);
		(void)ok; // error already logged
	}

	// we don't need the adb tunnel anymore
	this->m_tunnel.sc_adb_tunnel_close(this->m_intr, serial, this->m_device_socket_name);

	sc_socket first_socket = video ? video_socket : audio ? audio_socket : control_socket;

	// The sockets will be closed on stop if device_read_info() fails
	bool ok = device_read_info(this->m_intr, first_socket, info);
	if (!ok) {
		goto fail;
	}

	assert(!video || video_socket != SC_SOCKET_NONE);
	assert(!audio || audio_socket != SC_SOCKET_NONE);
	assert(!control || control_socket != SC_SOCKET_NONE);

	this->m_video_socket = video_socket;
	this->m_audio_socket = audio_socket;
	this->m_control_socket = control_socket;

	return true;

fail:
	if (video_socket != SC_SOCKET_NONE) {
		if (!net_close(video_socket)) {
			//LOGW("Could not close video socket");
		}
	}

	if (audio_socket != SC_SOCKET_NONE) {
		if (!net_close(audio_socket)) {
			//LOGW("Could not close audio socket");
		}
	}

	if (control_socket != SC_SOCKET_NONE) {
		if (!net_close(control_socket)) {
			//LOGW("Could not close control socket");
		}
	}

	if (this->m_tunnel.m_enabled) {
		// Always leave this function with tunnel disabled
		this->m_tunnel.sc_adb_tunnel_close(this->m_intr, serial, this->m_device_socket_name);
	}

	return false;
}

sc_socket sc_server::connect_to_server(unsigned attempts, sc_tick delay, uint32_t host, uint16_t port)
{
	do {
		//LOGD("Remaining connection attempts: %u", attempts);
		sc_socket socket = net_socket();
		if (socket != SC_SOCKET_NONE) {
			bool ok = connect_and_read_byte(this->m_intr, socket, host, port);
			if (ok) {
				// it worked!
				return socket;
			}

			net_close(socket);
		}

		if (this->m_intr.is_interrupted()) {
			// Stop immediately
			break;
		}

		if (attempts) {
			sc_tick deadline = sc_tick_now() + delay;
			bool ok = this->sc_server_sleep(deadline);
			if (!ok) {
				//LOGI("Connection attempt stopped");
				break;
			}
		}
	} while (--attempts);
	return SC_SOCKET_NONE;
}

bool sc_server::sc_server_sleep(sc_tick deadline)
{
	std::unique_lock<sc_mutex> lock(this->m_mutex);
	while (!this->m_stopped) {
		if (!sc_cond_timedwait(this->m_cond_stopped, this->m_mutex, deadline)) {
			// 超时
			return true; // 仍然在 sleep 状态
		}
	}
	// 被 stop 唤醒
	return false;
}

void sc_server::sc_server_kill_adb_if_requested()
{
	if (this->m_params.kill_adb_on_close) {
		//LOGI("Killing adb server...");
		unsigned flags = SC_ADB_NO_STDOUT | SC_ADB_NO_STDERR | SC_ADB_NO_LOGERR;
		sc_adb_kill_server(this->m_intr, flags);
	}
}

int sc_server::run_server(void *data)
{
	auto *server = static_cast<sc_server *>(data);
	const auto &params = server->m_params;

	// 1. 启动 adb server
	if (!sc_adb_start_server(server->m_intr, 0)) {
		server->sc_server_kill_adb_if_requested();
		server->m_cbs->on_connection_failed(*server, server->m_cbs_userdata);
		return -1;
	}

	// 2. 选择设备
	sc_adb_device_selector selector{};
	if (const char *env_serial = getenv("ANDROID_SERIAL")) {
		selector.type = SC_ADB_DEVICE_SELECT_SERIAL;
		selector.serial = env_serial;
	} else {
		selector.type = SC_ADB_DEVICE_SELECT_ALL;
	}

	sc_adb_device device{};
	if (!sc_adb_select_device(server->m_intr, selector, 0, device)) {
		server->sc_server_kill_adb_if_requested();
		server->m_cbs->on_connection_failed(*server, server->m_cbs_userdata);
		return -1;
	}

	// 3. 确定 serial
	if (!params.tcpip) {
		server->m_serial = std::move(device.serial);
	}
	const std::string serial = server->m_serial;
	if (serial.empty()) {
		server->m_cbs->on_connection_failed(*server, server->m_cbs_userdata);
		return -1;
	}

	// 4. push server
	if (!push_server(server->m_intr, serial)) {
		server->m_cbs->on_connection_failed(*server, server->m_cbs_userdata);
		return -1;
	}
	auto scid_hex = [](uint32_t scid) {
		std::ostringstream oss;
		oss << std::hex << scid; // 小写 hex，和官方一致
		return oss.str();
	};

	// 5. socket name
	server->m_device_socket_name = SC_SOCKET_NAME_PREFIX + scid_hex(params.scid);

	// 6. 打开 adb tunnel（RAII：失败立即关闭）
	if (!server->m_tunnel.adb_tunnel_open(server->m_intr, serial, server->m_device_socket_name, params.port_range,
					      params.force_adb_forward)) {
		server->m_cbs->on_connection_failed(*server, server->m_cbs_userdata);
		return -1;
	}

	// 7. 启动 server 进程
	sc_pid pid = server->execute_server(params);
	if (pid == SC_PROCESS_NONE) {
		server->m_tunnel.sc_adb_tunnel_close(server->m_intr, serial, server->m_device_socket_name);
		server->m_cbs->on_connection_failed(*server, server->m_cbs_userdata);
		return -1;
	}

	// 8. 注册进程终止回调
	server->m_listener.on_terminated = [server]() {
		server->m_intr.intr_interrupt();
		if (server->m_cbs) {
			server->m_cbs->on_disconnected(*server, server->m_cbs_userdata);
		}
	};

	try {
		// 9. observer（RAII）
		process_observer_raii observer(pid, server->m_listener, server);

		// 10. 连接 server socket
		if (!server->sc_server_connect_to(server->m_info)) {
			sc_process_terminate(pid);
			sc_process_wait(pid, true);
			server->m_cbs->on_connection_failed(*server, server->m_cbs_userdata);
			return -1;
		}

		// 11. 已连接
		server->m_cbs->on_connected(*server, server->m_cbs_userdata);

		// 12. 等待 stop
		{
			std::lock_guard<sc_mutex> lock(server->m_mutex);
			while (!server->m_stopped) {
				sc_cond_wait(server->m_cond_stopped, server->m_mutex);
			}
		}

		// 13. 中断 socket
		if (server->m_video_socket != SC_SOCKET_NONE)
			net_interrupt(server->m_video_socket);
		if (server->m_audio_socket != SC_SOCKET_NONE)
			net_interrupt(server->m_audio_socket);
		if (server->m_control_socket != SC_SOCKET_NONE)
			net_interrupt(server->m_control_socket);

		// 14. 等待 server 退出
		sc_tick deadline = sc_tick_now() + SC_TICK_FROM_SEC(1);
		if (!observer.timedwait(deadline)) {
			sc_process_terminate(pid);
		}

		sc_process_close(pid);
		server->sc_server_kill_adb_if_requested();
		return 0;
	} catch (const std::exception &) {
		sc_process_terminate(pid);
		sc_process_wait(pid, true);
		server->m_cbs->on_connection_failed(*server, server->m_cbs_userdata);
		return -1;
	}
}
