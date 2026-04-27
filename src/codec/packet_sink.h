#pragma once
#include <libavcodec/avcodec.h>

struct sc_packet_sink_ops;
struct sc_packet_sink {
    const struct sc_packet_sink_ops* ops;
};

struct sc_packet_sink_ops {
    /* The codec context is valid until the sink is closed */
    bool (*open)(struct sc_packet_sink* sink, AVCodecContext* ctx);
    void (*close)(struct sc_packet_sink* sink);
    bool (*push)(struct sc_packet_sink* sink, const AVPacket* packet);

    /*/
     * Called when the input stream has been disabled at runtime.
     *
     * If it is called, then open(), close() and push() will never be called.
     *
     * It is useful to notify the recorder that the requested audio stream has
     * finally been disabled because the device could not capture it.
     */
    void (*disable)(struct sc_packet_sink* sink);
};