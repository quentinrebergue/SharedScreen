#ifndef URING_UTILS_H
#define URING_UTILS_H

#include <liburing.h>

void prime_uring_requests(struct io_uring *ring, int sock);
void run_server_loop(struct io_uring *ring, int sock);

#endif // URING_UTILS_H
