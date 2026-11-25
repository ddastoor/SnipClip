/*
 * Headless GTK 3 / Cairo test suite for ScEditorCanvas (native/src/editor_canvas.c).
 *
 * Run under a virtual X server:
 *   xvfb-run -a -s "-screen 0 1920x1080x24" make -C native test-gtk
 *
 * Synthetic GdkEventButton / GdkEventMotion events are emitted directly on the canvas drawing
 * area (no realized window needed). With an unallocated widget the canvas origin is fixed at
 * (16, 16) and zoom is 1.0, so widget coordinates map to image coordinates by a -16 offset.
 */
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor_canvas.h"
#include "geometry.h"
#include "snipclip_version.h"

#define CANVAS_ORIGIN 16.0
#define IMG_W 200
#define IMG_H 150

static int g_checks = 0;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        ++g_checks;                                                                       \
        if (!(cond)) {                                                                    \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);               \
            exit(1);                                                                      \
        }                                                                                 \
    } while (0)

typedef struct {
    int r, g, b, a;
} Px;

static int g_state_changes = 0;

static void on_state_changed(void *user_data) {
    (void)user_data;
    g_state_changes++;
}

static GdkPixbuf *make_solid_pixbuf(int w, int h, bool alpha, guchar r, guchar g, guchar b, guchar a) {
    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, alpha, 8, w, h);
    assert(pb != NULL);
    guint32 fill = ((guint32)r << 24) | ((guint32)g << 16) | ((guint32)b << 8) | (guint32)(alpha ? a : 0xff);
    gdk_pixbuf_fill(pb, fill);
    return pb;
}

/* Checkerboard of 5x5 cells (black/white) so mosaic blocks are detectable */
static GdkPixbuf *make_checker_pixbuf(int w, int h) {
    GdkPixbuf *pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w, h);
    assert(pb != NULL);
    int rs = gdk_pixbuf_get_rowstride(pb);
    int nc = gdk_pixbuf_get_n_channels(pb);
    guchar *px = gdk_pixbuf_get_pixels(pb);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            guchar v = (((x / 5) + (y / 5)) % 2) ? 255 : 0;
            guchar *p = px + y * rs + x * nc;
            p[0] = v;
            p[1] = (guchar)(x & 0xff);
            p[2] = (guchar)(y & 0xff);
        }
    }
    return pb;
}

static Px get_px(GdkPixbuf *pb, int x, int y) {
    Px out = { 0, 0, 0, 255 };
    assert(x >= 0 && y >= 0 && x < gdk_pixbuf_get_width(pb) && y < gdk_pixbuf_get_height(pb));
    int nc = gdk_pixbuf_get_n_channels(pb);
    const guchar *p = gdk_pixbuf_get_pixels(pb) + y * gdk_pixbuf_get_rowstride(pb) + x * nc;
    out.r = p[0];
    out.g = p[1];
    out.b = p[2];
    out.a = nc == 4 ? p[3] : 255;
    return out;
}

static bool is_white(Px p) {
    return p.r >= 245 && p.g >= 245 && p.b >= 245 && p.a >= 245;
}

static bool is_strong_red(Px p) {
    return p.r >= 200 && p.g <= 90 && p.b <= 90 && p.a >= 200;
}

static Px render_px(ScEditorCanvas *ed, int x, int y) {
    GdkPixbuf *flat = sc_editor_render_pixbuf(ed);
    assert(flat != NULL);
    Px p = get_px(flat, x, y);
    g_object_unref(flat);
    return p;
}

static gboolean emit_button(GtkWidget *w, GdkEventType type, double ix, double iy) {
    GdkEvent *ev = gdk_event_new(type);
    ev->button.button = 1;
    ev->button.x = ix + CANVAS_ORIGIN;
    ev->button.y = iy + CANVAS_ORIGIN;
    ev->button.state = type == GDK_BUTTON_RELEASE ? GDK_BUTTON1_MASK : 0;
    gboolean handled = FALSE;
    g_signal_emit_by_name(w, type == GDK_BUTTON_PRESS ? "button-press-event" : "button-release-event", ev, &handled);
    gdk_event_free(ev);
    return handled;
}

static gboolean emit_motion(GtkWidget *w, double ix, double iy) {
    GdkEvent *ev = gdk_event_new(GDK_MOTION_NOTIFY);
    ev->motion.x = ix + CANVAS_ORIGIN;
    ev->motion.y = iy + CANVAS_ORIGIN;
    ev->motion.state = GDK_BUTTON1_MASK;
    gboolean handled = FALSE;
    g_signal_emit_by_name(w, "motion-notify-event", ev, &handled);
    gdk_event_free(ev);
    return handled;
}

/* Press at (x1,y1), drag through intermediate points to (x2,y2), release */
static void drag(ScEditorCanvas *ed, double x1, double y1, double x2, double y2) {
    GtkWidget *w = sc_editor_canvas_widget(ed);
    CHECK(emit_button(w, GDK_BUTTON_PRESS, x1, y1));
    for (int i = 1; i <= 8; ++i) {
        double t = (double)i / 8.0;
        emit_motion(w, x1 + (x2 - x1) * t, y1 + (y2 - y1) * t);
    }
    CHECK(emit_button(w, GDK_BUTTON_RELEASE, x2, y2));
}

static ScEditorCanvas *new_canvas_with(GdkPixbuf *pb) {
    ScEditorCanvas *ed = sc_editor_canvas_new(on_state_changed, NULL);
    CHECK(ed != NULL);
    g_object_ref_sink(sc_editor_canvas_widget(ed));
    sc_editor_set_pixbuf(ed, pb);
    GdkRGBA red = { 1.0, 0.0, 0.0, 1.0 };
    sc_editor_set_color_rgba(ed, &red);
    sc_editor_set_stroke_width(ed, 4.0);
    return ed;
}

static void free_canvas(ScEditorCanvas *ed) {
    GtkWidget *w = sc_editor_canvas_widget(ed);
    sc_editor_canvas_free(ed);
    g_object_unref(w);
}

static ScEditorCanvas *new_white_canvas(void) {
    GdkPixbuf *white = make_solid_pixbuf(IMG_W, IMG_H, FALSE, 255, 255, 255, 255);
    ScEditorCanvas *ed = new_canvas_with(white);
    g_object_unref(white);
    return ed;
}

static void test_empty_and_state(void) {
    ScEditorCanvas *ed = sc_editor_canvas_new(on_state_changed, NULL);
    g_object_ref_sink(sc_editor_canvas_widget(ed));
    CHECK(!sc_editor_has_image(ed));
    CHECK(sc_editor_render_pixbuf(ed) == NULL);
    CHECK(!sc_editor_can_undo(ed) && !sc_editor_can_redo(ed));
    int before = g_state_changes;
    GdkPixbuf *pb = make_solid_pixbuf(IMG_W, IMG_H, FALSE, 255, 255, 255, 255);
    sc_editor_set_pixbuf(ed, pb);
    g_object_unref(pb);
    CHECK(sc_editor_has_image(ed));
    CHECK(g_state_changes > before);
    int w = 0, h = 0;
    sc_editor_get_dimensions(ed, &w, &h);
    CHECK(w == IMG_W && h == IMG_H);
    sc_editor_set_zoom(ed, 9.0);
    CHECK(fabs(sc_editor_get_zoom(ed) - 4.0) < 1e-9);
    sc_editor_set_zoom(ed, 0.01);
    CHECK(fabs(sc_editor_get_zoom(ed) - 0.25) < 1e-9);
    sc_editor_set_zoom(ed, 1.0);
    sc_editor_set_stroke_width(ed, 100.0);
    CHECK(fabs(sc_editor_get_stroke_width(ed) - 32.0) < 1e-9);
    sc_editor_reset_to_empty(ed);
    CHECK(!sc_editor_has_image(ed));
    GtkWidget *wd = sc_editor_canvas_widget(ed);
    sc_editor_canvas_free(ed);
    g_object_unref(wd);
}

static void test_pen_highlighter_shapes(void) {
    /* Pen */
    ScEditorCanvas *ed = new_white_canvas();
    sc_editor_set_tool(ed, SC_TOOL_PEN);
    CHECK(sc_editor_get_tool(ed) == SC_TOOL_PEN);
    CHECK(is_white(render_px(ed, 50, 20)));
    drag(ed, 20, 20, 80, 20);
    CHECK(is_strong_red(render_px(ed, 50, 20)));
    CHECK(is_white(render_px(ed, 50, 40)));
    CHECK(sc_editor_can_undo(ed));
    sc_editor_undo(ed);
    CHECK(is_white(render_px(ed, 50, 20)));
    CHECK(sc_editor_can_redo(ed));
    sc_editor_redo(ed);
    CHECK(is_strong_red(render_px(ed, 50, 20)));
    free_canvas(ed);

    /* Highlighter: translucent (alpha 0.42) red over white keeps G/B partially bright */
    ed = new_white_canvas();
    sc_editor_set_tool(ed, SC_TOOL_HIGHLIGHTER);
    drag(ed, 20, 60, 180, 60);
    Px hl = render_px(ed, 100, 60);
    CHECK(hl.r >= 240);
    CHECK(hl.g > 100 && hl.g < 200);
    CHECK(hl.b > 100 && hl.b < 200);
    free_canvas(ed);

    /* Rectangle outline: edge red, interior untouched */
    ed = new_white_canvas();
    sc_editor_set_tool(ed, SC_TOOL_RECT);
    drag(ed, 30, 30, 110, 110);
    CHECK(is_strong_red(render_px(ed, 30, 70)));
    CHECK(is_strong_red(render_px(ed, 70, 110)));
    CHECK(is_white(render_px(ed, 70, 70)));
    free_canvas(ed);

    /* Ellipse: top/left extremes red, center and bbox corner untouched */
    ed = new_white_canvas();
    sc_editor_set_tool(ed, SC_TOOL_ELLIPSE);
    drag(ed, 40, 30, 160, 130);
    CHECK(is_strong_red(render_px(ed, 100, 30)));
    CHECK(is_strong_red(render_px(ed, 40, 80)));
    CHECK(is_white(render_px(ed, 100, 80)));
    CHECK(is_white(render_px(ed, 42, 32)));
    free_canvas(ed);

    /* Line */
    ed = new_white_canvas();
    sc_editor_set_tool(ed, SC_TOOL_LINE);
    drag(ed, 20, 20, 180, 130);
    double t = 0.5;
    CHECK(is_strong_red(render_px(ed, (int)lround(20 + 160 * t), (int)lround(20 + 110 * t))));
    CHECK(is_white(render_px(ed, 180, 20)));
    free_canvas(ed);

    /* Arrow: shaft plus both wing tips */
    ed = new_white_canvas();
    sc_editor_set_tool(ed, SC_TOOL_ARROW);
    drag(ed, 20, 75, 180, 75);
    CHECK(is_strong_red(render_px(ed, 100, 75)));
    ScPoint lw, rw;
    sc_compute_arrow_wings(20, 75, 180, 75, 4.0, &lw, &rw);
    /* sample just inside each wing segment, near its tip */
    int lx = (int)lround(180 + (lw.x - 180) * 0.8), ly = (int)lround(75 + (lw.y - 75) * 0.8);
    int rx = (int)lround(180 + (rw.x - 180) * 0.8), ry = (int)lround(75 + (rw.y - 75) * 0.8);
    CHECK(is_strong_red(render_px(ed, lx, ly)));
    CHECK(is_strong_red(render_px(ed, rx, ry)));
    CHECK(ly != ry);
    free_canvas(ed);

    /* Redact: opaque #0f172a-style slate block */
    ed = new_white_canvas();
    sc_editor_set_tool(ed, SC_TOOL_REDACT);
    drag(ed, 50, 40, 150, 100);
    Px rd = render_px(ed, 100, 70);
    CHECK(rd.r <= 30 && rd.g <= 40 && rd.b <= 60 && rd.a == 255);
    CHECK(is_white(render_px(ed, 160, 70)));
    free_canvas(ed);
}

static void test_eraser(void) {
    ScEditorCanvas *ed = new_white_canvas();
    sc_editor_set_tool(ed, SC_TOOL_LINE);
    drag(ed, 20, 100, 180, 100);
    CHECK(is_strong_red(render_px(ed, 100, 100)));
    sc_editor_set_tool(ed, SC_TOOL_LINE);
    drag(ed, 20, 30, 180, 30);
    CHECK(is_strong_red(render_px(ed, 100, 30)));

    sc_editor_set_tool(ed, SC_TOOL_ERASER);
    GtkWidget *w = sc_editor_canvas_widget(ed);
    CHECK(emit_button(w, GDK_BUTTON_PRESS, 100, 104));
    emit_button(w, GDK_BUTTON_RELEASE, 100, 104);
    CHECK(is_white(render_px(ed, 100, 100)));
    CHECK(is_strong_red(render_px(ed, 100, 30)));

    /* Erase is undoable */
    sc_editor_undo(ed);
    CHECK(is_strong_red(render_px(ed, 100, 100)));

    /* Eraser drag (motion with button 1 held) removes the other stroke */
    CHECK(emit_button(w, GDK_BUTTON_PRESS, 5, 5));
    emit_motion(w, 100, 33);
    emit_button(w, GDK_BUTTON_RELEASE, 100, 33);
    CHECK(is_white(render_px(ed, 100, 30)));

    /* Clear ink removes everything in one undoable step */
    sc_editor_clear_ink(ed);
    CHECK(is_white(render_px(ed, 100, 100)));
    sc_editor_undo(ed);
    CHECK(is_strong_red(render_px(ed, 100, 100)));
    free_canvas(ed);
}

static void test_pixelate_opaque_and_alpha(void) {
    /* Opaque checkerboard: inside the mosaic, each 10x10 block becomes one uniform color */
    GdkPixbuf *checker = make_checker_pixbuf(IMG_W, IMG_H);
    ScEditorCanvas *ed = new_canvas_with(checker);
    Px before_a = get_px(checker, 40, 40);
    Px before_b = get_px(checker, 45, 40);
    CHECK(before_a.r != before_b.r);
    sc_editor_set_tool(ed, SC_TOOL_PIXELATE);
    drag(ed, 40, 40, 100, 100);
    GdkPixbuf *flat = sc_editor_render_pixbuf(ed);
    CHECK(flat != NULL);
    Px m1 = get_px(flat, 40, 40);
    Px m2 = get_px(flat, 45, 40);
    Px m3 = get_px(flat, 49, 49);
    CHECK(m1.r == m2.r && m1.g == m2.g && m1.b == m2.b);
    CHECK(m1.r == m3.r && m1.g == m3.g && m1.b == m3.b);
    /* Sample of block (40..49) is taken from pixel (45,45) of the source */
    Px src = get_px(checker, 45, 45);
    CHECK(abs(m1.r - src.r) <= 1 && abs(m1.g - src.g) <= 1 && abs(m1.b - src.b) <= 1);
    /* Outside region unchanged */
    Px outside = get_px(flat, 120, 120);
    Px outside_src = get_px(checker, 120, 120);
    CHECK(outside.r == outside_src.r && outside.g == outside_src.g);
    g_object_unref(flat);
    g_object_unref(checker);
    free_canvas(ed);

    /* Transparent ARGB freeform snip: pixelate must NOT paint opaque black over clear pixels */
    GdkPixbuf *clear = make_solid_pixbuf(IMG_W, IMG_H, TRUE, 0, 0, 0, 0);
    /* opaque green island at top-left so one area is visible */
    GdkPixbuf *island = gdk_pixbuf_new_subpixbuf(clear, 0, 0, 60, 60);
    CHECK(island != NULL);
    gdk_pixbuf_fill(island, 0x00ff00ffu);
    g_object_unref(island);
    ed = new_canvas_with(clear);
    sc_editor_set_tool(ed, SC_TOOL_PIXELATE);
    drag(ed, 20, 20, 180, 140);
    flat = sc_editor_render_pixbuf(ed);
    CHECK(flat != NULL && gdk_pixbuf_get_has_alpha(flat));
    Px transparent = get_px(flat, 150, 120);
    CHECK(transparent.a == 0);
    Px island_px = get_px(flat, 30, 30);
    CHECK(island_px.a == 255 && island_px.g >= 250 && island_px.r <= 5);
    g_object_unref(flat);
    g_object_unref(clear);
    free_canvas(ed);
}

static void test_crop(void) {
    ScEditorCanvas *ed = new_white_canvas();
    /* annotate first so cropping flattens annotations into the new base */
    sc_editor_set_tool(ed, SC_TOOL_PEN);
    drag(ed, 20, 50, 100, 50);

    sc_editor_set_tool(ed, SC_TOOL_CROP);
    CHECK(sc_editor_is_crop_active(ed));
    drag(ed, 10, 10, 110, 90);
    sc_editor_apply_crop(ed);
    CHECK(!sc_editor_is_crop_active(ed));
    CHECK(sc_editor_get_tool(ed) == SC_TOOL_PEN);
    int w = 0, h = 0;
    sc_editor_get_dimensions(ed, &w, &h);
    CHECK(w == 100 && h == 80);
    /* stroke at image (60,50) -> cropped (50,40) baked into the pixels */
    CHECK(is_strong_red(render_px(ed, 50, 40)));
    CHECK(sc_editor_can_undo(ed));
    sc_editor_undo(ed);
    sc_editor_get_dimensions(ed, &w, &h);
    CHECK(w == IMG_W && h == IMG_H);

    /* Tiny crop (< 8 px) is rejected without touching undo history */
    sc_editor_set_tool(ed, SC_TOOL_CROP);
    drag(ed, 10, 10, 14, 14);
    sc_editor_apply_crop(ed);
    sc_editor_get_dimensions(ed, &w, &h);
    CHECK(w == IMG_W && h == IMG_H);
    CHECK(sc_editor_is_crop_active(ed));

    /* Edge crop hugging the far border (P0-3 rounding clamp) never overflows */
    drag(ed, 150.5, 100.5, 250, 250);
    sc_editor_apply_crop(ed);
    sc_editor_get_dimensions(ed, &w, &h);
    CHECK(w > 0 && w <= 50 && h > 0 && h <= 50);

    sc_editor_set_tool(ed, SC_TOOL_CROP);
    sc_editor_cancel_crop(ed);
    CHECK(!sc_editor_is_crop_active(ed));
    CHECK(sc_editor_get_tool(ed) == SC_TOOL_PEN);
    free_canvas(ed);
}

static void test_undo_history_eviction(void) {
    ScEditorCanvas *ed = new_white_canvas();
    sc_editor_set_tool(ed, SC_TOOL_LINE);
    sc_editor_set_stroke_width(ed, 2.0);
    const int strokes = SC_MAX_HISTORY + 5; /* 45 */
    for (int i = 0; i < strokes; ++i) {
        double y = 2.0 + i * 3.0;
        drag(ed, 10, y, 190, y);
    }
    CHECK(is_strong_red(render_px(ed, 100, 2)));
    CHECK(is_strong_red(render_px(ed, 100, 2 + (strokes - 1) * 3)));

    int undos = 0;
    while (sc_editor_can_undo(ed)) {
        sc_editor_undo(ed);
        undos++;
        CHECK(undos <= SC_MAX_HISTORY);
    }
    CHECK(undos == SC_MAX_HISTORY);
    /* The oldest 5 strokes were evicted from history and therefore remain */
    for (int i = 0; i < strokes - SC_MAX_HISTORY; ++i) {
        CHECK(is_strong_red(render_px(ed, 100, 2 + i * 3)));
    }
    CHECK(is_white(render_px(ed, 100, 2 + (strokes - SC_MAX_HISTORY) * 3)));
    CHECK(is_white(render_px(ed, 100, 2 + (strokes - 1) * 3)));

    int redos = 0;
    while (sc_editor_can_redo(ed)) {
        sc_editor_redo(ed);
        redos++;
        CHECK(redos <= SC_MAX_HISTORY);
    }
    CHECK(redos == SC_MAX_HISTORY);
    CHECK(is_strong_red(render_px(ed, 100, 2 + (strokes - 1) * 3)));

    /* A new action after undo clears the redo stack */
    sc_editor_undo(ed);
    CHECK(sc_editor_can_redo(ed));
    drag(ed, 10, 148, 190, 148);
    CHECK(!sc_editor_can_redo(ed));
    free_canvas(ed);
}

static void test_ruler_snap(void) {
    ScEditorCanvas *ed = new_white_canvas();
    sc_editor_set_ruler_visible(ed, true);
    CHECK(sc_editor_get_ruler_visible(ed));
    /* Ruler fitted to 200x150: center (100,75), half-thickness 18 -> lower edge at y=93 */
    ScRuler expect = sc_fit_ruler_to_canvas(IMG_W, IMG_H, true, 0.0);
    CHECK(llround(expect.cx) == 100 && llround(expect.cy) == 75 && llround(expect.half_thickness) == 18);

    sc_editor_set_tool(ed, SC_TOOL_LINE);
    sc_editor_set_stroke_width(ed, 2.0);
    drag(ed, 60, 100, 140, 100); /* within 18 px snap threshold of the edge */
    CHECK(is_strong_red(render_px(ed, 100, 93)));
    CHECK(is_white(render_px(ed, 100, 100)));

    /* Ruler is an on-screen guide only: never baked into exported pixels */
    CHECK(is_white(render_px(ed, 100, 75)));

    /* Pressing inside the ruler drags it instead of drawing */
    GtkWidget *w = sc_editor_canvas_widget(ed);
    CHECK(emit_button(w, GDK_BUTTON_PRESS, 100, 75));
    emit_motion(w, 100, 40);
    emit_button(w, GDK_BUTTON_RELEASE, 100, 40);
    CHECK(is_white(render_px(ed, 100, 60)));

    /* Hidden ruler: no snapping */
    sc_editor_set_ruler_visible(ed, false);
    drag(ed, 20, 140, 60, 140);
    CHECK(is_strong_red(render_px(ed, 40, 140)));
    free_canvas(ed);
}

int main(int argc, char **argv) {
    if (!gtk_init_check(&argc, &argv)) {
        printf("SKIP: test_editor_gtk requires a display (run under xvfb-run).\n");
        return 0;
    }

    test_empty_and_state();
    test_pen_highlighter_shapes();
    test_eraser();
    test_pixelate_opaque_and_alpha();
    test_crop();
    test_undo_history_eviction();
    test_ruler_snap();

    printf("PASS: All %d GTK 3 / Cairo editor canvas checks succeeded (v%s).\n", g_checks, SNIPCLIP_VERSION);
    return 0;
}
