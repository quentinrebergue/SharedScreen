#include "screenshot.h"
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>

// Callback une fois que le screenshot a été fait
// - GObject *source : l’objet qui a lancé l’opération => sd->portal
// - GAsyncResult *result : Le résultat du screenshot
// - gpointer user_data : notre screen_data qu'on a donné en argument
void on_screenshot_ready(GObject *source, GAsyncResult *res,
    gpointer user_data)
{
    //On convertit au bon type
    struct screen_data *sd = user_data;
    GError *error = NULL;

    //Termine l'opération de screenshot async et get l'URI de l'image
    //XDP_PORTAL est simplement un cast d'un GObject* vers XdpPortal*  
    gchar *uri = xdp_portal_take_screenshot_finish(XDP_PORTAL(source), res,
    &error);

    if (error)
    {
        g_printerr("Erreur de capture d'écran: %s\n", error->message);
        g_error_free(error);
        g_main_loop_quit(sd->loop);
        return;
    }

	// Conversion d'un URI en chemin local
    gchar *path = g_filename_from_uri(uri, NULL, &error);

    // On libère l’URI
    g_free(uri);

    if (error)
    {
        g_printerr("Erreur de conversion d'URI: %s\n", error->message);
        g_error_free(error);
        g_main_loop_quit(sd->loop);
        return;
    }

    //Lit le contenu du PNG dans notre buffer sd->data et update length
    if (!g_file_get_contents(path, (gchar**)&sd->data, &sd->length, &error))
    {
        g_printerr("Échec de la lecture de %s: %s\n", path, error->message);
        g_error_free(error);
    }

    //On free le path
    g_free(path);

    //On quitte la boucle async permettant de débloquer capture_screenshot
    g_main_loop_quit(sd->loop);
}

int capture_screenshot(struct screen_data* sd)
{
    //Init une GLIB event loop
    g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);

    //Stocke la GLIB event loop dans la struct
    sd->loop = loop;

    //Créer le portail XDG et le stocke dans la struct
    sd->portal = xdp_portal_new();

    // Démarre la capture d’écran de façon asynchrone :
    // - sd->portal   : Objet qui gère la capture (notre portail)
    // - NULL         : Fenêtre parent (aucune)
    // - XDP_SCREENSHOT_FLAG_NONE : Pas d'option
    // - NULL         : Aucun détail supplémentaire
    // - on_screenshot_ready : Callback appelé une fois la capture faite
    // - sd           : notre data qu'on veut donner à la callback
    xdp_portal_take_screenshot(sd->portal, NULL, XDP_SCREENSHOT_FLAG_NONE, NULL,
        on_screenshot_ready, sd);

    //Lance la boucle, en attente de la callback
    g_main_loop_run(loop);

    if (!sd->data)
    {
        fprintf(stderr, "La capture d'écran a échoué.\n");
        return -1;
    }
    return 0;
}
