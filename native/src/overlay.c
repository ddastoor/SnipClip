#include "overlay.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *drawing_area;
    GtkWidget *mode_btns[4];
    GdkPixbuf *desktop_pixbuf;
    int origin_x;
    int origin_y;
    ScSnipMode mode;
    ScWindowInfo windows[SC_MAX_WINDOWS];
    size_t window_count;
    int hovered_window_idx;

    bool dragging;
    double start_x;
    double start_y;
    double curr_x;
    double curr_y;

    ScPoint *lasso_pts;
    size_t lasso_count;
    size_t lasso_cap;

    bool finished;
    ScSnipCompletedFn on_completed;
    ScSnipCancelledFn on_cancelled;
    void *user_data;
} ScOverlayCtx;

static void overlay_free(ScOverlayCtx *ctx) {
    if (!ctx) return;
    if (ctx->desktop_pixbuf) {
        g_object_unref(ctx->desktop_pixbuf);
    }
    free(ctx->lasso_pts);
    free(ctx);
}

static void overlay_append_lasso(ScOverlayCtx *ctx, double x, double y) {
    if (ctx->lasso_count >= ctx->lasso_cap) {
        size_t next_cap = ctx->lasso_cap == 0 ? 256 : ctx->lasso_cap * 2;
        ScPoint *grown = (ScPoint *)realloc(ctx->lasso_pts, next_cap * sizeof(ScPoint));
        if (!grown) return;
        ctx->lasso_pts = grown;
        ctx->lasso_cap = next_cap;
    }
    ctx->lasso_pts[ctx->lasso_count].x = x;
    ctx->lasso_pts[ctx->lasso_count].y = y;
    ctx->lasso_count++;
}

static void overlay_finish_with_pixbuf(ScOverlayCtx *ctx, GdkPixbuf *result, ScSnipMode mode) {
    if (ctx->finished) {
        if (result) g_object_unref(result);
        return;
    }
    ctx->finished = true;
    ScSnipCompletedFn cb = ctx->on_completed;
    void *ud = ctx->user_data;
    gtk_widget_destroy(ctx->window);
    if (cb) {
        cb(result, mode, ud);
    }
    if (result) {
        g_object_unref(result);
    }
}

static void overlay_cancel(ScOverlayCtx *ctx) {
    if (ctx->finished) return;
    ctx->finished = true;
    ScSnipCancelledFn cb = ctx->on_cancelled;
    void *ud = ctx->user_data;
    gtk_widget_destroy(ctx->window);
    if (cb) {
        cb(ud);
    }
}

static void overlay_sync_mode_buttons(ScOverlayCtx *ctx) {
    for (int i = 0; i < 4; ++i) {
        GtkStyleContext *sc = gtk_widget_get_style_context(ctx->mode_btns[i]);
        if ((int)ctx->mode == i) {
            gtk_style_context_add_class(sc, "sc-overlay-active");
        } else {
            gtk_style_context_remove_class(sc, "sc-overlay-active");
        }
    }
}

static void on_mode_btn_clicked(GtkButton *btn, gpointer user_data) {
    ScOverlayCtx *ctx = (ScOverlayCtx *)user_data;
    int m = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "sc-mode"));
    ctx->mode = (ScSnipMode)m;
    ctx->dragging = false;
    ctx->lasso_count = 0;
    overlay_sync_mode_buttons(ctx);

    if (ctx->mode == SC_MODE_FULLSCREEN) {
        GdkPixbuf *copy = gdk_pixbuf_copy(ctx->desktop_pixbuf);
        overlay_finish_with_pixbuf(ctx, copy, SC_MODE_FULLSCREEN);
        return;
    }
    gtk_widget_queue_draw(ctx->drawing_area);
}

static void on_cancel_btn_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    overlay_cancel((ScOverlayCtx *)user_data);
}

static gboolean on_overlay_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)widget;
    ScOverlayCtx *ctx = (ScOverlayCtx *)user_data;
    if (event->keyval == GDK_KEY_Escape) {
        overlay_cancel(ctx);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_overlay_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    (void)widget;
    ScOverlayCtx *ctx = (ScOverlayCtx *)user_data;
    if (event->button == 3) {
        overlay_cancel(ctx);
        return TRUE;
    }
    if (event->button != 1) {
        return FALSE;
    }

    if (ctx->mode == SC_MODE_WINDOW) {
        if (ctx->hovered_window_idx >= 0 && (size_t)ctx->hovered_window_idx < ctx->window_count) {
            ScRect wr = ctx->windows[ctx->hovered_window_idx].bounds;
            int max_w = gdk_pixbuf_get_width(ctx->desktop_pixbuf);
            int max_h = gdk_pixbuf_get_height(ctx->desktop_pixbuf);
            ScRect local = sc_normalize_rect(
                wr.x - ctx->origin_x,
                wr.y - ctx->origin_y,
                wr.x - ctx->origin_x + wr.width,
                wr.y - ctx->origin_y + wr.height,
                max_w,
                max_h
            );
            if (local.width >= 8 && local.height >= 8) {
                GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(
                    ctx->desktop_pixbuf, local.x, local.y, local.width, local.height
                );
                GdkPixbuf *cloned = gdk_pixbuf_copy(sub);
                g_object_unref(sub);
                overlay_finish_with_pixbuf(ctx, cloned, SC_MODE_WINDOW);
                return TRUE;
            }
        }
        GdkPixbuf *full = gdk_pixbuf_copy(ctx->desktop_pixbuf);
        overlay_finish_with_pixbuf(ctx, full, SC_MODE_WINDOW);
        return TRUE;
    }

    ctx->dragging = true;
    ctx->start_x = event->x;
    ctx->start_y = event->y;
    ctx->curr_x = event->x;
    ctx->curr_y = event->y;
    if (ctx->mode == SC_MODE_FREEFORM) {
        ctx->lasso_count = 0;
        overlay_append_lasso(ctx, event->x, event->y);
    }
    gtk_widget_queue_draw(ctx->drawing_area);
    return TRUE;
}

static gboolean on_overlay_motion(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    (void)widget;
    ScOverlayCtx *ctx = (ScOverlayCtx *)user_data;
    ctx->curr_x = event->x;
    ctx->curr_y = event->y;

    if (ctx->mode == SC_MODE_WINDOW) {
        int gx = (int)event->x + ctx->origin_x;
        int gy = (int)event->y + ctx->origin_y;
        int found = -1;
        for (size_t i = 0; i < ctx->window_count; ++i) {
            ScRect r = ctx->windows[i].bounds;
            if (gx >= r.x && gx <= r.x + r.width && gy >= r.y && gy <= r.y + r.height) {
                found = (int)i;
                break;
            }
        }
        if (found != ctx->hovered_window_idx) {
            ctx->hovered_window_idx = found;
            gtk_widget_queue_draw(ctx->drawing_area);
        }
        return TRUE;
    }

    if (ctx->dragging) {
        if (ctx->mode == SC_MODE_FREEFORM) {
            overlay_append_lasso(ctx, event->x, event->y);
        }
        gtk_widget_queue_draw(ctx->drawing_area);
    }
    return TRUE;
}

static gboolean on_overlay_button_release(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    (void)widget;
    ScOverlayCtx *ctx = (ScOverlayCtx *)user_data;
    if (event->button != 1 || !ctx->dragging) {
        return FALSE;
    }
    ctx->dragging = false;
    ctx->curr_x = event->x;
    ctx->curr_y = event->y;

    int max_w = gdk_pixbuf_get_width(ctx->desktop_pixbuf);
    int max_h = gdk_pixbuf_get_height(ctx->desktop_pixbuf);

    if (ctx->mode == SC_MODE_RECTANGLE) {
        ScRect r = sc_normalize_rect(ctx->start_x, ctx->start_y, ctx->curr_x, ctx->curr_y, max_w, max_h);
        if (r.width >= 6 && r.height >= 6) {
            GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(ctx->desktop_pixbuf, r.x, r.y, r.width, r.height);
            GdkPixbuf *cropped = gdk_pixbuf_copy(sub);
            g_object_unref(sub);
            overlay_finish_with_pixbuf(ctx, cropped, SC_MODE_RECTANGLE);
            return TRUE;
        }
        gtk_widget_queue_draw(ctx->drawing_area);
        return TRUE;
    }

    if (ctx->mode == SC_MODE_FREEFORM && ctx->lasso_count >= 3) {
        ScRect bbox;
        if (sc_polygon_bbox(ctx->lasso_pts, ctx->lasso_count, max_w, max_h, &bbox)) {
            cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bbox.width, bbox.height);
            cairo_t *cr = cairo_create(surf);
            cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

            cairo_new_path(cr);
            cairo_move_to(cr, ctx->lasso_pts[0].x - bbox.x, ctx->lasso_pts[0].y - bbox.y);
            for (size_t i = 1; i < ctx->lasso_count; ++i) {
                cairo_line_to(cr, ctx->lasso_pts[i].x - bbox.x, ctx->lasso_pts[i].y - bbox.y);
            }
            cairo_close_path(cr);
            cairo_clip(cr);

            gdk_cairo_set_source_pixbuf(cr, ctx->desktop_pixbuf, -bbox.x, -bbox.y);
            cairo_paint(cr);
            cairo_destroy(cr);

            GdkPixbuf *masked = gdk_pixbuf_get_from_surface(surf, 0, 0, bbox.width, bbox.height);
            cairo_surface_destroy(surf);
            overlay_finish_with_pixbuf(ctx, masked, SC_MODE_FREEFORM);
            return TRUE;
        }
        ctx->lasso_count = 0;
        gtk_widget_queue_draw(ctx->drawing_area);
    }

    return TRUE;
}

static gboolean on_overlay_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    ScOverlayCtx *ctx = (ScOverlayCtx *)user_data;
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    gdk_cairo_set_source_pixbuf(cr, ctx->desktop_pixbuf, 0.0, 0.0);
    cairo_paint(cr);

    bool has_rect = false;
    ScRect sel = { 0, 0, 0, 0 };
    char label[192] = "";

    if (ctx->mode == SC_MODE_RECTANGLE && ctx->dragging) {
        sel = sc_normalize_rect(ctx->start_x, ctx->start_y, ctx->curr_x, ctx->curr_y, w, h);
        has_rect = true;
        snprintf(label, sizeof(label), "%d x %d px", sel.width, sel.height);
    } else if (ctx->mode == SC_MODE_WINDOW &&
               ctx->hovered_window_idx >= 0 &&
               (size_t)ctx->hovered_window_idx < ctx->window_count) {
        ScRect wr = ctx->windows[ctx->hovered_window_idx].bounds;
        sel = sc_normalize_rect(
            wr.x - ctx->origin_x,
            wr.y - ctx->origin_y,
            wr.x - ctx->origin_x + wr.width,
            wr.y - ctx->origin_y + wr.height,
            w,
            h
        );
        has_rect = true;
        snprintf(
            label,
            sizeof(label),
            "%.44s (%d x %d px)",
            ctx->windows[ctx->hovered_window_idx].title,
            sel.width,
            sel.height
        );
    }

    /* Draw dim overlay with even-odd cutout */
    cairo_save(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_rectangle(cr, 0.0, 0.0, (double)w, (double)h);
    if (has_rect) {
        cairo_rectangle(cr, (double)sel.x, (double)sel.y, (double)sel.width, (double)sel.height);
    } else if (ctx->mode == SC_MODE_FREEFORM && ctx->lasso_count >= 2) {
        cairo_move_to(cr, ctx->lasso_pts[0].x, ctx->lasso_pts[0].y);
        for (size_t i = 1; i < ctx->lasso_count; ++i) {
            cairo_line_to(cr, ctx->lasso_pts[i].x, ctx->lasso_pts[i].y);
        }
        cairo_close_path(cr);
    }
    cairo_set_source_rgba(cr, 0.06, 0.09, 0.16, 0.48);
    cairo_fill(cr);
    cairo_restore(cr);

    /* Selection border & live pixel badge */
    if (has_rect) {
        cairo_set_source_rgb(cr, 0.22, 0.74, 0.97);
        cairo_set_line_width(cr, 2.0);
        cairo_rectangle(cr, sel.x + 0.5, sel.y + 0.5, sel.width, sel.height);
        cairo_stroke(cr);

        if (label[0] != '\0') {
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 12.0);
            cairo_text_extents_t ext;
            cairo_text_extents(cr, label, &ext);
            double bw = ext.width + 18.0;
            double bh = 24.0;
            double bx = (double)sel.x;
            double by = sel.y > 32 ? (double)sel.y - 30.0 : (double)sel.y + 8.0;

            cairo_set_source_rgba(cr, 0.06, 0.09, 0.16, 0.90);
            cairo_rectangle(cr, bx, by, bw, bh);
            cairo_fill(cr);

            cairo_set_source_rgb(cr, 0.97, 0.98, 0.99);
            cairo_move_to(cr, bx + 9.0, by + 16.0);
            cairo_show_text(cr, label);
        }
    } else if (ctx->mode == SC_MODE_FREEFORM && ctx->lasso_count >= 2) {
        cairo_set_source_rgb(cr, 0.22, 0.74, 0.97);
        cairo_set_line_width(cr, 2.0);
        cairo_move_to(cr, ctx->lasso_pts[0].x, ctx->lasso_pts[0].y);
        for (size_t i = 1; i < ctx->lasso_count; ++i) {
            cairo_line_to(cr, ctx->lasso_pts[i].x, ctx->lasso_pts[i].y);
        }
        cairo_stroke(cr);
    }

    return TRUE;
}

static void on_overlay_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    overlay_free((ScOverlayCtx *)user_data);
}

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
) {
    if (!desktop_pixbuf) {
        if (on_cancelled) on_cancelled(user_data);
        return;
    }

    if (initial_mode == SC_MODE_FULLSCREEN) {
        GdkPixbuf *copy = gdk_pixbuf_copy(desktop_pixbuf);
        if (on_completed) on_completed(copy, SC_MODE_FULLSCREEN, user_data);
        g_object_unref(copy);
        return;
    }

    ScOverlayCtx *ctx = (ScOverlayCtx *)calloc(1, sizeof(ScOverlayCtx));
    ctx->desktop_pixbuf = g_object_ref(desktop_pixbuf);
    ctx->origin_x = origin_x;
    ctx->origin_y = origin_y;
    ctx->mode = initial_mode;
    ctx->hovered_window_idx = -1;
    ctx->on_completed = on_completed;
    ctx->on_cancelled = on_cancelled;
    ctx->user_data = user_data;

    if (windows && window_count > 0) {
        ctx->window_count = window_count > SC_MAX_WINDOWS ? SC_MAX_WINDOWS : window_count;
        memcpy(ctx->windows, windows, ctx->window_count * sizeof(ScWindowInfo));
    }

    int w = gdk_pixbuf_get_width(desktop_pixbuf);
    int h = gdk_pixbuf_get_height(desktop_pixbuf);

    ctx->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(ctx->window), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(ctx->window), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(ctx->window), w, h);
    gtk_window_move(GTK_WINDOW(ctx->window), origin_x, origin_y);
    gtk_window_fullscreen(GTK_WINDOW(ctx->window));

    GtkWidget *overlay_container = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(ctx->window), overlay_container);

    ctx->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(ctx->drawing_area, w, h);
    gtk_widget_add_events(
        ctx->drawing_area,
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK
    );
    gtk_container_add(GTK_CONTAINER(overlay_container), ctx->drawing_area);

    /* Floating Windows 11-style Capture Mode Bar */
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(bar), "sc-overlay-bar");
    gtk_widget_set_halign(bar, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(bar, GTK_ALIGN_START);
    gtk_widget_set_margin_top(bar, 18);

    const char *labels[4] = { "Rectangle", "Window", "Fullscreen", "Freeform" };
    for (int i = 0; i < 4; ++i) {
        GtkWidget *b = gtk_button_new_with_label(labels[i]);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "sc-overlay-btn");
        g_object_set_data(G_OBJECT(b), "sc-mode", GINT_TO_POINTER(i));
        g_signal_connect(b, "clicked", G_CALLBACK(on_mode_btn_clicked), ctx);
        gtk_box_pack_start(GTK_BOX(bar), b, FALSE, FALSE, 0);
        ctx->mode_btns[i] = b;
    }

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel (Esc)");
    gtk_style_context_add_class(gtk_widget_get_style_context(cancel_btn), "sc-overlay-btn");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_btn_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(bar), cancel_btn, FALSE, FALSE, 0);

    overlay_sync_mode_buttons(ctx);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay_container), bar);

    g_signal_connect(ctx->drawing_area, "draw", G_CALLBACK(on_overlay_draw), ctx);
    g_signal_connect(ctx->drawing_area, "button-press-event", G_CALLBACK(on_overlay_button_press), ctx);
    g_signal_connect(ctx->drawing_area, "motion-notify-event", G_CALLBACK(on_overlay_motion), ctx);
    g_signal_connect(ctx->drawing_area, "button-release-event", G_CALLBACK(on_overlay_button_release), ctx);
    g_signal_connect(ctx->window, "key-press-event", G_CALLBACK(on_overlay_key_press), ctx);
    g_signal_connect(ctx->window, "destroy", G_CALLBACK(on_overlay_destroy), ctx);

    gtk_widget_show_all(ctx->window);
    gtk_window_present(GTK_WINDOW(ctx->window));

    GdkWindow *gdk_win = gtk_widget_get_window(ctx->drawing_area);
    if (gdk_win) {
        GdkCursor *cross = gdk_cursor_new_for_display(gdk_window_get_display(gdk_win), GDK_CROSSHAIR);
        gdk_window_set_cursor(gdk_win, cross);
        if (cross) g_object_unref(cross);
    }
}
