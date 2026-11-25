#ifndef SNIPCLIP_DISPLAY_DETECT_H
#define SNIPCLIP_DISPLAY_DETECT_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SC_PROTO_HEADLESS = 0,
    SC_PROTO_WAYLAND = 1,
    SC_PROTO_X11 = 2
} ScDisplayProtocol;

typedef struct {
    ScDisplayProtocol protocol;
    char protocol_name[16];
    char detection_source[128];
    char gdk_backend[32];
    char desktop_environment[64];
    char wayland_display[64];
    char x11_display[64];
    char capture_driver[96];
    char clipboard_driver[64];
    bool supports_window_enumeration;
} ScDisplaySession;

typedef const char *(*ScGetenvFn)(const char *name, void *user_data);
typedef bool (*ScHasBinaryFn)(const char *bin_name, void *user_data);

/* Injectable detector for deterministic C unit tests */
ScDisplaySession sc_detect_display_session_custom(
    ScGetenvFn getenv_fn,
    ScHasBinaryFn has_bin_fn,
    const char *discovered_wayland_sock,
    const char *discovered_x11_sock,
    const char *loginctl_type,
    void *user_data
);

/* Live OS detector executed automatically on native GTK app startup */
ScDisplaySession sc_detect_display_session(void);

/* Configure GDK_BACKEND, WAYLAND_DISPLAY, XDG_RUNTIME_DIR, and DISPLAY before gtk_init() */
void sc_bootstrap_display_env(const ScDisplaySession *session);

/* Reconcile detected session with active GdkDisplay runtime name after gtk_init() */
void sc_reconcile_with_gdk_backend_name(ScDisplaySession *session, const char *gdk_type_name);

/* Serialize session telemetry as JSON (all string fields escaped, incl. control chars as \u00XX) */
int sc_format_display_session_json(const ScDisplaySession *session, char *buf, size_t buf_size);

/* Normalize desktop launcher file path or file:// URI (ignoring blank or %f/%u placeholders) */
bool sc_normalize_launcher_path(const char *raw_arg, char *out_path, size_t out_size);

/* PATH lookup for an executable regular file (ScHasBinaryFn-compatible; user_data unused) */
bool sc_has_binary_in_path(const char *bin_name, void *user_data);

/* Case-insensitive substring match of sway / hyprland / river / wayfire in XDG_CURRENT_DESKTOP */
bool sc_desktop_is_wlroots(const char *desktop_environment);

typedef enum {
    SC_WL_CAPTURE_GRIM = 0,
    SC_WL_CAPTURE_PORTAL = 1,
    SC_WL_CAPTURE_SPECTACLE = 2,
    SC_WL_CAPTURE_GNOME_SCREENSHOT = 3,
    SC_WL_CAPTURE_GDK_ROOT = 4
} ScWaylandCaptureStep;

#define SC_WL_CAPTURE_MAX_STEPS 5

/* Ordered Wayland full-desktop capture plan shared by capture_driver telemetry and the runtime
 * capture backend (1:1 alignment):
 *   - wlroots desktop + grim     -> grim first
 *   - dbus-daemon/gdbus present  -> XDG portal -> grim -> spectacle -> gnome-screenshot
 *   - no dbus tooling            -> grim -> spectacle -> gnome-screenshot -> XDG portal
 *   - GDK root window always last.
 * Only installed CLI helpers are included. Returns the number of steps written. */
size_t sc_wayland_capture_plan(
    const char *desktop_environment,
    ScHasBinaryFn has_bin_fn,
    void *user_data,
    ScWaylandCaptureStep *out_steps,
    size_t max_steps
);

#endif /* SNIPCLIP_DISPLAY_DETECT_H */
