#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <liburing.h>

#include "config.h"
#include "server_socket.h"
#include "uring_utils.h"
#include "reception.h"

volatile int running = 1;


int main(void)
{
    // Setup de la socket du serveur
    int sock = setup_server_socket();
    
    // Si la socket n'a pas pu être créée, on quitte
    if (sock < 0)
    {
        return 1;
    }

    struct io_uring ring;

    // Initialisation de io_uring
    if (io_uring_queue_init(RECV_BUFFERS + 1, &ring, 0) < 0) {
        perror("io_uring_queue_init");
        close(sock);
        return 1;
    }

    printf("Serveur UDP démarré sur le port %d. Arrêt auto après %d sec d'inactivité.\n", PORT, SHUTDOWN_TIMEOUT);

    // Initialise les requêtes de réception dans io_uring
    prime_uring_requests(&ring, sock);

    // Boucle principale du serveur, gérant la réception et le timeout
    run_server_loop(&ring, sock);

    printf("Arrêt du serveur...\n");

    // Réinitialise l'état de réception et free les ressources
    reset_reception_state();
    io_uring_queue_exit(&ring);
    close(sock);

    return 0;
}