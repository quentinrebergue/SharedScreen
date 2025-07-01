#ifndef RECEPTION_H
#define RECEPTION_H

#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include "packet.h"

typedef struct reception_state {
    uint32_t current_image_id; // ID de l'image en cours de réception
    uint32_t total_packets; // Nombre total de paquets attendus
    uint32_t width, height; // Dimensions de l'image
    uint8_t *image_buffer; // Buffer pour stocker l'image reçue
    uint8_t *received_mask; // Masque pour suivre les paquets reçus
    uint32_t packets_received; // Nombre de paquets reçus
    size_t packet_payload_size; // Taille du payload d'un paquet (données de l'image)
    time_t last_activity; // Dernière activité (timestamp)
    int active; // Indique si une réception est en cours
} reception_state_t;

// État de réception global
extern reception_state_t rx_state;

void reset_reception_state(void);
void save_image(void);
void process_packet(char *data, ssize_t len);

#endif // RECEPTION_H
            