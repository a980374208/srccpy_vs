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
    memset(pm, 0, sizeof(pm));
	pm->scid = scid;
	pm->req_serial = nullptr;

    

    pm->log_level = SC_LOG_LEVEL_DEBUG;
    pm->video_codec = SC_CODEC_H264;
    pm->audio_codec = SC_CODEC_AAC;
    pm->video_source = SC_VIDEO_SOURCE_DISPLAY;
    pm->audio_source = SC_AUDIO_SOURCE_MIC;
    pm->camera_facing = SC_CAMERA_FACING_FRONT;
    pm->crop = nullptr;
	pm->video_codec_options = nullptr;
	pm->audio_codec_options = nullptr;
	pm->video_encoder = nullptr;
	pm->audio_encoder = nullptr;
	pm->camera_id = nullptr;
	pm->camera_size = nullptr;
	pm->camera_ar = nullptr;
	pm->camera_fps = 0;
	pm->port_range.first = 27183;
	pm->port_range.last = 27199;
	pm->tunnel_host = 0;
	pm->tunnel_port = 0;
	pm->max_size = 0;
	pm->video_bit_rate = 0;
	pm->audio_bit_rate = 0;
	pm->max_fps = nullptr;
	pm->angle = nullptr;
    pm->screen_off_timeout = -1;
	pm->capture_orientation = SC_ORIENTATION_0;
	pm->capture_orientation_lock = SC_ORIENTATION_UNLOCKED;
	pm->control = false;
	pm->display_id = 0;
	pm->new_display = nullptr;
	pm->display_ime_policy = SC_DISPLAY_IME_POLICY_UNDEFINED;   
	pm->video = true;
	pm->audio = false;
	pm->audio_dup = false;
	pm->show_touches = false;
	pm->stay_awake = false;
	pm->force_adb_forward = false;
	pm->power_off_on_close = false;
	pm->clipboard_autosync = true;
	pm->downsize_on_error = true;
	pm->tcpip = false;
	pm->tcpip_dst = nullptr;
	pm->select_usb = false;
	pm->cleanup = true;
	pm->power_on = true;
	pm->kill_adb_on_close = false;
	pm->camera_high_speed = false;
	pm->vd_destroy_content = true;
	pm->list = 0;


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
