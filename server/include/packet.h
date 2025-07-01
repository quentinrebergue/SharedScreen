#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>

struct packet_header {
    uint32_t image_id;
    uint32_t seq;
    uint32_t total_packets;
    uint32_t width;
    uint32_t height;
};

#endif // PACKET_H