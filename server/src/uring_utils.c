#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <liburing.h>
#include <sys/socket.h>
#include <time.h>
#include "uring_utils.h"
#include "config.h"
#include "reception.h"

extern volatile int running;

// Initialise les requêtes de réception dans io_uring
void prime_uring_requests(struct io_uring *ring, int sock)
{
    // Prépare les buffers de réception
    for (int i = 0; i < RECV_BUFFERS; i++)
    {
        // Alloue un buffer pour recevoir les paquets
        char *buf = malloc(PACKET_SIZE);
        if (!buf)
        {
            break;
        }

        // Récupère un SQE (Submission Queue Entry) de io_uring
        struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

        if (!sqe)
        {
            free(buf);
            break;
        }
        
        // Prépare la requête de réception
        io_uring_prep_recv(sqe, sock, buf, PACKET_SIZE, 0);
        // Associe le buffer à la SQE pour le libérer plus tard
        io_uring_sqe_set_data(sqe, buf);
    }
    // Soumet les requêtes à io_uring
    io_uring_submit(ring);
    printf("Waiting for packets...\n");
}

// Boucle principale du serveur, gérant la réception et le timeout
void run_server_loop(struct io_uring *ring, int sock)
{

    while (running)
    {
        struct io_uring_cqe *cqe;
        
        // Timeout pour permettre à la boucle de vérifier l'inactivité
        struct __kernel_timespec ts = {
            .tv_sec = 1,
            .tv_nsec = 0
        };

        // Attendre une complétion avec timeout
        int ret = io_uring_wait_cqe_timeout(ring, &cqe, &ts);

        // Si l'état de réception est actif et que le délai d'inactivité est dépassé
        if (rx_state.active && (time(NULL) - rx_state.last_activity) >= SHUTDOWN_TIMEOUT)
        {
            printf("Inactivity detected. Saving and shutting down.\n");

            // Sauvegarde l'image reçue
            save_image();

            // On arrête le serveur
            running = 0;
            continue;
        }

        // Si le wait a expiré, on continue la boucle
        if (ret == -ETIME)
        {
            continue;
        }
        if (ret < 0)
        {
            if (ret != -EINTR)
            {
                perror("io_uring_wait");
                break;
            }
        }
        char *buf = io_uring_cqe_get_data(cqe);
        if (cqe->res > 0) process_packet(buf, cqe->res);

        struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
        io_uring_prep_recv(sqe, sock, buf, PACKET_SIZE, 0);
        io_uring_sqe_set_data(sqe, buf);
        io_uring_submit(ring);
        
        io_uring_cqe_seen(ring, cqe);
    }
}
