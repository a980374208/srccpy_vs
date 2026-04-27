#pragma once
#include "packet_sink.h"

#define SC_PACKET_SOURCE_MAX_SINKS 2

class sc_packet_source {
    struct sc_packet_sink* sinks[SC_PACKET_SOURCE_MAX_SINKS];
    unsigned sink_count;
};
