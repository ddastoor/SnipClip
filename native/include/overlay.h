#ifndef SNIPCLIP_OVERLAY_H
#define SNIPCLIP_OVERLAY_H

#include <gtk/gtk.h>

#include "capture_backend.h"
#include "geometry.h"

typedef void (*ScSnipCompletedFn)(GdkPixbuf *captured, ScSnipMode mode, void *user_data);
typedef void (*ScSnipCancelledFn)(void *user_data);
/* Invoked (after the overlay window is destroyed) when the user picks "Window" on a session
 * without enumerable windows (Wayland): the caller should run an interactive portal window
 * re-capture instead of silently grabbing the full desktop. */
typedef void (*ScSnipWindowRecaptureFn)(void *user_data);

/*
 * Launch the fullscreen capture overlay.
 *
 * desktop_pixbuf may be a physical-pixel (HiDPI) capture; all overlay UI is drawn in logical
 * widget coordinates and selections are mapped to physical pixbuf pixels on crop.
 * windows[] bounds are in the same physical root coordinate space as desktop_pixbuf + origin.
 * When window_count == 0, the overlay "Window" button either calls on_window_recapture (if
 * non-NULL) or is disabled — it never captures the whole desktop as a "window".
 */
void sc_launch_capture_overlay(
    GdkPixbuf *desktop_pixbuf,
    int origin_x,
    int origin_y,
    ScSnipMode initial_mode,
    const ScWindowInfo *windows,
    size_t window_count,
    ScSnipCompletedFn on_completed,
    ScSnipCancelledFn on_cancelled,
    ScSnipWindowRecaptureFn on_window_recapture,
    void *user_data
);

#endif /* SNIPCLIP_OVERLAY_H */
