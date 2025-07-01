#include "screenshot.h"
#include "network.h"
#include <liburing.h>
#include <stdio.h>
#include <time.h>

int main(void)
{
    // Initialise screen_data qui contient les données de l'image
    struct screen_data sd = {0};

    // Lance une capture d'ecran qui sera stocke dans sd
    if (capture_screenshot(&sd) != 0)
    {
        return 1;
    }

    // Convertit la capture d'ecran PNG en un buffer brut BGRx
    if (convert_png_to_raw(&sd) != 0)
    {
        return 1;
    }

    // Setup la socket UDP pour envoyer les données
    struct sockaddr_in dest;
    int sock = setup_socket(&dest);

    if (sock < 0)
    {
        return 1;
    }

    // Setup io_uring avec 32 opérations asynchrones.
    struct io_uring ring;
    if (io_uring_queue_init(32, &ring, 0) < 0)
    {
        return 1;
    }

    double start = clock();

    // Envoi de l'image capturée en plussieurs paquets UDP
    send_image_data(sock, &ring, &sd, &dest);

    double end = (clock() - start) / CLOCKS_PER_SEC;
    
    // Affiche le temps d'envoi et le débit
    printf("Envoi terminé en %.2f s, débit %.2f MB/s\n",
           end, (sd.length/ (1024.0*1024.0))/end);
   
    // Nettoyage des ressources
    io_uring_queue_exit(&ring);
    close(sock);
    g_free(sd.data);
    g_object_unref(sd.portal);
    
    return 0;
}
