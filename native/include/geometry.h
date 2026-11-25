#ifndef SNIPCLIP_GEOMETRY_H
#define SNIPCLIP_GEOMETRY_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SC_MODE_RECTANGLE = 0,
    SC_MODE_WINDOW = 1,
    SC_MODE_FULLSCREEN = 2,
    SC_MODE_FREEFORM = 3
} ScSnipMode;

typedef enum {
    SC_TOOL_PEN = 0,
    SC_TOOL_HIGHLIGHTER = 1,
    SC_TOOL_ERASER = 2,
    SC_TOOL_RECT = 3,
    SC_TOOL_ELLIPSE = 4,
    SC_TOOL_LINE = 5,
    SC_TOOL_ARROW = 6,
    SC_TOOL_REDACT = 7,
    SC_TOOL_PIXELATE = 8,
    SC_TOOL_CROP = 9
} ScToolKind;

typedef struct {
    double x;
    double y;
} ScPoint;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} ScRect;

typedef struct {
    bool visible;
    double cx;
    double cy;
    double angle_deg;
    double length;
    double half_thickness;
} ScRuler;

ScRect sc_normalize_rect(double x1, double y1, double x2, double y2, int max_w, int max_h);

/* Map a rectangle from a logical (widget / overlay) coordinate space of logical_w x logical_h
 * into the physical pixel space of a pixbuf_w x pixbuf_h pixbuf (HiDPI scale compensation).
 * The result is expanded outward to whole pixels and clamped to the pixbuf bounds.
 * Works in either direction (pass physical dims as "logical" to map physical -> logical). */
ScRect sc_scale_rect_to_pixbuf(ScRect logical_rect, int logical_w, int logical_h, int pixbuf_w, int pixbuf_h);

bool sc_polygon_bbox(const ScPoint *pts, size_t count, int max_w, int max_h, ScRect *out_rect);

bool sc_point_in_polygon(double px, double py, const ScPoint *pts, size_t count);

void sc_compute_arrow_wings(
    double x1,
    double y1,
    double x2,
    double y2,
    double stroke_width,
    ScPoint *out_left,
    ScPoint *out_right
);

ScRuler sc_fit_ruler_to_canvas(int canvas_w, int canvas_h, bool visible, double angle_deg);

bool sc_point_inside_ruler(double px, double py, const ScRuler *ruler);

ScPoint sc_snap_point_to_ruler(double px, double py, const ScRuler *ruler, double snap_threshold);

double sc_distance_to_segment(double px, double py, double ax, double ay, double bx, double by);

bool sc_hit_test_annotation(
    double px,
    double py,
    ScToolKind kind,
    const ScPoint *pts,
    size_t count,
    double threshold
);

#define SC_LAUNCHER_WIDTH 460
#define SC_LAUNCHER_HEIGHT 200
#define SC_EDITOR_MIN_WIDTH 560
#define SC_EDITOR_MIN_HEIGHT 380
#define SC_EDITOR_CHROME_W 48
#define SC_EDITOR_CHROME_H 136

void sc_compute_window_size(
    int image_w,
    int image_h,
    int workarea_w,
    int workarea_h,
    int *out_w,
    int *out_h
);

const char *sc_snip_mode_to_string(ScSnipMode mode);
bool sc_snip_mode_from_string(const char *str, ScSnipMode *out_mode);

#endif /* SNIPCLIP_GEOMETRY_H */
