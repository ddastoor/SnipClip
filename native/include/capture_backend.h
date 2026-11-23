#ifndef SNIPCLIP_CAPTURE_BACKEND_H
#define SNIPCLIP_CAPTURE_BACKEND_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include <stddef.h>

#include "display_detect.h"
#include "geometry.h"

#define SC_MAX_WINDOWS 128

typedef struct {
    unsigned long window_id;
    char title[128];
    ScRect bounds;
} ScWindowInfo;

/* Capture full desktop (or interactive Wayland window when interactive_window=true) */
GdkPixbuf *sc_capture_desktop_pixbuf(
    const ScDisplaySession *session,
    bool interactive_window,
    int *out_origin_x,
    int *out_origin_y
);

/* Enumerate visible top-level X11 windows in topmost-first stacking order via libX11 */
size_t sc_enumerate_x11_windows(ScWindowInfo *out_windows, size_t max_windows);

/* Copy GdkPixbuf to GtkClipboard and persist via non-blocking wl-copy / xclip daemon */
bool sc_copy_pixbuf_to_clipboard(GdkPixbuf *pixbuf, const ScDisplaySession *session);

#endif /* SNIPCLIP_CAPTURE_BACKEND_H */
