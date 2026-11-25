#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "config_store.h"
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
    bool has_gnome_screenshot;
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
    if (strcmp(bin, "gnome-screenshot") == 0) return m->has_gnome_screenshot;
    if (strcmp(bin, "wl-copy") == 0) return m->has_wl_copy;
    if (strcmp(bin, "xclip") == 0) return m->has_xclip;
    if (strcmp(bin, "xsel") == 0) return m->has_xsel;
    return false;
}

static ScDisplaySession detect_wayland_with(MockEnv *env) {
    env->xdg_session_type = "wayland";
    if (!env->wayland_display) env->wayland_display = "wayland-0";
    return sc_detect_display_session_custom(mock_getenv, mock_has_bin, NULL, NULL, NULL, env);
}

static void test_wayland_driver_priority_and_json(void) {
    /* wlroots desktop + grim -> grim first, even when the portal (gdbus) is available */
    MockEnv sway = { .desktop = "sway", .has_gdbus = true, .has_grim = true };
    ScDisplaySession s_sway = detect_wayland_with(&sway);
    assert(strcmp(s_sway.capture_driver, "grim (wlroots Wayland)") == 0);
    ScWaylandCaptureStep plan[SC_WL_CAPTURE_MAX_STEPS];
    size_t n = sc_wayland_capture_plan("sway", mock_has_bin, &sway, plan, SC_WL_CAPTURE_MAX_STEPS);
    assert(n == 3 && plan[0] == SC_WL_CAPTURE_GRIM && plan[1] == SC_WL_CAPTURE_PORTAL &&
           plan[2] == SC_WL_CAPTURE_GDK_ROOT);

    /* Case-insensitive substring matching (Hyprland, river, Wayfire) */
    assert(sc_desktop_is_wlroots("Hyprland"));
    assert(sc_desktop_is_wlroots("RIVER"));
    assert(sc_desktop_is_wlroots("wayfire:wlroots"));
    assert(!sc_desktop_is_wlroots("ubuntu:GNOME"));
    assert(!sc_desktop_is_wlroots(NULL));
    MockEnv hypr = { .desktop = "Hyprland", .has_gdbus = true, .has_grim = true };
    ScDisplaySession s_hypr = detect_wayland_with(&hypr);
    assert(strcmp(s_hypr.capture_driver, "grim (wlroots Wayland)") == 0);

    /* wlroots desktop WITHOUT grim -> portal */
    MockEnv sway_nogrim = { .desktop = "sway", .has_gdbus = true };
    ScDisplaySession s_sng = detect_wayland_with(&sway_nogrim);
    assert(strcmp(s_sng.capture_driver, "Native GDBus XDG Portal (org.freedesktop.portal.Screenshot)") == 0);

    /* GNOME + dbus + grim -> portal first, then grim -> spectacle -> gnome-screenshot -> GDK root */
    MockEnv gnome = { .desktop = "ubuntu:GNOME", .has_gdbus = true, .has_grim = true,
                      .has_spectacle = true, .has_gnome_screenshot = true };
    ScDisplaySession s_gnome = detect_wayland_with(&gnome);
    assert(strcmp(s_gnome.capture_driver, "Native GDBus XDG Portal (org.freedesktop.portal.Screenshot)") == 0);
    n = sc_wayland_capture_plan("ubuntu:GNOME", mock_has_bin, &gnome, plan, SC_WL_CAPTURE_MAX_STEPS);
    assert(n == 5);
    assert(plan[0] == SC_WL_CAPTURE_PORTAL && plan[1] == SC_WL_CAPTURE_GRIM &&
           plan[2] == SC_WL_CAPTURE_SPECTACLE && plan[3] == SC_WL_CAPTURE_GNOME_SCREENSHOT &&
           plan[4] == SC_WL_CAPTURE_GDK_ROOT);

    /* No dbus tooling: grim -> spectacle -> gnome-screenshot -> portal/GDK fallback */
    MockEnv kde_nodbus = { .desktop = "KDE", .has_grim = true, .has_spectacle = true };
    ScDisplaySession s_kn = detect_wayland_with(&kde_nodbus);
    assert(strcmp(s_kn.capture_driver, "grim (wlroots Wayland)") == 0);
    n = sc_wayland_capture_plan("KDE", mock_has_bin, &kde_nodbus, plan, SC_WL_CAPTURE_MAX_STEPS);
    assert(n == 4 && plan[0] == SC_WL_CAPTURE_GRIM && plan[1] == SC_WL_CAPTURE_SPECTACLE &&
           plan[2] == SC_WL_CAPTURE_PORTAL && plan[3] == SC_WL_CAPTURE_GDK_ROOT);
    MockEnv kde_spec = { .desktop = "KDE", .has_spectacle = true };
    assert(strcmp(detect_wayland_with(&kde_spec).capture_driver, "spectacle (KDE Wayland)") == 0);
    MockEnv gshot = { .desktop = "GNOME", .has_gnome_screenshot = true };
    assert(strcmp(detect_wayland_with(&gshot).capture_driver, "gnome-screenshot") == 0);
    MockEnv bare = { .desktop = "GNOME" };
    assert(strcmp(detect_wayland_with(&bare).capture_driver, "Native GDBus Portal / GDK Fallback") == 0);
    n = sc_wayland_capture_plan("GNOME", mock_has_bin, &bare, plan, SC_WL_CAPTURE_MAX_STEPS);
    assert(n == 2 && plan[0] == SC_WL_CAPTURE_PORTAL && plan[1] == SC_WL_CAPTURE_GDK_ROOT);

    /* xsel alone no longer claims PNG clipboard persistence */
    MockEnv x11_xsel = { .xdg_session_type = "x11", .display = ":0", .desktop = "XFCE", .has_xsel = true };
    ScDisplaySession s_xsel = sc_detect_display_session_custom(mock_getenv, mock_has_bin, NULL, NULL, NULL, &x11_xsel);
    assert(strcmp(s_xsel.clipboard_driver, "GtkClipboard (X11)") == 0);
    assert(strstr(s_xsel.clipboard_driver, "xsel") == NULL);

    /* JSON escaping of display names incl. quotes, backslashes and control characters */
    ScDisplaySession js = s_gnome;
    snprintf(js.wayland_display, sizeof(js.wayland_display), "%s", "wl\"0\\\n");
    snprintf(js.x11_display, sizeof(js.x11_display), "%s", ":0\x01");
    char json[4096];
    int len = sc_format_display_session_json(&js, json, sizeof(json));
    assert(len > 0 && (size_t)len < sizeof(json));
    assert(strstr(json, "\"wayland_display\": \"wl\\\"0\\\\\\u000a\"") != NULL);
    assert(strstr(json, "\"x11_display\": \":0\\u0001\"") != NULL);
    for (const char *p = json; *p; ++p) {
        assert(*p == '\n' || (unsigned char)*p >= 0x20);
    }
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

    /* 6. Reconcile headless with live GdkWaylandDisplay (including full Wayland driver fields) */
    ScDisplaySession s6 = sc_detect_display_session_custom(mock_getenv, mock_has_bin, NULL, NULL, NULL, &stripped);
    assert(s6.protocol == SC_PROTO_HEADLESS);
    sc_reconcile_with_gdk_backend_name(&s6, "GdkWaylandDisplay");
    assert(s6.protocol == SC_PROTO_WAYLAND);
    assert(strcmp(s6.protocol_name, "wayland") == 0);
    assert(strcmp(s6.gdk_backend, "wayland,x11") == 0);
    assert(s6.capture_driver[0] != '\0');
    assert(s6.clipboard_driver[0] != '\0');

    /* 7. Explicit DISPLAY env var wins over stale wayland-0 runtime socket when WAYLAND_DISPLAY is unset */
    MockEnv x11_stale_wl = {
        .xdg_session_type = "unspecified",
        .wayland_display = NULL,
        .display = ":0",
        .desktop = "XFCE",
        .has_xclip = true
    };
    ScDisplaySession s7 = sc_detect_display_session_custom(
        mock_getenv, mock_has_bin, "wayland-0", ":0", NULL, &x11_stale_wl
    );
    assert(s7.protocol == SC_PROTO_X11);
    assert(strcmp(s7.protocol_name, "x11") == 0);
    assert(strcmp(s7.detection_source, "DISPLAY=:0") == 0);
    assert(strcmp(s7.gdk_backend, "x11") == 0);
    assert(s7.supports_window_enumeration == true);

    /* 8. XWayland DISPLAY does not override Wayland when loginctl confirms session Type=wayland */
    MockEnv wl_via_loginctl = {
        .xdg_session_type = "unspecified",
        .wayland_display = "",
        .display = ":0",
        .desktop = "GNOME",
        .has_gdbus = true
    };
    ScDisplaySession s8 = sc_detect_display_session_custom(
        mock_getenv, mock_has_bin, "wayland-0", ":0", "wayland", &wl_via_loginctl
    );
    assert(s8.protocol == SC_PROTO_WAYLAND);
    assert(strcmp(s8.wayland_display, "wayland-0") == 0);
    assert(strcmp(s8.detection_source, "loginctl session Type=wayland") == 0);

    /* 9. Runtime GdkDisplay fallback from Wayland to GdkX11Display updates full X11 session metadata */
    ScDisplaySession s9 = s1;
    assert(s9.protocol == SC_PROTO_WAYLAND);
    sc_reconcile_with_gdk_backend_name(&s9, "GdkX11Display");
    assert(s9.protocol == SC_PROTO_X11);
    assert(strcmp(s9.protocol_name, "x11") == 0);
    assert(strcmp(s9.gdk_backend, "x11") == 0);
    assert(strcmp(s9.detection_source, "GdkDisplay=GdkX11Display") == 0);
    assert(strstr(s9.capture_driver, "X11") != NULL);
    assert(s9.supports_window_enumeration == true);

    /* 10. sc_bootstrap_display_env replaces empty string env vars instead of leaving them blank */
    const char *orig_disp = getenv("DISPLAY");
    char saved_disp[128] = "";
    bool had_disp = (orig_disp != NULL);
    if (orig_disp) {
        snprintf(saved_disp, sizeof(saved_disp), "%s", orig_disp);
    }
    setenv("DISPLAY", "", 1);
    sc_bootstrap_display_env(&s7);
    assert(getenv("DISPLAY") != NULL && strcmp(getenv("DISPLAY"), ":0") == 0);
    if (had_disp) {
        setenv("DISPLAY", saved_disp, 1);
    } else {
        unsetenv("DISPLAY");
    }
}

static void test_window_sizing_math(void) {
    int w = 0, h = 0;

    /* Empty launcher mode defaults to compact 460x200 */
    sc_compute_window_size(0, 0, 1920, 1080, &w, &h);
    assert(w == SC_LAUNCHER_WIDTH && w == 460);
    assert(h == SC_LAUNCHER_HEIGHT && h == 200);

    /* Small snip (200x120) clamps up to editor minimum 560x380 */
    sc_compute_window_size(200, 120, 1920, 1080, &w, &h);
    assert(w == SC_EDITOR_MIN_WIDTH && w == 560);
    assert(h == SC_EDITOR_MIN_HEIGHT && h == 380);

    /* Medium snip (700x450) adds chrome padding (700+48=748, 450+136=586) */
    sc_compute_window_size(700, 450, 1920, 1080, &w, &h);
    assert(w == 748 && h == 586);

    /* Huge 4K snip (3840x2160) on 1920x1080 workarea clamps to 80% (1536x864) */
    sc_compute_window_size(3840, 2160, 1920, 1080, &w, &h);
    assert(w == 1536 && h == 864);
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

    /* P0-3: independent rounding must never overshoot the pixbuf bounds (99.5 -> 100, extent 0.5 -> 1) */
    ScRect edge = sc_normalize_rect(99.5, 99.5, 100.0, 100.0, 100, 100);
    assert(edge.x == 100 && edge.y == 100);
    assert(edge.width == 0 && edge.height == 0);
    assert(edge.x + edge.width <= 100 && edge.y + edge.height <= 100);
    ScRect edge2 = sc_normalize_rect(98.5, 0.0, 250.0, 49.5, 100, 50);
    assert(edge2.x + edge2.width <= 100 && edge2.y + edge2.height <= 50);
    assert(edge2.x == 99 && edge2.width == 1);

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

static void test_hidpi_scale_rect(void) {
    ScRect logical = { 10, 20, 100, 50 };

    /* 1x: identity */
    ScRect s1 = sc_scale_rect_to_pixbuf(logical, 1000, 800, 1000, 800);
    assert(s1.x == 10 && s1.y == 20 && s1.width == 100 && s1.height == 50);

    /* 1.5x fractional HiDPI */
    ScRect s15 = sc_scale_rect_to_pixbuf(logical, 1000, 800, 1500, 1200);
    assert(s15.x == 15 && s15.y == 30 && s15.width == 150 && s15.height == 75);

    /* 2x Retina-class HiDPI */
    ScRect s2 = sc_scale_rect_to_pixbuf(logical, 1000, 800, 2000, 1600);
    assert(s2.x == 20 && s2.y == 40 && s2.width == 200 && s2.height == 100);

    /* 1.5x odd origin expands outward to whole physical pixels (7*1.5=10.5 -> 10, 20*1.5=30) */
    ScRect odd = { 7, 7, 13, 13 };
    ScRect s15b = sc_scale_rect_to_pixbuf(odd, 1000, 800, 1500, 1200);
    assert(s15b.x == 10 && s15b.y == 10 && s15b.width == 20 && s15b.height == 20);

    /* Reverse mapping (physical -> logical) and clamping at the far edge */
    ScRect phys = { 1900, 1500, 200, 200 };
    ScRect back = sc_scale_rect_to_pixbuf(phys, 2000, 1600, 1000, 800);
    assert(back.x == 950 && back.y == 750 && back.width == 50 && back.height == 50);
    assert(back.x + back.width <= 1000 && back.y + back.height <= 800);

    /* Degenerate logical size falls back to clamped normalization */
    ScRect deg = sc_scale_rect_to_pixbuf(logical, 0, 0, 50, 50);
    assert(deg.x == 10 && deg.y == 20 && deg.width == 40 && deg.height == 30);
}

static void make_temp_dir(char *out, size_t out_size) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp || tmp[0] != '/') tmp = "/tmp";
    snprintf(out, out_size, "%s/snipclip_cfg_test_XXXXXX", tmp);
    char *made = mkdtemp(out);
    assert(made != NULL);
}

static void remove_tree_simple(const char *dir) {
    /* Remove only the files this test creates (no recursion into unknown content) */
    char p[1024];
    snprintf(p, sizeof(p), "%s/snipclip/config.ini", dir);
    unlink(p);
    snprintf(p, sizeof(p), "%s/snipclip", dir);
    chmod(p, 0700);
    rmdir(p);
    snprintf(p, sizeof(p), "%s/ro/snipclip", dir);
    rmdir(p);
    snprintf(p, sizeof(p), "%s/ro", dir);
    chmod(p, 0700);
    rmdir(p);
    snprintf(p, sizeof(p), "%s/malformed.ini", dir);
    unlink(p);
    rmdir(dir);
}

static void test_config_persistence(void) {
    char path[1024];

    /* Path resolution: XDG_CONFIG_HOME wins, HOME/.config fallback, relative XDG ignored */
    assert(sc_config_resolve_path_custom("/xdg/cfg", "/home/u", path, sizeof(path)));
    assert(strcmp(path, "/xdg/cfg/snipclip/config.ini") == 0);
    assert(sc_config_resolve_path_custom(NULL, "/home/u", path, sizeof(path)));
    assert(strcmp(path, "/home/u/.config/snipclip/config.ini") == 0);
    assert(sc_config_resolve_path_custom("relative/cfg", "/home/u", path, sizeof(path)));
    assert(strcmp(path, "/home/u/.config/snipclip/config.ini") == 0);
    assert(!sc_config_resolve_path_custom("", "", path, sizeof(path)));
    assert(!sc_config_resolve_path_custom(NULL, NULL, path, sizeof(path)));

    ScConfig defaults;
    sc_config_init_defaults(&defaults);
    assert(defaults.snip_mode == SC_MODE_RECTANGLE);
    assert(defaults.delay_seconds == 0);
    assert(strcmp(defaults.custom_shortcut, "Ctrl+Shift+S") == 0);

    char dir[512];
    make_temp_dir(dir, sizeof(dir));

    /* Round-trip save/load creates the missing snipclip/ parent directory with 0700 */
    assert(sc_config_resolve_path_custom(dir, NULL, path, sizeof(path)));
    ScConfig cfg;
    sc_config_init_defaults(&cfg);
    cfg.snip_mode = SC_MODE_FREEFORM;
    cfg.delay_seconds = 5;
    snprintf(cfg.custom_shortcut, sizeof(cfg.custom_shortcut), "%s", "<Control><Alt>p");
    assert(sc_config_save(path, &cfg));

    char parent[1024];
    snprintf(parent, sizeof(parent), "%s/snipclip", dir);
    struct stat st;
    assert(stat(parent, &st) == 0 && S_ISDIR(st.st_mode));
    assert((st.st_mode & 0777) == 0700);
    assert(stat(path, &st) == 0 && (st.st_mode & 0077) == 0);

    ScConfig loaded;
    sc_config_init_defaults(&loaded);
    assert(sc_config_load(path, &loaded));
    assert(loaded.snip_mode == SC_MODE_FREEFORM);
    assert(loaded.delay_seconds == 5);
    assert(strcmp(loaded.custom_shortcut, "<Control><Alt>p") == 0);

    /* Overwrite atomically with new values */
    cfg.snip_mode = SC_MODE_WINDOW;
    cfg.delay_seconds = 10;
    assert(sc_config_save(path, &cfg));
    sc_config_init_defaults(&loaded);
    assert(sc_config_load(path, &loaded));
    assert(loaded.snip_mode == SC_MODE_WINDOW && loaded.delay_seconds == 10);

    /* Invalid values are rejected on save and leave the previous file intact */
    ScConfig bad = cfg;
    bad.delay_seconds = 7;
    assert(!sc_config_save(path, &bad));
    sc_config_init_defaults(&loaded);
    assert(sc_config_load(path, &loaded));
    assert(loaded.delay_seconds == 10);

    /* Malformed INI resilience: garbage, bad values, foreign sections, over-long lines */
    char malformed[1024];
    snprintf(malformed, sizeof(malformed), "%s/malformed.ini", dir);
    FILE *fp = fopen(malformed, "w");
    assert(fp != NULL);
    fputs("this is not ini\n", fp);
    fputs("=novalue\n", fp);
    fputs("[other]\nsnip_mode=fullscreen\n", fp);
    fputs("[snipclip\n", fp);
    fputs("snip_mode=window\n", fp); /* inside a broken section header -> ignored */
    fputs("[ snipclip ]\n", fp);
    fputs("snip_mode = banana\n", fp);
    fputs("delay_seconds=7\n", fp);
    fputs("delay_seconds=3abc\n", fp);
    fputs("custom_shortcut=\n", fp);
    fputs("custom_shortcut=", fp);
    for (int i = 0; i < 700; ++i) fputc('A', fp);
    fputs("\n", fp);
    fputs("  delay_seconds = 3  \n", fp);
    fputs("; comment\n# comment\n", fp);
    fclose(fp);

    ScConfig m;
    sc_config_init_defaults(&m);
    assert(sc_config_load(malformed, &m));
    assert(m.snip_mode == SC_MODE_RECTANGLE);
    assert(m.delay_seconds == 3);
    assert(strcmp(m.custom_shortcut, "Ctrl+Shift+S") == 0);

    /* Missing file degrades silently and keeps defaults */
    char missing[1024];
    snprintf(missing, sizeof(missing), "%s/does-not-exist/config.ini", dir);
    ScConfig miss;
    sc_config_init_defaults(&miss);
    assert(!sc_config_load(missing, &miss));
    assert(miss.snip_mode == SC_MODE_RECTANGLE && miss.delay_seconds == 0);
    assert(!sc_config_load(NULL, &miss));
    assert(!sc_config_save(NULL, &cfg));

    /* Read-only parent directory: save fails cleanly (unless running as root) without crashing */
    char ro_dir[1024];
    snprintf(ro_dir, sizeof(ro_dir), "%s/ro", dir);
    assert(mkdir(ro_dir, 0500) == 0);
    char ro_path[1024];
    assert(sc_config_resolve_path_custom(ro_dir, NULL, ro_path, sizeof(ro_path)));
    bool saved_ro = sc_config_save(ro_path, &cfg);
    if (geteuid() != 0) {
        assert(!saved_ro);
        ScConfig ro_cfg;
        sc_config_init_defaults(&ro_cfg);
        assert(!sc_config_load(ro_path, &ro_cfg));
        assert(ro_cfg.snip_mode == SC_MODE_RECTANGLE);
    } else if (saved_ro) {
        unlink(ro_path);
    }

    remove_tree_simple(dir);
}

int main(void) {
    test_display_detection_cascade();
    test_window_sizing_math();
    test_launcher_path_normalization();
    test_geometry_and_ruler_math();
    test_hidpi_scale_rect();
    test_wayland_driver_priority_and_json();
    test_config_persistence();
    printf("PASS: All native C SnipClip unit tests succeeded (v%s).\n", SNIPCLIP_VERSION);
    return 0;
}
