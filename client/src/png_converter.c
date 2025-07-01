#include "screenshot.h"
#include "network.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdlib.h>

int convert_png_to_raw(struct screen_data *sd)
{
    // Un loader GdkPixbuf permet de decoder et lire un fichier compressé.

    // On cree un loader pour decoder et lire des fichiers PNG compressés 
    GdkPixbufLoader *loader = gdk_pixbuf_loader_new_with_type("png", NULL);

    // On va ecrire les données PNG dans le loader.
    // Si le loader n'est pas créé ou si l'écriture échoue,
    if (!loader || !gdk_pixbuf_loader_write(loader, sd->data, sd->length, NULL))
    {
        g_printerr("Erreur lors du chargement des données PNG.\n");
        if (loader)
        {
            g_object_unref(loader);
        }
        return -1;
    }

    // On close l'écriture dans le loader (comme un fd)
    gdk_pixbuf_loader_close(loader, NULL);

    // On récupère le pixbuf qui contient l'image décodée entiere
    // cad avec les meta-données et les pixels de l'image. 
    GdkPixbuf *pix_buf = gdk_pixbuf_loader_get_pixbuf(loader);
    
    // On garde une référence au pixbuf
    g_object_ref(pix_buf);
    // On libère le loader car on n'en a plus besoin
    g_object_unref(loader);

    // On récupère les dimensions de l'image
    int image_width = gdk_pixbuf_get_width(pix_buf);
    int image_height = gdk_pixbuf_get_height(pix_buf);
    
    // Nombre d’octets par ligne 
    int octet_per_line = gdk_pixbuf_get_rowstride(pix_buf);

    // On recupère le nombre d'octet decrivant un pixel
    // (on appelle ça des channels : RGB = 3, RGBA = 4)
    int nb_channel = gdk_pixbuf_get_n_channels(pix_buf);

    if (nb_channel < 3)
    {
        g_printerr("Nombre de canaux inattendu: %d\n", nb_channel);
        g_object_unref(pix_buf);
        return -1;
    }

    // On calcule la taille du buffer brut (raw) en octets
    size_t raw_size = (size_t)image_width * image_height * PIXEL_BYTES;

    // On alloue un buffer brut pour stocker les pixels
    guchar *raw = malloc(raw_size);

    if (!raw)
    {
        perror("malloc");
        g_object_unref(pix_buf);
        return -1;
    }

    // On recupère les pixels de l'image
    guchar *pixels = gdk_pixbuf_get_pixels(pix_buf);

    // On itere sur la hauteur et la largeur de l'image pour chaque pixel
    for (int y = 0; y < image_height; y++)
    {
        // Calcul de la ligne de pixels
        guchar *row = pixels + (y * octet_per_line);

        for (int x = 0; x < image_width; x++)
        {
            guchar *pixel = row + x * nb_channel;

            // Offset = (ligne * largeur + colonne) * taille d'un pixel
            size_t off = (y * image_width + x) * PIXEL_BYTES;

            raw[off + 0] = pixel[2];    // B
            raw[off + 1] = pixel[1];    // G
            raw[off + 2] = pixel[0];    // R
            raw[off + 3] = 0xFF;        // X
        }
    }

    // On free notre pixbuf
    g_object_unref(pix_buf);

    // On free notre image png stockee dans sd->data
    g_free(sd->data);

    // On insert notre buffer brut dans la structure screen_data avec les
    // donnees de l'image
    sd->data = raw;
    sd->length = raw_size;
    sd->width = image_width;
    sd->height = image_height;
    
    return 0;
}
