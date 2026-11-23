#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "display_detect.h"
#include "geometry.h"
#include "snipclip_version.h"

typedef struct {
    const char *xdg_session_type;
    const char *wayland_display;
    const char *display;
    const char *desktop;
    bool has_gdbus;
    bool has_grim;
    bool has_spectacle;
    bool has_wl_copy;
    bool has_xclip;
    bool has_xsel;
} MockEnv;

static const char *mock_getenv(const char *name, void *ud) {
    const MockEnv *m = (const MockEnv *)ud;
    if (strcmp(name, "XDG_SESSION_TYPE") == 0) return m->xdg_session_type;
    if (strcmp(name, "WAYLAND_DISPLAY") == 0) return m->wayland_display;
    if (strcmp(name, "DISPLAY") == 0) return m->display;
    if (strcmp(name, "XDG_CURRENT_DESKTOP") == 0) return m->desktop;
    return NULL;
}

static bool mock_has_bin(const char *bin, void *ud) {
    const MockEnv *m = (const MockEnv *)ud;
    if (strcmp(bin, "gdbus") == 0 || strcmp(bin, "dbus-daemon") == 0) return m->has_gdbus;
    if (strcmp(bin, "grim") == 0) return m->has_grim;
    if (strcmp(bin, "spectacle") == 0) return m->has_spectacle;
    if (strcmp(bin, "wl-copy") == 0) return m->has_wl_copy;
    if (strcmp(bin, "xclip") == 0) return m->has_xclip;
    if (strcmp(bin, "xsel") == 0) return m->has_xsel;
    return false;
}

static void test_display_detection_cascade(void) {
    /* 1. Explicit Wayland session with KDE Plasma */
    MockEnv kde_wl = {
        .xdg_session_type = "  Wayland \n",
        .wayland_display = "wayland-0",
        .display = ":1",
        .desktop = "KDE",
        .has_gdbus = true,
        .has_wl_copy = true
    };
    ScDisplaySession s1 = sc_detect_display_session_custom(mock_getenv, mock_has_bin, NULL, NULL, NULL, &kde_wl);
    assert(s1.protocol == SC_PROTO_WAYLAND);
    assert(strcmp(s1.protocol_name, "wayland") == 0);
    assert(strcmp(s1.detection_source, "XDG_SESSION_TYPE=wayland") == 0);
    assert(strcmp(s1.gdk_backend, "wayland,x11") == 0);
    assert(strstr(s1.capture_driver, "Portal") != NULL);
    assert(strcmp(s1.clipboard_driver, "GtkClipboard + wl-copy") == 0);

    /* 2. XWayland ambiguity prefers WAYLAND_DISPLAY over DISPLAY when XDG_SESSION_TYPE unset */
    MockEnv sway_wl = {
        .xdg_session_type = "",
        .wayland_display = "wayland-1",
        .display = ":0",
        .desktop = "sway",
        .has_grim = true
    };
    ScDisplaySession s2 = sc_detect_display_session_custom(mock_getenv, mock_has_bin, NULL, NULL, NULL, &sway_wl);
    assert(s2.protocol == SC_PROTO_WAYLAND);
    assert(strcmp(s2.detection_source, "WAYLAND_DISPLAY=wayland-1") == 0);
    assert(strstr(s2.capture_driver, "grim") != NULL);

    /* 3. Stripped GUI installer environment discovers /run/user/<uid>/wayland-0 socket */
    MockEnv stripped = { 0 };
    ScDisplaySession s3 = sc_detect_display_session_custom(
        mock_getenv, mock_has_bin, "wayland-0", ":0", NULL, &stripped
    );
    assert(s3.protocol == SC_PROTO_WAYLAND);
    assert(strcmp(s3.wayland_display, "wayland-0") == 0);

    /* 4. Stripped GUI installer environment falls back to loginctl then X11 socket */
    ScDisplaySession s4 = sc_detect_display_session_custom(
        mock_getenv, mock_has_bin, NULL, ":0", "x11", &stripped
    );
    assert(s4.protocol == SC_PROTO_X11);
    assert(strcmp(s4.x11_display, ":0") == 0);
    assert(s4.supports_window_enumeration == true);

    /* 5. Explicit X11 session */
    MockEnv xfce_x11 = {
        .xdg_session_type = "X11",
        .display = ":0.0",
        .desktop = "XFCE",
        .has_xclip = true
    };
    ScDisplaySession s5 = sc_detect_display_session_custom(mock_getenv, mock_has_bin, NULL, NULL, NULL, &xfce_x11);
    assert(s5.protocol == SC_PROTO_X11);
    assert(strcmp(s5.gdk_backend, "x11") == 0);
    assert(strcmp(s5.clipboard_driver, "GtkClipboard + xclip") == 0);

    /* 6. Reconcile headless with live GdkWaylandDisplay */
    ScDisplaySession s6 = sc_detect_display_session_custom(mock_getenv, mock_has_bin, NULL, NULL, NULL, &stripped);
    assert(s6.protocol == SC_PROTO_HEADLESS);
    sc_reconcile_with_gdk_backend_name(&s6, "GdkWaylandDisplay");
    assert(s6.protocol == SC_PROTO_WAYLAND);
}

static void test_launcher_path_normalization(void) {
    char out[256];
    assert(!sc_normalize_launcher_path(NULL, out, sizeof(out)));
    assert(!sc_normalize_launcher_path("   ", out, sizeof(out)));
    assert(!sc_normalize_launcher_path("%f", out, sizeof(out)));
    assert(!sc_normalize_launcher_path("%U", out, sizeof(out)));

    assert(sc_normalize_launcher_path("file:///home/user/Pictures/My%20Snip.png", out, sizeof(out)));
    assert(strcmp(out, "/home/user/Pictures/My Snip.png") == 0);

    assert(sc_normalize_launcher_path("/tmp/capture.png", out, sizeof(out)));
    assert(strcmp(out, "/tmp/capture.png") == 0);
}

static void test_geometry_and_ruler_math(void) {
    ScRect r = sc_normalize_rect(350.0, 280.0, 100.0, -20.0, 1120, 640);
    assert(r.x == 100 && r.y == 0 && r.width == 250 && r.height == 280);

    ScPoint tri[3] = { { 40.0, 50.0 }, { 240.0, 65.0 }, { 190.0, 210.0 } };
    ScRect bbox;
    assert(sc_polygon_bbox(tri, 3, 800, 600, &bbox));
    assert(bbox.x == 40 && bbox.y == 50 && bbox.width == 200 && bbox.height == 160);
    assert(sc_point_in_polygon(160.0, 110.0, tri, 3));
    assert(!sc_point_in_polygon(10.0, 10.0, tri, 3));

    ScRuler ruler = { true, 300.0, 200.0, 0.0, 420.0, 28.0 };
    assert(sc_point_inside_ruler(310.0, 210.0, &ruler));
    ScPoint snapped = sc_snap_point_to_ruler(340.0, 235.0, &ruler, 16.0);
    assert(llround(snapped.x) == 340 && llround(snapped.y) == 228);

    ScPoint arrow_pts[2] = { { 50.0, 50.0 }, { 250.0, 50.0 } };
    assert(sc_hit_test_annotation(150.0, 56.0, SC_TOOL_ARROW, arrow_pts, 2, 12.0));
    assert(!sc_hit_test_annotation(150.0, 95.0, SC_TOOL_ARROW, arrow_pts, 2, 12.0));
}

int main(void) {
    test_display_detection_cascade();
    test_launcher_path_normalization();
    test_geometry_and_ruler_math();
    printf("PASS: All native C SnipClip unit tests succeeded (v%s).\n", SNIPCLIP_VERSION);
    return 0;
}
