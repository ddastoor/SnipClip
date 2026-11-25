#include "editor_canvas.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SC_MAX_HISTORY 40

typedef struct {
    ScToolKind kind;
    GdkRGBA color;
    double width;
    ScPoint *pts;
    size_t count;
    size_t cap;
} ScStrokeItem;

typedef struct {
    GdkPixbuf *pixbuf;
    ScStrokeItem *items;
    size_t item_count;
} ScSnapshot;

struct ScEditorCanvas {
    GtkWidget *drawing_area;
    GdkPixbuf *pixbuf;

    ScStrokeItem *items;
    size_t item_count;
    size_t item_cap;

    bool has_active_item;
    ScStrokeItem active_item;

    ScSnapshot undo_stack[SC_MAX_HISTORY];
    size_t undo_len;
    ScSnapshot redo_stack[SC_MAX_HISTORY];
    size_t redo_len;

    ScToolKind tool;
    GdkRGBA color;
    double stroke_width;
    double zoom;

    ScRuler ruler;
    bool dragging_ruler;
    double ruler_drag_dx;
    double ruler_drag_dy;

    bool crop_active;
    ScRect crop_rect;
    bool dragging_crop;
    double crop_start_x;
    double crop_start_y;

    ScEditorStateCb on_state_changed;
    void *user_data;
};

static void stroke_free(ScStrokeItem *it) {
    if (!it) return;
    free(it->pts);
    it->pts = NULL;
    it->count = 0;
    it->cap = 0;
}

static ScStrokeItem stroke_clone(const ScStrokeItem *src) {
    ScStrokeItem dst;
    memset(&dst, 0, sizeof(dst));
    dst.kind = src->kind;
    dst.color = src->color;
    dst.width = src->width;
    if (src->count > 0 && src->pts) {
        dst.pts = (ScPoint *)malloc(src->count * sizeof(ScPoint));
        if (dst.pts) {
            memcpy(dst.pts, src->pts, src->count * sizeof(ScPoint));
            dst.count = src->count;
            dst.cap = src->count;
        }
    }
    return dst;
}

static void stroke_add_point(ScStrokeItem *it, double x, double y) {
    if (it->count >= it->cap) {
        size_t next = it->cap == 0 ? 32 : it->cap * 2;
        ScPoint *grown = (ScPoint *)realloc(it->pts, next * sizeof(ScPoint));
        if (!grown) return;
        it->pts = grown;
        it->cap = next;
    }
    it->pts[it->count].x = x;
    it->pts[it->count].y = y;
    it->count++;
}

static void snapshot_clear(ScSnapshot *snap) {
    if (!snap) return;
    if (snap->pixbuf) {
        g_object_unref(snap->pixbuf);
        snap->pixbuf = NULL;
    }
    for (size_t i = 0; i < snap->item_count; ++i) {
        stroke_free(&snap->items[i]);
    }
    free(snap->items);
    snap->items = NULL;
    snap->item_count = 0;
}

static ScSnapshot snapshot_capture(const ScEditorCanvas *ed) {
    ScSnapshot s;
    memset(&s, 0, sizeof(s));
    if (ed->pixbuf) {
        s.pixbuf = gdk_pixbuf_copy(ed->pixbuf);
    }
    if (ed->item_count > 0) {
        s.items = (ScStrokeItem *)calloc(ed->item_count, sizeof(ScStrokeItem));
        if (s.items) {
            s.item_count = ed->item_count;
            for (size_t i = 0; i < ed->item_count; ++i) {
                s.items[i] = stroke_clone(&ed->items[i]);
            }
        }
    }
    return s;
}

static void editor_clear_stack(ScSnapshot *stack, size_t *len) {
    for (size_t i = 0; i < *len; ++i) {
        snapshot_clear(&stack[i]);
    }
    *len = 0;
}

static void editor_notify(ScEditorCanvas *ed) {
    if (ed->on_state_changed) {
        ed->on_state_changed(ed->user_data);
    }
}

static void editor_sync_size_request(ScEditorCanvas *ed) {
    if (!ed->pixbuf) {
        gtk_widget_set_size_request(ed->drawing_area, -1, -1);
        return;
    }
    int w = (int)(gdk_pixbuf_get_width(ed->pixbuf) * ed->zoom) + 32;
    int h = (int)(gdk_pixbuf_get_height(ed->pixbuf) * ed->zoom) + 32;
    gtk_widget_set_size_request(ed->drawing_area, w, h);
}

static void editor_push_undo(ScEditorCanvas *ed) {
    if (ed->undo_len >= SC_MAX_HISTORY) {
        snapshot_clear(&ed->undo_stack[0]);
        memmove(&ed->undo_stack[0], &ed->undo_stack[1], (SC_MAX_HISTORY - 1) * sizeof(ScSnapshot));
        ed->undo_len = SC_MAX_HISTORY - 1;
    }
    ed->undo_stack[ed->undo_len++] = snapshot_capture(ed);
    editor_clear_stack(ed->redo_stack, &ed->redo_len);
}

static void editor_restore_snapshot(ScEditorCanvas *ed, ScSnapshot *snap) {
    if (ed->pixbuf) {
        g_object_unref(ed->pixbuf);
        ed->pixbuf = NULL;
    }
    if (snap->pixbuf) {
        ed->pixbuf = gdk_pixbuf_copy(snap->pixbuf);
    }
    for (size_t i = 0; i < ed->item_count; ++i) {
        stroke_free(&ed->items[i]);
    }
    free(ed->items);
    ed->items = NULL;
    ed->item_count = 0;
    ed->item_cap = 0;

    if (snap->item_count > 0) {
        ed->items = (ScStrokeItem *)calloc(snap->item_count, sizeof(ScStrokeItem));
        if (ed->items) {
            ed->item_count = snap->item_count;
            ed->item_cap = snap->item_count;
            for (size_t i = 0; i < snap->item_count; ++i) {
                ed->items[i] = stroke_clone(&snap->items[i]);
            }
        }
    }
    snapshot_clear(snap);
}

static void editor_append_item(ScEditorCanvas *ed, const ScStrokeItem *item) {
    if (ed->item_count >= ed->item_cap) {
        size_t next = ed->item_cap == 0 ? 16 : ed->item_cap * 2;
        ScStrokeItem *grown = (ScStrokeItem *)realloc(ed->items, next * sizeof(ScStrokeItem));
        if (!grown) return;
        ed->items = grown;
        ed->item_cap = next;
    }
    ed->items[ed->item_count++] = stroke_clone(item);
}

static void editor_canvas_origin(const ScEditorCanvas *ed, double *out_ox, double *out_oy) {
    if (!ed->pixbuf) {
        *out_ox = 0.0;
        *out_oy = 0.0;
        return;
    }
    int alloc_w = gtk_widget_get_allocated_width(ed->drawing_area);
    int alloc_h = gtk_widget_get_allocated_height(ed->drawing_area);
    double sw = gdk_pixbuf_get_width(ed->pixbuf) * ed->zoom;
    double sh = gdk_pixbuf_get_height(ed->pixbuf) * ed->zoom;
    *out_ox = fmax(16.0, (alloc_w - sw) / 2.0);
    *out_oy = fmax(16.0, (alloc_h - sh) / 2.0);
}

static ScPoint widget_to_image(const ScEditorCanvas *ed, double wx, double wy) {
    double ox = 0.0, oy = 0.0;
    editor_canvas_origin(ed, &ox, &oy);
    ScPoint p = { (wx - ox) / ed->zoom, (wy - oy) / ed->zoom };
    return p;
}

static void draw_pixelate_mosaic(cairo_t *cr, const GdkPixbuf *backing, ScRect region) {
    if (!backing || region.width < 4 || region.height < 4) return;
    int bw = gdk_pixbuf_get_width(backing);
    int bh = gdk_pixbuf_get_height(backing);
    ScRect r = sc_normalize_rect(region.x, region.y, region.x + region.width, region.y + region.height, bw, bh);
    if (r.width < 4 || r.height < 4) return;

    int block = 10;
    const guchar *pixels = gdk_pixbuf_get_pixels(backing);
    int rowstride = gdk_pixbuf_get_rowstride(backing);
    int n_channels = gdk_pixbuf_get_n_channels(backing);

    for (int y = r.y; y < r.y + r.height; y += block) {
        for (int x = r.x; x < r.x + r.width; x += block) {
            int sx = x + block / 2 < bw ? x + block / 2 : bw - 1;
            int sy = y + block / 2 < bh ? y + block / 2 : bh - 1;
            const guchar *p = pixels + sy * rowstride + sx * n_channels;
            int cw = (x + block > r.x + r.width) ? (r.x + r.width - x) : block;
            int ch = (y + block > r.y + r.height) ? (r.y + r.height - y) : block;
            cairo_set_source_rgb(cr, p[0] / 255.0, p[1] / 255.0, p[2] / 255.0);
            cairo_rectangle(cr, (double)x, (double)y, (double)cw, (double)ch);
            cairo_fill(cr);
        }
    }
}

static void draw_stroke_item(cairo_t *cr, const ScStrokeItem *it, const GdkPixbuf *backing) {
    if (!it || it->count == 0) return;

    cairo_save(cr);
    if (it->kind == SC_TOOL_HIGHLIGHTER) {
        cairo_set_source_rgba(cr, it->color.red, it->color.green, it->color.blue, 0.42);
        cairo_set_line_width(cr, it->width);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_move_to(cr, it->pts[0].x, it->pts[0].y);
        for (size_t i = 1; i < it->count; ++i) {
            cairo_line_to(cr, it->pts[i].x, it->pts[i].y);
        }
        cairo_stroke(cr);
    } else if (it->kind == SC_TOOL_PEN) {
        cairo_set_source_rgba(cr, it->color.red, it->color.green, it->color.blue, 1.0);
        cairo_set_line_width(cr, it->width);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        if (it->count == 1) {
            cairo_arc(cr, it->pts[0].x, it->pts[0].y, it->width / 2.0, 0.0, 2.0 * M_PI);
            cairo_fill(cr);
        } else {
            cairo_move_to(cr, it->pts[0].x, it->pts[0].y);
            for (size_t i = 1; i < it->count; ++i) {
                cairo_line_to(cr, it->pts[i].x, it->pts[i].y);
            }
            cairo_stroke(cr);
        }
    } else if (it->count >= 2) {
        ScPoint p1 = it->pts[0];
        ScPoint p2 = it->pts[it->count - 1];
        double rx = fmin(p1.x, p2.x);
        double ry = fmin(p1.y, p2.y);
        double rw = fabs(p2.x - p1.x);
        double rh = fabs(p2.y - p1.y);

        cairo_set_source_rgba(cr, it->color.red, it->color.green, it->color.blue, 1.0);
        cairo_set_line_width(cr, it->width);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

        if (it->kind == SC_TOOL_RECT) {
            cairo_rectangle(cr, rx, ry, rw, rh);
            cairo_stroke(cr);
        } else if (it->kind == SC_TOOL_ELLIPSE && rw > 1.0 && rh > 1.0) {
            cairo_save(cr);
            cairo_translate(cr, rx + rw / 2.0, ry + rh / 2.0);
            cairo_scale(cr, rw / 2.0, rh / 2.0);
            cairo_arc(cr, 0.0, 0.0, 1.0, 0.0, 2.0 * M_PI);
            cairo_restore(cr);
            cairo_stroke(cr);
        } else if (it->kind == SC_TOOL_LINE) {
            cairo_move_to(cr, p1.x, p1.y);
            cairo_line_to(cr, p2.x, p2.y);
            cairo_stroke(cr);
        } else if (it->kind == SC_TOOL_ARROW) {
            cairo_move_to(cr, p1.x, p1.y);
            cairo_line_to(cr, p2.x, p2.y);
            ScPoint left_w, right_w;
            sc_compute_arrow_wings(p1.x, p1.y, p2.x, p2.y, it->width, &left_w, &right_w);
            cairo_move_to(cr, p2.x, p2.y);
            cairo_line_to(cr, left_w.x, left_w.y);
            cairo_move_to(cr, p2.x, p2.y);
            cairo_line_to(cr, right_w.x, right_w.y);
            cairo_stroke(cr);
        } else if (it->kind == SC_TOOL_REDACT) {
            cairo_set_source_rgb(cr, 0.06, 0.09, 0.16);
            cairo_rectangle(cr, rx, ry, rw, rh);
            cairo_fill(cr);
        } else if (it->kind == SC_TOOL_PIXELATE) {
            ScRect mr = { (int)rx, (int)ry, (int)rw, (int)rh };
            draw_pixelate_mosaic(cr, backing, mr);
        }
    }
    cairo_restore(cr);
}

static void draw_ruler_overlay(cairo_t *cr, const ScRuler *ruler) {
    if (!ruler || !ruler->visible) return;

    cairo_save(cr);
    cairo_translate(cr, ruler->cx, ruler->cy);
    cairo_rotate(cr, ruler->angle_deg * (M_PI / 180.0));

    double half_l = ruler->length / 2.0;
    double half_t = ruler->half_thickness;

    cairo_set_source_rgba(cr, 0.97, 0.98, 0.99, 0.90);
    cairo_rectangle(cr, -half_l, -half_t, ruler->length, half_t * 2.0);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.28, 0.33, 0.41);
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, -half_l, -half_t, ruler->length, half_t * 2.0);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, 0.39, 0.45, 0.55);
    cairo_set_line_width(cr, 1.0);
    int limit = (int)half_l - 8;
    for (int tx = -limit; tx <= limit; tx += 10) {
        double tick = (tx % 50 == 0) ? 11.0 : 5.5;
        cairo_move_to(cr, (double)tx, -half_t);
        cairo_line_to(cr, (double)tx, -half_t + tick);
        cairo_move_to(cr, (double)tx, half_t);
        cairo_line_to(cr, (double)tx, half_t - tick);
    }
    cairo_stroke(cr);

    char deg_buf[64];
    int disp_deg = ((int)llround(ruler->angle_deg) % 180 + 180) % 180;
    snprintf(deg_buf, sizeof(deg_buf), "Ruler · %d deg", disp_deg);
    cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, deg_buf, &ext);
    cairo_set_source_rgb(cr, 0.06, 0.09, 0.16);
    cairo_move_to(cr, -ext.width / 2.0, 4.0);
    cairo_show_text(cr, deg_buf);

    cairo_restore(cr);
}

static void erase_near_point(ScEditorCanvas *ed, ScPoint pt) {
    for (long i = (long)ed->item_count - 1; i >= 0; --i) {
        ScStrokeItem *it = &ed->items[i];
        if (sc_hit_test_annotation(pt.x, pt.y, it->kind, it->pts, it->count, 14.0)) {
            editor_push_undo(ed);
            stroke_free(it);
            if ((size_t)i + 1 < ed->item_count) {
                memmove(&ed->items[i], &ed->items[i + 1], (ed->item_count - (size_t)i - 1) * sizeof(ScStrokeItem));
            }
            ed->item_count--;
            editor_notify(ed);
            gtk_widget_queue_draw(ed->drawing_area);
            return;
        }
    }
}

static gboolean on_canvas_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
    (void)widget;
    ScEditorCanvas *ed = (ScEditorCanvas *)user_data;
    if (!ed->pixbuf) return FALSE;

    double dy = 0.0;
    if (event->direction == GDK_SCROLL_UP) dy = -1.0;
    else if (event->direction == GDK_SCROLL_DOWN) dy = 1.0;
    else if (event->direction == GDK_SCROLL_SMOOTH) {
        gdk_event_get_scroll_deltas((const GdkEvent *)event, NULL, &dy);
    }
    if (fabs(dy) < 0.01) return FALSE;

    ScPoint img_pt = widget_to_image(ed, event->x, event->y);
    if (sc_point_inside_ruler(img_pt.x, img_pt.y, &ed->ruler)) {
        double step = dy < 0.0 ? 5.0 : -5.0;
        ed->ruler.angle_deg = fmod(ed->ruler.angle_deg + step + 360.0, 360.0);
        gtk_widget_queue_draw(ed->drawing_area);
        return TRUE;
    }

    if (event->state & GDK_CONTROL_MASK) {
        double zstep = dy < 0.0 ? 0.10 : -0.10;
        sc_editor_set_zoom(ed, ed->zoom + zstep);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_canvas_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    (void)widget;
    ScEditorCanvas *ed = (ScEditorCanvas *)user_data;
    if (!ed->pixbuf || event->button != 1) return FALSE;

    ScPoint img_pt = widget_to_image(ed, event->x, event->y);

    if (sc_point_inside_ruler(img_pt.x, img_pt.y, &ed->ruler)) {
        ed->dragging_ruler = true;
        ed->ruler_drag_dx = img_pt.x - ed->ruler.cx;
        ed->ruler_drag_dy = img_pt.y - ed->ruler.cy;
        return TRUE;
    }

    if (ed->crop_active) {
        ed->dragging_crop = true;
        ed->crop_start_x = img_pt.x;
        ed->crop_start_y = img_pt.y;
        ed->crop_rect = sc_normalize_rect(
            img_pt.x,
            img_pt.y,
            img_pt.x,
            img_pt.y,
            gdk_pixbuf_get_width(ed->pixbuf),
            gdk_pixbuf_get_height(ed->pixbuf)
        );
        gtk_widget_queue_draw(ed->drawing_area);
        return TRUE;
    }

    if (ed->tool == SC_TOOL_ERASER) {
        erase_near_point(ed, img_pt);
        return TRUE;
    }

    ScPoint snapped = sc_snap_point_to_ruler(img_pt.x, img_pt.y, &ed->ruler, 18.0);
    memset(&ed->active_item, 0, sizeof(ed->active_item));
    ed->active_item.kind = ed->tool;
    ed->active_item.color = ed->color;
    ed->active_item.width = ed->stroke_width * (ed->tool == SC_TOOL_HIGHLIGHTER ? 4.5 : 1.0);
    stroke_add_point(&ed->active_item, snapped.x, snapped.y);
    ed->has_active_item = true;
    gtk_widget_queue_draw(ed->drawing_area);
    return TRUE;
}

static gboolean on_canvas_motion(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    (void)widget;
    ScEditorCanvas *ed = (ScEditorCanvas *)user_data;
    if (!ed->pixbuf) return FALSE;

    ScPoint img_pt = widget_to_image(ed, event->x, event->y);

    if (ed->dragging_ruler) {
        ed->ruler.cx = img_pt.x - ed->ruler_drag_dx;
        ed->ruler.cy = img_pt.y - ed->ruler_drag_dy;
        gtk_widget_queue_draw(ed->drawing_area);
        return TRUE;
    }

    if (ed->dragging_crop && ed->crop_active) {
        ed->crop_rect = sc_normalize_rect(
            ed->crop_start_x,
            ed->crop_start_y,
            img_pt.x,
            img_pt.y,
            gdk_pixbuf_get_width(ed->pixbuf),
            gdk_pixbuf_get_height(ed->pixbuf)
        );
        gtk_widget_queue_draw(ed->drawing_area);
        return TRUE;
    }

    if ((event->state & GDK_BUTTON1_MASK) && ed->tool == SC_TOOL_ERASER) {
        erase_near_point(ed, img_pt);
        return TRUE;
    }

    if (ed->has_active_item) {
        ScPoint snapped = sc_snap_point_to_ruler(img_pt.x, img_pt.y, &ed->ruler, 18.0);
        if (ed->active_item.kind == SC_TOOL_PEN || ed->active_item.kind == SC_TOOL_HIGHLIGHTER) {
            stroke_add_point(&ed->active_item, snapped.x, snapped.y);
        } else {
            if (ed->active_item.count < 2) {
                stroke_add_point(&ed->active_item, snapped.x, snapped.y);
            } else {
                ed->active_item.pts[1] = snapped;
            }
        }
        gtk_widget_queue_draw(ed->drawing_area);
        return TRUE;
    }

    return FALSE;
}

static gboolean on_canvas_button_release(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    (void)widget;
    ScEditorCanvas *ed = (ScEditorCanvas *)user_data;
    if (event->button != 1) return FALSE;

    if (ed->dragging_ruler) {
        ed->dragging_ruler = false;
        return TRUE;
    }
    if (ed->dragging_crop) {
        ed->dragging_crop = false;
        return TRUE;
    }
    if (ed->has_active_item) {
        if (ed->active_item.count >= 2 ||
            ed->active_item.kind == SC_TOOL_PEN ||
            ed->active_item.kind == SC_TOOL_HIGHLIGHTER) {
            editor_push_undo(ed);
            editor_append_item(ed, &ed->active_item);
            editor_notify(ed);
        }
        stroke_free(&ed->active_item);
        ed->has_active_item = false;
        gtk_widget_queue_draw(ed->drawing_area);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_canvas_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    ScEditorCanvas *ed = (ScEditorCanvas *)user_data;
    int aw = gtk_widget_get_allocated_width(widget);
    int ah = gtk_widget_get_allocated_height(widget);

    cairo_set_source_rgb(cr, 0.945, 0.961, 0.976);
    cairo_rectangle(cr, 0.0, 0.0, (double)aw, (double)ah);
    cairo_fill(cr);

    if (!ed->pixbuf) {
        const char *line1 = "Click '+ New' or press PrintScreen to snip";
        const char *line2 = "Ctrl+Shift+S shortcut  \xc2\xb7  Ctrl+O to open image";
        double cy = (double)ah / 2.0;

        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 13.0);
        cairo_text_extents_t ext1;
        cairo_text_extents(cr, line1, &ext1);
        cairo_set_source_rgb(cr, 0.200, 0.255, 0.333); /* #334155 */
        cairo_move_to(cr, ((double)aw - ext1.width) / 2.0 - ext1.x_bearing, cy - 5.0);
        cairo_show_text(cr, line1);

        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.5);
        cairo_text_extents_t ext2;
        cairo_text_extents(cr, line2, &ext2);
        cairo_set_source_rgb(cr, 0.392, 0.455, 0.545); /* #64748b */
        cairo_move_to(cr, ((double)aw - ext2.width) / 2.0 - ext2.x_bearing, cy + 15.0);
        cairo_show_text(cr, line2);
        return TRUE;
    }

    int iw = gdk_pixbuf_get_width(ed->pixbuf);
    int ih = gdk_pixbuf_get_height(ed->pixbuf);
    double ox = 0.0, oy = 0.0;
    editor_canvas_origin(ed, &ox, &oy);

    cairo_save(cr);
    cairo_translate(cr, ox, oy);
    cairo_scale(cr, ed->zoom, ed->zoom);

    cairo_set_source_rgb(cr, 0.80, 0.84, 0.88);
    cairo_set_line_width(cr, 1.0 / ed->zoom);
    cairo_rectangle(cr, -1.0, -1.0, iw + 2.0, ih + 2.0);
    cairo_stroke(cr);

    gdk_cairo_set_source_pixbuf(cr, ed->pixbuf, 0.0, 0.0);
    cairo_paint(cr);

    for (size_t i = 0; i < ed->item_count; ++i) {
        draw_stroke_item(cr, &ed->items[i], ed->pixbuf);
    }
    if (ed->has_active_item) {
        draw_stroke_item(cr, &ed->active_item, ed->pixbuf);
    }

    draw_ruler_overlay(cr, &ed->ruler);

    if (ed->crop_active) {
        cairo_save(cr);
        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
        cairo_rectangle(cr, 0.0, 0.0, (double)iw, (double)ih);
        cairo_rectangle(
            cr,
            (double)ed->crop_rect.x,
            (double)ed->crop_rect.y,
            (double)ed->crop_rect.width,
            (double)ed->crop_rect.height
        );
        cairo_set_source_rgba(cr, 0.06, 0.09, 0.16, 0.48);
        cairo_fill(cr);

        double dashes[2] = { 6.0 / ed->zoom, 4.0 / ed->zoom };
        cairo_set_dash(cr, dashes, 2, 0.0);
        cairo_set_source_rgb(cr, 0.145, 0.388, 0.922);
        cairo_set_line_width(cr, 2.0 / ed->zoom);
        cairo_rectangle(
            cr,
            (double)ed->crop_rect.x,
            (double)ed->crop_rect.y,
            (double)ed->crop_rect.width,
            (double)ed->crop_rect.height
        );
        cairo_stroke(cr);
        cairo_restore(cr);
    }

    cairo_restore(cr);
    return TRUE;
}

ScEditorCanvas *sc_editor_canvas_new(ScEditorStateCb on_state_changed, void *user_data) {
    ScEditorCanvas *ed = (ScEditorCanvas *)calloc(1, sizeof(ScEditorCanvas));
    ed->tool = SC_TOOL_PEN;
    gdk_rgba_parse(&ed->color, "#ef4444");
    ed->stroke_width = 3.0;
    ed->zoom = 1.0;
    ed->ruler = sc_fit_ruler_to_canvas(640, 420, false, 0.0);
    ed->on_state_changed = on_state_changed;
    ed->user_data = user_data;

    ed->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(ed->drawing_area, -1, -1);
    gtk_widget_add_events(
        ed->drawing_area,
        GDK_BUTTON_PRESS_MASK |
            GDK_BUTTON_RELEASE_MASK |
            GDK_POINTER_MOTION_MASK |
            GDK_SCROLL_MASK |
            GDK_SMOOTH_SCROLL_MASK
    );

    g_signal_connect(ed->drawing_area, "draw", G_CALLBACK(on_canvas_draw), ed);
    g_signal_connect(ed->drawing_area, "button-press-event", G_CALLBACK(on_canvas_button_press), ed);
    g_signal_connect(ed->drawing_area, "motion-notify-event", G_CALLBACK(on_canvas_motion), ed);
    g_signal_connect(ed->drawing_area, "button-release-event", G_CALLBACK(on_canvas_button_release), ed);
    g_signal_connect(ed->drawing_area, "scroll-event", G_CALLBACK(on_canvas_scroll), ed);

    return ed;
}

void sc_editor_canvas_free(ScEditorCanvas *ed) {
    if (!ed) return;
    if (ed->pixbuf) g_object_unref(ed->pixbuf);
    for (size_t i = 0; i < ed->item_count; ++i) {
        stroke_free(&ed->items[i]);
    }
    free(ed->items);
    if (ed->has_active_item) {
        stroke_free(&ed->active_item);
    }
    editor_clear_stack(ed->undo_stack, &ed->undo_len);
    editor_clear_stack(ed->redo_stack, &ed->redo_len);
    free(ed);
}

GtkWidget *sc_editor_canvas_widget(ScEditorCanvas *ed) {
    return ed ? ed->drawing_area : NULL;
}

void sc_editor_set_pixbuf(ScEditorCanvas *ed, GdkPixbuf *pixbuf) {
    if (!ed || !pixbuf) return;
    if (ed->pixbuf) g_object_unref(ed->pixbuf);
    ed->pixbuf = gdk_pixbuf_copy(pixbuf);

    for (size_t i = 0; i < ed->item_count; ++i) {
        stroke_free(&ed->items[i]);
    }
    ed->item_count = 0;
    editor_clear_stack(ed->undo_stack, &ed->undo_len);
    editor_clear_stack(ed->redo_stack, &ed->redo_len);

    ed->crop_active = false;
    ed->zoom = 1.0;
    ed->ruler = sc_fit_ruler_to_canvas(
        gdk_pixbuf_get_width(ed->pixbuf),
        gdk_pixbuf_get_height(ed->pixbuf),
        ed->ruler.visible,
        ed->ruler.angle_deg
    );
    editor_sync_size_request(ed);
    editor_notify(ed);
    gtk_widget_queue_draw(ed->drawing_area);
}

void sc_editor_reset_to_empty(ScEditorCanvas *ed) {
    if (!ed) return;
    if (ed->pixbuf) {
        g_object_unref(ed->pixbuf);
        ed->pixbuf = NULL;
    }
    for (size_t i = 0; i < ed->item_count; ++i) {
        stroke_free(&ed->items[i]);
    }
    ed->item_count = 0;
    if (ed->has_active_item) {
        stroke_free(&ed->active_item);
        ed->has_active_item = false;
    }
    editor_clear_stack(ed->undo_stack, &ed->undo_len);
    editor_clear_stack(ed->redo_stack, &ed->redo_len);

    ed->crop_active = false;
    ed->dragging_crop = false;
    ed->dragging_ruler = false;
    if (ed->tool == SC_TOOL_CROP) {
        ed->tool = SC_TOOL_PEN;
    }
    ed->zoom = 1.0;
    editor_sync_size_request(ed);
    gtk_widget_queue_draw(ed->drawing_area);
    editor_notify(ed);
}

bool sc_editor_has_image(const ScEditorCanvas *ed) {
    return ed && ed->pixbuf != NULL;
}

void sc_editor_get_dimensions(const ScEditorCanvas *ed, int *out_w, int *out_h) {
    if (!ed || !ed->pixbuf) {
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;
        return;
    }
    if (out_w) *out_w = gdk_pixbuf_get_width(ed->pixbuf);
    if (out_h) *out_h = gdk_pixbuf_get_height(ed->pixbuf);
}

void sc_editor_set_tool(ScEditorCanvas *ed, ScToolKind tool) {
    if (!ed) return;
    ed->tool = tool;
    if (tool == SC_TOOL_CROP && ed->pixbuf) {
        int iw = gdk_pixbuf_get_width(ed->pixbuf);
        int ih = gdk_pixbuf_get_height(ed->pixbuf);
        int mx = (int)fmax(12.0, iw * 0.08);
        int my = (int)fmax(12.0, ih * 0.08);
        ed->crop_rect = sc_normalize_rect(mx, my, iw - mx, ih - my, iw, ih);
        ed->crop_active = true;
    } else if (ed->crop_active && tool != SC_TOOL_CROP) {
        ed->crop_active = false;
    }
    editor_notify(ed);
    gtk_widget_queue_draw(ed->drawing_area);
}

ScToolKind sc_editor_get_tool(const ScEditorCanvas *ed) {
    return ed ? ed->tool : SC_TOOL_PEN;
}

void sc_editor_set_color_rgba(ScEditorCanvas *ed, const GdkRGBA *rgba) {
    if (ed && rgba) ed->color = *rgba;
}

void sc_editor_get_color_rgba(const ScEditorCanvas *ed, GdkRGBA *out_rgba) {
    if (ed && out_rgba) *out_rgba = ed->color;
}

void sc_editor_set_stroke_width(ScEditorCanvas *ed, double width) {
    if (!ed) return;
    if (width < 1.0) width = 1.0;
    if (width > 32.0) width = 32.0;
    ed->stroke_width = width;
}

double sc_editor_get_stroke_width(const ScEditorCanvas *ed) {
    return ed ? ed->stroke_width : 3.0;
}

void sc_editor_set_ruler_visible(ScEditorCanvas *ed, bool visible) {
    if (!ed) return;
    ed->ruler.visible = visible;
    gtk_widget_queue_draw(ed->drawing_area);
}

bool sc_editor_get_ruler_visible(const ScEditorCanvas *ed) {
    return ed ? ed->ruler.visible : false;
}

void sc_editor_set_zoom(ScEditorCanvas *ed, double zoom) {
    if (!ed) return;
    if (zoom < 0.25) zoom = 0.25;
    if (zoom > 4.0) zoom = 4.0;
    ed->zoom = round(zoom * 100.0) / 100.0;
    editor_sync_size_request(ed);
    editor_notify(ed);
    gtk_widget_queue_draw(ed->drawing_area);
}

double sc_editor_get_zoom(const ScEditorCanvas *ed) {
    return ed ? ed->zoom : 1.0;
}

bool sc_editor_can_undo(const ScEditorCanvas *ed) {
    return ed && ed->undo_len > 0;
}

bool sc_editor_can_redo(const ScEditorCanvas *ed) {
    return ed && ed->redo_len > 0;
}

void sc_editor_undo(ScEditorCanvas *ed) {
    if (!ed || ed->undo_len == 0) return;
    if (ed->redo_len < SC_MAX_HISTORY) {
        ed->redo_stack[ed->redo_len++] = snapshot_capture(ed);
    }
    editor_restore_snapshot(ed, &ed->undo_stack[--ed->undo_len]);
    editor_sync_size_request(ed);
    editor_notify(ed);
    gtk_widget_queue_draw(ed->drawing_area);
}

void sc_editor_redo(ScEditorCanvas *ed) {
    if (!ed || ed->redo_len == 0) return;
    if (ed->undo_len < SC_MAX_HISTORY) {
        ed->undo_stack[ed->undo_len++] = snapshot_capture(ed);
    }
    editor_restore_snapshot(ed, &ed->redo_stack[--ed->redo_len]);
    editor_sync_size_request(ed);
    editor_notify(ed);
    gtk_widget_queue_draw(ed->drawing_area);
}

void sc_editor_clear_ink(ScEditorCanvas *ed) {
    if (!ed || ed->item_count == 0) return;
    editor_push_undo(ed);
    for (size_t i = 0; i < ed->item_count; ++i) {
        stroke_free(&ed->items[i]);
    }
    ed->item_count = 0;
    editor_notify(ed);
    gtk_widget_queue_draw(ed->drawing_area);
}

bool sc_editor_is_crop_active(const ScEditorCanvas *ed) {
    return ed && ed->crop_active;
}

void sc_editor_apply_crop(ScEditorCanvas *ed) {
    if (!ed || !ed->crop_active || !ed->pixbuf) return;
    GdkPixbuf *flattened = sc_editor_render_pixbuf(ed);
    if (!flattened) return;

    int fw = gdk_pixbuf_get_width(flattened);
    int fh = gdk_pixbuf_get_height(flattened);
    ScRect r = sc_normalize_rect(
        ed->crop_rect.x,
        ed->crop_rect.y,
        ed->crop_rect.x + ed->crop_rect.width,
        ed->crop_rect.y + ed->crop_rect.height,
        fw,
        fh
    );
    if (r.width < 8 || r.height < 8) {
        g_object_unref(flattened);
        return;
    }

    editor_push_undo(ed);
    GdkPixbuf *sub = gdk_pixbuf_new_subpixbuf(flattened, r.x, r.y, r.width, r.height);
    GdkPixbuf *cropped = gdk_pixbuf_copy(sub);
    g_object_unref(sub);
    g_object_unref(flattened);

    g_object_unref(ed->pixbuf);
    ed->pixbuf = cropped;

    for (size_t i = 0; i < ed->item_count; ++i) {
        stroke_free(&ed->items[i]);
    }
    ed->item_count = 0;
    ed->crop_active = false;
    ed->tool = SC_TOOL_PEN;
    ed->ruler = sc_fit_ruler_to_canvas(r.width, r.height, ed->ruler.visible, ed->ruler.angle_deg);

    editor_sync_size_request(ed);
    editor_notify(ed);
    gtk_widget_queue_draw(ed->drawing_area);
}

void sc_editor_cancel_crop(ScEditorCanvas *ed) {
    if (!ed) return;
    ed->crop_active = false;
    if (ed->tool == SC_TOOL_CROP) {
        ed->tool = SC_TOOL_PEN;
    }
    editor_notify(ed);
    gtk_widget_queue_draw(ed->drawing_area);
}

GdkPixbuf *sc_editor_render_pixbuf(const ScEditorCanvas *ed) {
    if (!ed || !ed->pixbuf) return NULL;
    int w = gdk_pixbuf_get_width(ed->pixbuf);
    int h = gdk_pixbuf_get_height(ed->pixbuf);

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t *cr = cairo_create(surf);
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    gdk_cairo_set_source_pixbuf(cr, ed->pixbuf, 0.0, 0.0);
    cairo_paint(cr);

    for (size_t i = 0; i < ed->item_count; ++i) {
        draw_stroke_item(cr, &ed->items[i], ed->pixbuf);
    }
    cairo_destroy(cr);

    GdkPixbuf *result = gdk_pixbuf_get_from_surface(surf, 0, 0, w, h);
    cairo_surface_destroy(surf);
    return result;
}
