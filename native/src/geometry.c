#include "geometry.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double clamp_double(double val, double lo, double hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

static int clamp_int(int val, int lo, int hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

ScRect sc_normalize_rect(double x1, double y1, double x2, double y2, int max_w, int max_h) {
    double min_x = x1 < x2 ? x1 : x2;
    double max_x = x1 > x2 ? x1 : x2;
    double min_y = y1 < y2 ? y1 : y2;
    double max_y = y1 > y2 ? y1 : y2;

    if (max_w > 0) {
        min_x = clamp_double(min_x, 0.0, (double)max_w);
        max_x = clamp_double(max_x, 0.0, (double)max_w);
    }
    if (max_h > 0) {
        min_y = clamp_double(min_y, 0.0, (double)max_h);
        max_y = clamp_double(max_y, 0.0, (double)max_h);
    }

    int rx = (int)llround(min_x);
    int ry = (int)llround(min_y);
    int rw = (int)llround(max_x - min_x);
    int rh = (int)llround(max_y - min_y);

    /* Independent rounding of origin and extent can overshoot the bounds by one pixel
     * (e.g. min_x=99.5, max_x=100.0, max_w=100 -> rx=100, rw=1). Clamp the final
     * integer rectangle so rx + rw <= max_w and ry + rh <= max_h always hold. */
    if (max_w > 0) {
        rx = clamp_int(rx, 0, max_w);
        if (rx + rw > max_w) rw = max_w - rx;
    }
    if (max_h > 0) {
        ry = clamp_int(ry, 0, max_h);
        if (ry + rh > max_h) rh = max_h - ry;
    }
    if (rw < 0) rw = 0;
    if (rh < 0) rh = 0;

    ScRect r = { rx, ry, rw, rh };
    return r;
}

ScRect sc_scale_rect_to_pixbuf(ScRect logical_rect, int logical_w, int logical_h, int pixbuf_w, int pixbuf_h) {
    if (logical_w <= 0 || logical_h <= 0 || pixbuf_w <= 0 || pixbuf_h <= 0) {
        return sc_normalize_rect(
            logical_rect.x,
            logical_rect.y,
            (double)logical_rect.x + logical_rect.width,
            (double)logical_rect.y + logical_rect.height,
            pixbuf_w,
            pixbuf_h
        );
    }

    double sx = (double)pixbuf_w / (double)logical_w;
    double sy = (double)pixbuf_h / (double)logical_h;

    /* Expand outward (floor origin, ceil far edge) so no selected physical pixel is lost */
    double x1 = floor((double)logical_rect.x * sx + 1e-9);
    double y1 = floor((double)logical_rect.y * sy + 1e-9);
    double x2 = ceil(((double)logical_rect.x + logical_rect.width) * sx - 1e-9);
    double y2 = ceil(((double)logical_rect.y + logical_rect.height) * sy - 1e-9);

    int ix1 = clamp_int((int)x1, 0, pixbuf_w);
    int iy1 = clamp_int((int)y1, 0, pixbuf_h);
    int ix2 = clamp_int((int)x2, 0, pixbuf_w);
    int iy2 = clamp_int((int)y2, 0, pixbuf_h);

    ScRect r = { ix1, iy1, ix2 > ix1 ? ix2 - ix1 : 0, iy2 > iy1 ? iy2 - iy1 : 0 };
    return r;
}

bool sc_polygon_bbox(const ScPoint *pts, size_t count, int max_w, int max_h, ScRect *out_rect) {
    if (!pts || count < 3 || !out_rect) {
        return false;
    }

    double min_x = pts[0].x;
    double max_x = pts[0].x;
    double min_y = pts[0].y;
    double max_y = pts[0].y;

    for (size_t i = 1; i < count; ++i) {
        if (pts[i].x < min_x) min_x = pts[i].x;
        if (pts[i].x > max_x) max_x = pts[i].x;
        if (pts[i].y < min_y) min_y = pts[i].y;
        if (pts[i].y > max_y) max_y = pts[i].y;
    }

    int x1 = clamp_int((int)floor(min_x), 0, max_w);
    int y1 = clamp_int((int)floor(min_y), 0, max_h);
    int x2 = clamp_int((int)ceil(max_x), 0, max_w);
    int y2 = clamp_int((int)ceil(max_y), 0, max_h);

    int w = x2 - x1;
    int h = y2 - y1;
    if (w < 8 || h < 8) {
        return false;
    }

    out_rect->x = x1;
    out_rect->y = y1;
    out_rect->width = w;
    out_rect->height = h;
    return true;
}

bool sc_point_in_polygon(double px, double py, const ScPoint *pts, size_t count) {
    if (!pts || count < 3) {
        return false;
    }
    bool inside = false;
    for (size_t i = 0, j = count - 1; i < count; j = i++) {
        double xi = pts[i].x, yi = pts[i].y;
        double xj = pts[j].x, yj = pts[j].y;
        bool intersect = ((yi > py) != (yj > py)) &&
                         (px < (xj - xi) * (py - yi) / ((yj - yi) + 1e-12) + xi);
        if (intersect) {
            inside = !inside;
        }
    }
    return inside;
}

void sc_compute_arrow_wings(
    double x1,
    double y1,
    double x2,
    double y2,
    double stroke_width,
    ScPoint *out_left,
    ScPoint *out_right
) {
    double angle = atan2(y2 - y1, x2 - x1);
    double head_len = fmax(12.0, stroke_width * 4.2);
    double spread = M_PI / 6.5;

    if (out_left) {
        out_left->x = x2 - head_len * cos(angle - spread);
        out_left->y = y2 - head_len * sin(angle - spread);
    }
    if (out_right) {
        out_right->x = x2 - head_len * cos(angle + spread);
        out_right->y = y2 - head_len * sin(angle + spread);
    }
}

ScRuler sc_fit_ruler_to_canvas(int canvas_w, int canvas_h, bool visible, double angle_deg) {
    int safe_w = canvas_w < 80 ? 80 : canvas_w;
    int safe_h = canvas_h < 80 ? 80 : canvas_h;
    double length = fmin(380.0, fmax(140.0, (double)safe_w - 56.0));
    double half_t = fmin(26.0, fmax(18.0, (double)safe_h * 0.10));

    ScRuler r;
    r.visible = visible;
    r.cx = round((double)safe_w / 2.0);
    r.cy = round((double)safe_h / 2.0);
    r.angle_deg = fmod(angle_deg, 360.0);
    if (r.angle_deg < 0.0) r.angle_deg += 360.0;
    r.length = round(length);
    r.half_thickness = round(half_t);
    return r;
}

bool sc_point_inside_ruler(double px, double py, const ScRuler *ruler) {
    if (!ruler || !ruler->visible) {
        return false;
    }
    double rad = ruler->angle_deg * (M_PI / 180.0);
    double dx = cos(rad);
    double dy = sin(rad);
    double nx = -dy;
    double ny = dx;

    double rx = px - ruler->cx;
    double ry = py - ruler->cy;
    double along = rx * dx + ry * dy;
    double perp = rx * nx + ry * ny;

    return fabs(along) <= (ruler->length / 2.0) && fabs(perp) <= ruler->half_thickness;
}

ScPoint sc_snap_point_to_ruler(double px, double py, const ScRuler *ruler, double snap_threshold) {
    ScPoint orig = { px, py };
    if (!ruler || !ruler->visible) {
        return orig;
    }

    double rad = ruler->angle_deg * (M_PI / 180.0);
    double dx = cos(rad);
    double dy = sin(rad);
    double nx = -dy;
    double ny = dx;

    double rx = px - ruler->cx;
    double ry = py - ruler->cy;
    double along = rx * dx + ry * dy;
    double perp = rx * nx + ry * ny;

    if (fabs(along) > (ruler->length / 2.0) + 16.0) {
        return orig;
    }

    if (fabs(perp - ruler->half_thickness) <= snap_threshold) {
        ScPoint snapped = {
            ruler->cx + along * dx + ruler->half_thickness * nx,
            ruler->cy + along * dy + ruler->half_thickness * ny
        };
        return snapped;
    }
    if (fabs(perp + ruler->half_thickness) <= snap_threshold) {
        ScPoint snapped = {
            ruler->cx + along * dx - ruler->half_thickness * nx,
            ruler->cy + along * dy - ruler->half_thickness * ny
        };
        return snapped;
    }
    return orig;
}

double sc_distance_to_segment(double px, double py, double ax, double ay, double bx, double by) {
    double vx = bx - ax;
    double vy = by - ay;
    double l2 = vx * vx + vy * vy;
    if (l2 <= 1e-12) {
        return hypot(px - ax, py - ay);
    }
    double t = ((px - ax) * vx + (py - ay) * vy) / l2;
    t = clamp_double(t, 0.0, 1.0);
    double proj_x = ax + t * vx;
    double proj_y = ay + t * vy;
    return hypot(px - proj_x, py - proj_y);
}

bool sc_hit_test_annotation(
    double px,
    double py,
    ScToolKind kind,
    const ScPoint *pts,
    size_t count,
    double threshold
) {
    if (!pts || count == 0) {
        return false;
    }
    if (count == 1) {
        return hypot(px - pts[0].x, py - pts[0].y) <= threshold;
    }

    if (kind == SC_TOOL_PEN || kind == SC_TOOL_HIGHLIGHTER) {
        for (size_t i = 0; i + 1 < count; ++i) {
            if (sc_distance_to_segment(px, py, pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y) <= threshold) {
                return true;
            }
        }
        return false;
    }

    ScPoint a = pts[0];
    ScPoint b = pts[count - 1];
    if (kind == SC_TOOL_LINE || kind == SC_TOOL_ARROW) {
        return sc_distance_to_segment(px, py, a.x, a.y, b.x, b.y) <= threshold;
    }

    double min_x = fmin(a.x, b.x) - threshold;
    double max_x = fmax(a.x, b.x) + threshold;
    double min_y = fmin(a.y, b.y) - threshold;
    double max_y = fmax(a.y, b.y) + threshold;
    return px >= min_x && px <= max_x && py >= min_y && py <= max_y;
}

void sc_compute_window_size(
    int image_w,
    int image_h,
    int workarea_w,
    int workarea_h,
    int *out_w,
    int *out_h
) {
    int w = SC_LAUNCHER_WIDTH;
    int h = SC_LAUNCHER_HEIGHT;

    if (image_w <= 0 || image_h <= 0) {
        if (workarea_w > 120 && workarea_w - 32 < w) {
            w = workarea_w - 32;
        }
        if (workarea_h > 120 && workarea_h - 32 < h) {
            h = workarea_h - 32;
        }
    } else {
        int max_w = (workarea_w > 0) ? (int)(workarea_w * 0.80) : 1100;
        int max_h = (workarea_h > 0) ? (int)(workarea_h * 0.80) : 760;
        if (max_w < 320) max_w = 320;
        if (max_h < 240) max_h = 240;

        w = image_w + SC_EDITOR_CHROME_W;
        if (max_w < SC_EDITOR_MIN_WIDTH) {
            w = max_w;
        } else {
            w = clamp_int(w, SC_EDITOR_MIN_WIDTH, max_w);
        }

        h = image_h + SC_EDITOR_CHROME_H;
        if (max_h < SC_EDITOR_MIN_HEIGHT) {
            h = max_h;
        } else {
            h = clamp_int(h, SC_EDITOR_MIN_HEIGHT, max_h);
        }
    }

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}

const char *sc_snip_mode_to_string(ScSnipMode mode) {
    switch (mode) {
        case SC_MODE_WINDOW: return "window";
        case SC_MODE_FULLSCREEN: return "fullscreen";
        case SC_MODE_FREEFORM: return "freeform";
        case SC_MODE_RECTANGLE:
        default: return "rectangle";
    }
}

bool sc_snip_mode_from_string(const char *str, ScSnipMode *out_mode) {
    if (!str || !out_mode) return false;
    if (strcmp(str, "rectangle") == 0) {
        *out_mode = SC_MODE_RECTANGLE;
        return true;
    }
    if (strcmp(str, "window") == 0) {
        *out_mode = SC_MODE_WINDOW;
        return true;
    }
    if (strcmp(str, "fullscreen") == 0) {
        *out_mode = SC_MODE_FULLSCREEN;
        return true;
    }
    if (strcmp(str, "freeform") == 0) {
        *out_mode = SC_MODE_FREEFORM;
        return true;
    }
    return false;
}
