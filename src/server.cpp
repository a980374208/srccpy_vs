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

static const char*
log_level_to_server_string(enum sc_log_level level)
{
    switch (level)
    {
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

static const char*
sc_server_get_codec_name(enum sc_codec codec)
{
    switch (codec)
    {
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

static const char*
sc_server_get_camera_facing_name(enum sc_camera_facing camera_facing)
{
    switch (camera_facing)
    {
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

static const char*
sc_server_get_audio_source_name(enum sc_audio_source audio_source)
{
    switch (audio_source)
    {
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

static const char*
sc_server_get_display_ime_policy_name(enum sc_display_ime_policy policy)
{
    switch (policy)
    {
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

sc_server::sc_server() : m_serial(""),
						 m_device_socket_name(""),
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

bool sc_server::push_server(sc_intr& intr, const std::string& serial)
{
	std::string server_path = get_server_path();
	if (server_path.empty())
	{
		return false;
	}
	if (!sc_file_is_regular(server_path))
	{
		//LOGE("'%s' does not exist or is not a regular file\n", server_path);
		return false;
	}
	bool ok = sc_adb_push(intr, serial, server_path, SC_DEVICE_SERVER_PATH, 0);
	return ok;
}

std::string sc_server::get_server_path()
{
	const char* env_path = std::getenv("SCRCPY_SERVER_PATH");
	
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

static inline bool validate_string(const std::string& s)
{
    // The parameters values are passed as command line arguments to adb, so
    // they must either be properly escaped, or they must not contain any
    // special shell characters.
    // Since they are not properly escaped on Windows anyway (see
    // sys/win/process.c), just forbid special shell characters.
    static const char* invalid = " ;'\"*$?&`#\\|<>[]{}()!~\r\n";
    if (s.find_first_of(invalid) != std::string::npos)
    {
        //LOGE("Invalid server param: [%s]", s.c_str());
        return false;
    }
    return true;
}

sc_pid sc_server::execute_server(const sc_server_params& params)
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
    if (!sdk_version)
    {
        LOGE("Could not determine SDK version");
        return 0;
    }

#define SERVER_DEBUGGER_PORT "5005"
    const char* dbg;
    if (sdk_version < 28)
    {
        // Android < 9
        dbg = "-agentlib:jdwp=transport=dt_socket,suspend=y,server=y,address=" SERVER_DEBUGGER_PORT;
    }
    else if (sdk_version < 30)
    {
        // Android >= 9 && Android < 11
        dbg = "-XjdwpProvider:internal -XjdwpOptions:transport=dt_socket,"
            "suspend=y,server=y,address=" SERVER_DEBUGGER_PORT;
    }
    else
    {
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
    auto add_param = [&cmd](auto&&... args) {
        std::ostringstream ss;
        (ss << ... << args);  // fold expression，逐个拼接参数
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

    add_param("scid=", params.scid);
    add_param("log_level=", log_level_to_server_string(params.log_level));

    if (!params.video)
    {
        add_param("video=false");
    }
    if (params.video_bit_rate)
    {
        add_param("video_bit_rate=", params.video_bit_rate);
    }
    if (!params.audio)
    {
        add_param("audio=false");
    }
    if (params.audio_bit_rate)
    {
        add_param("audio_bit_rate=", params.audio_bit_rate);
    }
    if (params.video_codec != SC_CODEC_H264)
    {
        add_param("video_codec=",
            sc_server_get_codec_name(params.video_codec));
    }
    if (params.audio_codec != SC_CODEC_OPUS)
    {
        add_param("audio_codec=",
            sc_server_get_codec_name(params.audio_codec));
    }
    if (params.video_source != SC_VIDEO_SOURCE_DISPLAY)
    {
        assert(params.video_source == SC_VIDEO_SOURCE_CAMERA);
        add_param("video_source=camera");
    }
    // If audio is enabled, an "auto" audio source must have been resolved
    assert(params.audio_source != SC_AUDIO_SOURCE_AUTO || !params.audio);
    if (params.audio_source != SC_AUDIO_SOURCE_OUTPUT && params.audio)
    {
        add_param("audio_source=",
            sc_server_get_audio_source_name(params.audio_source));
    }
    if (params.audio_dup)
    {
        add_param("audio_dup=true");
    }
    if (params.max_size)
    {
        add_param("max_size=", params.max_size);
    }
    if (params.max_fps)
    {
        VALIDATE_STRING(params.max_fps);
        add_param("max_fps=", params.max_fps);
    }
    if (params.angle)
    {
        VALIDATE_STRING(params.angle);
        add_param("angle=", params.angle);
    }
    if (params.capture_orientation_lock != SC_ORIENTATION_UNLOCKED || params.capture_orientation != SC_ORIENTATION_0)
    {
        if (params.capture_orientation_lock == SC_ORIENTATION_LOCKED_INITIAL)
        {
            add_param("capture_orientation=@");
        }
        else
        {
            const char* orient =
                sc_orientation_get_name(params.capture_orientation);
            bool locked =
                params.capture_orientation_lock != SC_ORIENTATION_UNLOCKED;
            add_param("capture_orientation=", locked ? "@" : "", orient);
        }
    }
    if (this->m_tunnel.m_forward)
    {
        add_param("tunnel_forward=true");
    }
    if (params.crop)
    {
        VALIDATE_STRING(params.crop);
        add_param("crop=%s", params.crop);
    }
    if (!params.control)
    {
        // By default, control is true
        add_param("control=false");
    }
    if (params.display_id)
    {
        add_param("display_id=", params.display_id);
    }
    if (params.camera_id)
    {
        VALIDATE_STRING(params.camera_id);
        add_param("camera_id=", params.camera_id);
    }
    if (params.camera_size)
    {
        VALIDATE_STRING(params.camera_size);
        add_param("camera_size=", params.camera_size);
    }
    if (params.camera_facing != SC_CAMERA_FACING_ANY)
    {
        add_param("camera_facing=",
            sc_server_get_camera_facing_name(params.camera_facing));
    }
    if (params.camera_ar)
    {
        VALIDATE_STRING(params.camera_ar);
        add_param("camera_ar=", params.camera_ar);
    }
    if (params.camera_fps)
    {
        add_param("camera_fps=", params.camera_fps);
    }
    if (params.camera_high_speed)
    {
        add_param("camera_high_speed=true");
    }
    if (params.show_touches)
    {
        add_param("show_touches=true");
    }
    if (params.stay_awake)
    {
        add_param("stay_awake=true");
    }
    if (params.screen_off_timeout != -1)
    {
        assert(params.screen_off_timeout >= 0);
        uint64_t ms = SC_TICK_TO_MS(params.screen_off_timeout);
        add_param("screen_off_timeout=", ms);
    }
    if (params.video_codec_options)
    {
        VALIDATE_STRING(params.video_codec_options);
        add_param("video_codec_options=", params.video_codec_options);
    }
    if (params.audio_codec_options)
    {
        VALIDATE_STRING(params.audio_codec_options);
        add_param("audio_codec_options=", params.audio_codec_options);
    }
    if (params.video_encoder)
    {
        VALIDATE_STRING(params.video_encoder);
        add_param("video_encoder=", params.video_encoder);
    }
    if (params.audio_encoder)
    {
        VALIDATE_STRING(params.audio_encoder);
        add_param("audio_encoder=", params.audio_encoder);
    }
    if (params.power_off_on_close)
    {
        add_param("power_off_on_close=true");
    }
    if (!params.clipboard_autosync)
    {
        // By default, clipboard_autosync is true
        add_param("clipboard_autosync=false");
    }
    if (!params.downsize_on_error)
    {
        // By default, downsize_on_error is true
        add_param("downsize_on_error=false");
    }
    if (!params.cleanup)
    {
        // By default, cleanup is true
        add_param("cleanup=false");
    }
    if (!params.power_on)
    {
        // By default, power_on is true
        add_param("power_on=false");
    }
    if (params.new_display)
    {
        VALIDATE_STRING(params.new_display);
        add_param("new_display=", params.new_display);
    }
    if (params.display_ime_policy != SC_DISPLAY_IME_POLICY_UNDEFINED)
    {
        add_param("display_ime_policy=",
            sc_server_get_display_ime_policy_name(params.display_ime_policy));
    }
    if (!params.vd_destroy_content)
    {
        add_param("vd_destroy_content=false");
    }
    if (!params.vd_system_decorations)
    {
        add_param("vd_system_decorations=false");
    }
    if (params.list & SC_OPTION_LIST_ENCODERS)
    {
        add_param("list_encoders=true");
    }
    if (params.list & SC_OPTION_LIST_DISPLAYS)
    {
        add_param("list_displays=true");
    }
    if (params.list & SC_OPTION_LIST_CAMERAS)
    {
        add_param("list_cameras=true");
    }
    if (params.list & SC_OPTION_LIST_CAMERA_SIZES)
    {
        add_param("list_camera_sizes=true");
    }
    if (params.list & SC_OPTION_LIST_APPS)
    {
        add_param("list_apps=true");
    }

#ifdef SERVER_DEBUGGER
    LOGI("Server debugger listening%s...",
        sdk_version < 30 ? " on port " SERVER_DEBUGGER_PORT : "");
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


int sc_server::run_server(void *data)
{
	sc_server* server = (sc_server*)data;

	const struct sc_server_params& params = server->m_params;

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
	ok = sc_adb_select_device(server->m_intr, selector, 0, device);

	if (!ok)
	{
		//goto error_connection_failed;
	}

	//if (params.tcpip)
	//{
		//assert(!params.tcpip_dst);
		/*ok = sc_server_configure_tcpip_unknown_address(server,
			device.serial);
		if (!ok)
		{
			goto error_connection_failed;
		}*/
	//	assert(!server->m_serial.empty());
	//}
	//else
	//{
		server->m_serial = std::move(device.serial);
	//}

	const std::string serial = server->m_serial;
	assert(!serial.empty());
	//LOGD("Device serial: %s", serial);
	// 把运行手机屏幕、摄像头捕获等需要在android端执行的可执行程序推送到android的临时目录
	ok = push_server(server->m_intr, serial);
	if (!ok)
	{
		//goto error_connection_failed;
	}
	std::ostringstream oss;
	oss << SC_SOCKET_NAME_PREFIX << params.scid;
	server->m_device_socket_name = oss.str();
	assert(!server->m_device_socket_name.empty());

	ok = server->m_tunnel.adb_tunnel_open(server->m_intr, serial,
		server->m_device_socket_name, params.port_range,
		params.force_adb_forward);

	if (!ok)
	{
		//goto error_connection_failed;
	}

	// server will connect to our server socket
	sc_pid pid = server->execute_server( params);
	if (pid == SC_PROCESS_NONE)
	{
		/*sc_adb_tunnel_close(&server->m_tunnel, &server->m_intr, serial,
			server->m_device_socket_name);
		goto error_connection_failed;*/
	}



	return 0;
}
