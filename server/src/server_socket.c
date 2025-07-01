#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "server_socket.h"
#include "config.h"

// Setup une socket serveur UDP
int setup_server_socket(void)
{
    // Création d'une socket UDP en IPv4
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return -1;
    }
    
    // Taille du buffer de réception (256Mo est adapté pour un flux d'image)
    int rcvbuf = 256 * 1024 * 1024;

    // On configure le buffer de réception de la socket
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    
    // Permettre la réutilisation de l'adresse immédiatement après la fermeture
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // Configuration de l'adresse du serveur
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr = { .s_addr = INADDR_ANY },
        .sin_port = htons(PORT)
    };

    // On lie la socket à l'adresse et au port
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(sock);
        return -1;
    }

    // On rettourne la socket
    return sock;
}
