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

/* Capture full desktop (or interactive Wayland window when interactive_window=true).
 * Wayland full-desktop captures follow sc_wayland_capture_plan() exactly (matching capture_driver). */
GdkPixbuf *sc_capture_desktop_pixbuf(
    const ScDisplaySession *session,
    bool interactive_window,
    int *out_origin_x,
    int *out_origin_y
);

/* Interactive XDG Desktop Portal window/area picker ONLY (returns NULL instead of silently
 * falling back to a full-desktop grab when the portal is unavailable or cancelled). */
GdkPixbuf *sc_capture_wayland_window_interactive(void);

/* Enumerate visible top-level X11 windows in topmost-first stacking order via libX11 */
size_t sc_enumerate_x11_windows(ScWindowInfo *out_windows, size_t max_windows);

/* Copy GdkPixbuf to GtkClipboard and persist via non-blocking wl-copy / xclip daemon */
bool sc_copy_pixbuf_to_clipboard(GdkPixbuf *pixbuf, const ScDisplaySession *session);

#endif /* SNIPCLIP_CAPTURE_BACKEND_H */
