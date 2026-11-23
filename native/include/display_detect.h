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

/* Serialize session telemetry as JSON */
int sc_format_display_session_json(const ScDisplaySession *session, char *buf, size_t buf_size);

/* Normalize desktop launcher file path or file:// URI (ignoring blank or %f/%u placeholders) */
bool sc_normalize_launcher_path(const char *raw_arg, char *out_path, size_t out_size);

#endif /* SNIPCLIP_DISPLAY_DETECT_H */
