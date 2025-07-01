#include "reception.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>

// État de réception des images
reception_state_t rx_state = {0};

// Réinitialise l'état de réception
void reset_reception_state(void)
{
    free(rx_state.image_buffer);
    free(rx_state.received_mask);
    memset(&rx_state, 0, sizeof(rx_state));
}

/// Sauvegarde l’image en mémoire sous forme de fichier PPM
void save_image(void)
{
    
    // Si pas actif ou pas de buffer d'image
    if (!rx_state.active || !rx_state.image_buffer)
    {
        return;
    }

    // Nom du fichier
    char filename[64];

    // Génère le nom du fichier avec l'ID de l'image actuelle
    snprintf(filename, sizeof(filename), "image_%u.ppm", rx_state.current_image_id);
    
    // Ouvre le fichier en écriture binaire
    FILE *f = fopen(filename, "wb");

    if (!f)
    {
        perror("fopen");
        return;
    }
    
    // Écrit l'en-tête PPM
    fprintf(f, "P6 %u %u 255\n", rx_state.width, rx_state.height);

    // Itere sur les pixels de l'image et écrit les données RGB
    for (size_t i = 0; i < (size_t)rx_state.width * rx_state.height * PIXEL_BYTES; i += PIXEL_BYTES) {
        fputc(rx_state.image_buffer[i + 2], f);
        fputc(rx_state.image_buffer[i + 1], f);
        fputc(rx_state.image_buffer[i + 0], f);
    }

    // Ferme le fichier
    fclose(f);
    
    printf("Image %u saved: %s (%.1f%% complete)\n",
           rx_state.current_image_id, filename,
           (100.0 * rx_state.packets_received) / rx_state.total_packets);
}

/// Traite un paquet, extrait les données et met à jour l'état de réception
void process_packet(char *data, ssize_t len)
{
    // Si la longueur du paquet est inférieure à l'en-tête, on ignore
    if (len < (ssize_t)sizeof(struct packet_header))
    {
        return;
    }
    
    // Copie l'en-tête du paquet dans une structure
    struct packet_header hdr;
    memcpy(&hdr, data, sizeof(hdr));

    // On extrait les informations de l'en-tête
    uint32_t img_id = ntohl(hdr.image_id);
    uint32_t seq    = ntohl(hdr.seq);

    // Si état de réception pas actif ou ID de l'image correspond pas
    if (!rx_state.active || img_id != rx_state.current_image_id)
    {
        // Si une image est déjà en cours, on la sauvegarde
        if (rx_state.active)
        {
            save_image();
        }

        // Réinitialise l'état de réception
        reset_reception_state();

        // Met à jour l'état de réception avec les nouvelles données
        rx_state.active            = 1;
        rx_state.current_image_id  = img_id;
        rx_state.total_packets     = ntohl(hdr.total_packets);
        rx_state.width             = ntohl(hdr.width);
        rx_state.height            = ntohl(hdr.height);
        rx_state.packet_payload_size = PACKET_SIZE - sizeof(struct packet_header);
        
        // Alloue le buffer pour l'image et le masque de réception
        size_t image_size = (size_t)rx_state.total_packets * rx_state.packet_payload_size;
        rx_state.image_buffer = calloc(1, image_size);
        rx_state.received_mask = calloc(rx_state.total_packets, 1);
        
        // Si l'allocation échoue, on réinitialise l'état de réception
        if (!rx_state.image_buffer || !rx_state.received_mask)
        {
            perror("calloc");
            reset_reception_state();
            return;
        }

        printf("New image %u: %ux%u, %u packets expected.\n",
               img_id, rx_state.width, rx_state.height, rx_state.total_packets);
    }

    // Met à jour l'heure de la dernière activité
    rx_state.last_activity = time(NULL);

    // Si la séquence est valide et pas déjà reçue
    if (seq < rx_state.total_packets && !rx_state.received_mask[seq])
    {
        // Marque le paquet comme reçu
        rx_state.received_mask[seq] = 1;
        rx_state.packets_received++;

        // Copie les données du paquet dans le buffer d'image
        memcpy(rx_state.image_buffer + seq * rx_state.packet_payload_size,
               data + sizeof(struct packet_header), len - sizeof(struct packet_header));
    }
}
