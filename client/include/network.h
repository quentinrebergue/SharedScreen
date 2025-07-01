#ifndef NETWORK_H
#define NETWORK_H

#include <netinet/in.h>
#include <liburing.h>
#include "screenshot.h"
#include <stdint.h>

#define PIXEL_BYTES 4
#define PACKET_SIZE 1000
#define SERVER_ADDR "192.168.1.241"
#define SERVER_PORT 8080


struct __attribute__((packed)) packet_header {
    uint32_t image_id;
    uint32_t seq;
    uint32_t total_packets;
    uint32_t width;
    uint32_t height;
};

int setup_socket(struct sockaddr_in *dest);

void send_image_data(int sock, struct io_uring *ring, struct screen_data *SD, const struct sockaddr_in *dest);

#endif // NETWORK_H
