#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <glib.h>
#include <libportal/portal.h>

struct screen_data {
	GMainLoop *loop;           //GLIB event loop pour gérer l'async
    guchar    *data;           //Buffer où on stocke l'image
    gsize      length;         //Taille du buffer
    guint32    width, height;  //Les dimensions de l'image
    XdpPortal *portal;         //Le portail
};

int capture_screenshot(struct screen_data *sd);
void on_screenshot_ready(GObject *source, GAsyncResult *res, gpointer user_data);
int convert_png_to_raw(struct screen_data *sd);

#endif // SCREENSHOT_H
