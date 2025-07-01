#include "network.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int setup_socket(struct sockaddr_in *dest)
{
    // Creation d'une socket UDP en IPv4
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0)
    {
        perror("socket"); 
        return -1;
    }

    // Mise à 0 de la structure d'adresse de destination
    memset(dest, 0, sizeof(*dest));

    // L'adresse est en IPv4, sur le port SERVER_PORT = 8080
    dest->sin_family = AF_INET;
    dest->sin_port   = htons(SERVER_PORT);

    // Conversion de l'adresse IP en format binaire
    if (inet_pton(AF_INET, SERVER_ADDR, &dest->sin_addr) != 1)
    {
        perror("inet_pton"); close(sock); return -1;
    }

    // Taille du buffer d'envoi (32Mo est adapte pour un flux d'image)
    int sndbuf = 32*1024*1024;

    // On configure le buffer d'envoi de la socket
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    return sock;
}

void send_image_data(int sock, struct io_uring *ring, struct screen_data* sd,
const struct sockaddr_in *dest)
{
    // Taille du header de notre paquet
    size_t header_size = sizeof(struct packet_header);
    
    // Taille du payload de notre paquet
    size_t payload_size = PACKET_SIZE - header_size;

    // Nombre total de paquets à envoyer
    size_t total = (sd->length + payload_size - 1) / payload_size;

    // La sequence de l'image
    size_t seq = 0;

    // Nombre de paquets en cours d'envoi
    int inflight = 0;

    // Envoi des paquets jusqu'à ce que tous soient traités
    while (seq < total)
    {
        // Tant qu'il y a moins de 32 paquets en cours d'envoi et
        // qu'il reste des paquets à envoyer
        while (inflight < 32 && seq < total)
        {
            // On traite un nouveau paquet en récupérant un SQE de io_uring
            struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

            // Si on n'a plus de SQE, on sort de la boucle
            if (!sqe)
            {
                break;
            }

            // On prépare le header du paquet
            struct packet_header header = {
                .image_id = htonl(0),
                .seq = htonl(seq),
                .total_packets = htonl(total),
                .width = htonl(sd->width),
                .height = htonl(sd->height)
            };

            char *packet = malloc(PACKET_SIZE);

            if (!packet)
            {
                continue;
            }

            // On copie le header dans le paquet
            memcpy(packet, &header, header_size);

            // Calcul la position de la sequence dans l'image
            size_t offset = (size_t)seq * payload_size;

            // On calcule la taille des données à copier dans ce paquet
            // Si on dépasse la fin de l'image, on réduit la taille copiée
            size_t copy_size = payload_size;
            if (offset + copy_size > sd->length)
            {
                copy_size = sd->length - offset;
            }
            
            // On copie les donnees de la sequence de l'image dans le paquet
            memcpy(packet + header_size, sd->data + offset, copy_size);
            
            // Contenu à envoyer par io_uring
            struct iovec iov = {
                .iov_base = packet,
                .iov_len = header_size + copy_size
            };
            
            // Structure du paquet UDP à envoyer par io_uring
            struct msghdr msgh = {
                .msg_name = (void*)dest,
                .msg_namelen = sizeof(*dest),
                .msg_iov = &iov,
                .msg_iovlen = 1
            };

            // On prépare l'envoi du paquet UDP avec io_uring en l'associant
            // au SQE
            io_uring_prep_sendmsg(sqe, sock, &msgh, 0);

            // On associe notre packet à la SQE pour le free plus tard 
            io_uring_sqe_set_data(sqe, packet);

            inflight++;
            seq++;
        }

        // Des qu'on a 32 paquets en cours ou qu'on a traité tous les paquets
        // On les envoie dans io_uring
        io_uring_submit(ring);

        // On attend une reponse de nos SQE de la part de io_uring sous forme
        // de CQE (Completion Queue Entry)
        struct io_uring_cqe *cqe;
        io_uring_wait_cqe(ring, &cqe);

        // Une fois qu'on a une reponse de io_uring, on free notre packet
        free(io_uring_cqe_get_data(cqe));

        // On marque la CQE comme traitée
        io_uring_cqe_seen(ring, cqe);

        // On a un paquet de moins en cours d'envoi
        inflight--;
    }
}
