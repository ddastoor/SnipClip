#ifndef SNIPCLIP_OVERLAY_H
#define SNIPCLIP_OVERLAY_H

#include <gtk/gtk.h>

#include "capture_backend.h"
#include "geometry.h"

typedef void (*ScSnipCompletedFn)(GdkPixbuf *captured, ScSnipMode mode, void *user_data);
typedef void (*ScSnipCancelledFn)(void *user_data);

void sc_launch_capture_overlay(
    GdkPixbuf *desktop_pixbuf,
    int origin_x,
    int origin_y,
    ScSnipMode initial_mode,
    const ScWindowInfo *windows,
    size_t window_count,
    ScSnipCompletedFn on_completed,
    ScSnipCancelledFn on_cancelled,
    void *user_data
);

#endif /* SNIPCLIP_OVERLAY_H */
