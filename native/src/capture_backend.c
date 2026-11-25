#define _POSIX_C_SOURCE 200809L

#include "capture_backend.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <errno.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int ignore_x11_errors(Display *dpy, XErrorEvent *ev) {
    (void)dpy;
    (void)ev;
    return 0;
}

typedef struct {
    GMainLoop *loop;
    guint response_code;
    char *uri;
    bool completed;
} PortalWaitState;

static void on_portal_response_signal(
    GDBusConnection *connection,
    const gchar *sender_name,
    const gchar *object_path,
    const gchar *interface_name,
    const gchar *signal_name,
    GVariant *parameters,
    gpointer user_data
) {
    (void)connection;
    (void)sender_name;
    (void)object_path;
    (void)interface_name;
    (void)signal_name;

    PortalWaitState *st = (PortalWaitState *)user_data;
    if (!st || st->completed) return;

    guint32 resp = 2;
    GVariant *results = NULL;
    g_variant_get(parameters, "(u@a{sv})", &resp, &results);
    st->response_code = resp;

    if (resp == 0 && results != NULL) {
        const gchar *uri_str = NULL;
        if (g_variant_lookup(results, "uri", "&s", &uri_str) && uri_str != NULL) {
            st->uri = g_strdup(uri_str);
        }
    }
    if (results != NULL) {
        g_variant_unref(results);
    }
    st->completed = true;
    if (st->loop && g_main_loop_is_running(st->loop)) {
        g_main_loop_quit(st->loop);
    }
}

static gboolean on_portal_timeout(gpointer user_data) {
    PortalWaitState *st = (PortalWaitState *)user_data;
    if (st && st->loop && g_main_loop_is_running(st->loop)) {
        g_main_loop_quit(st->loop);
    }
    return G_SOURCE_REMOVE;
}

static GdkPixbuf *capture_via_xdg_portal_gdbus(bool interactive) {
    GError *err = NULL;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
    if (!bus) {
        if (err) g_error_free(err);
        return NULL;
    }

    const gchar *unique_name = g_dbus_connection_get_unique_name(bus);
    if (!unique_name || unique_name[0] != ':') {
        g_object_unref(bus);
        return NULL;
    }

    /* Convert ":1.84" -> "1_84" per XDG Desktop Portal Request path specification */
    char sender_sanitized[128];
    snprintf(sender_sanitized, sizeof(sender_sanitized), "%s", unique_name + 1);
    for (size_t i = 0; sender_sanitized[i] != '\0'; ++i) {
        if (sender_sanitized[i] == '.') {
            sender_sanitized[i] = '_';
        }
    }

    static unsigned int s_seq = 1;
    char token[64];
    snprintf(token, sizeof(token), "snipclip_%u_%u", (unsigned int)getpid(), s_seq++);

    char expected_path[256];
    snprintf(
        expected_path,
        sizeof(expected_path),
        "/org/freedesktop/portal/desktop/request/%s/%s",
        sender_sanitized,
        token
    );

    PortalWaitState state;
    memset(&state, 0, sizeof(state));
    state.loop = g_main_loop_new(NULL, FALSE);
    state.response_code = 2;

    guint sub_id = g_dbus_connection_signal_subscribe(
        bus,
        "org.freedesktop.portal.Desktop",
        "org.freedesktop.portal.Request",
        "Response",
        expected_path,
        NULL,
        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
        on_portal_response_signal,
        &state,
        NULL
    );

    GVariantBuilder opts;
    g_variant_builder_init(&opts, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&opts, "{sv}", "handle_token", g_variant_new_string(token));
    g_variant_builder_add(&opts, "{sv}", "interactive", g_variant_new_boolean(interactive ? TRUE : FALSE));
    g_variant_builder_add(&opts, "{sv}", "modal", g_variant_new_boolean(interactive ? TRUE : FALSE));

    GVariant *reply = g_dbus_connection_call_sync(
        bus,
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Screenshot",
        "Screenshot",
        g_variant_new("(sa{sv})", "", &opts),
        G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_NONE,
        5000,
        NULL,
        &err
    );

    if (!reply) {
        if (err) g_error_free(err);
        g_dbus_connection_signal_unsubscribe(bus, sub_id);
        g_main_loop_unref(state.loop);
        g_object_unref(bus);
        return NULL;
    }

    const gchar *returned_path = NULL;
    g_variant_get(reply, "(&o)", &returned_path);
    guint alt_sub_id = 0;
    if (returned_path && strcmp(returned_path, expected_path) != 0) {
        alt_sub_id = g_dbus_connection_signal_subscribe(
            bus,
            "org.freedesktop.portal.Desktop",
            "org.freedesktop.portal.Request",
            "Response",
            returned_path,
            NULL,
            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
            on_portal_response_signal,
            &state,
            NULL
        );
    }
    g_variant_unref(reply);

    if (!state.completed) {
        guint timer_id = g_timeout_add(interactive ? 30000 : 8000, on_portal_timeout, &state);
        g_main_loop_run(state.loop);
        g_source_remove(timer_id);
    }

    g_dbus_connection_signal_unsubscribe(bus, sub_id);
    if (alt_sub_id != 0) {
        g_dbus_connection_signal_unsubscribe(bus, alt_sub_id);
    }
    g_main_loop_unref(state.loop);
    g_object_unref(bus);

    GdkPixbuf *pixbuf = NULL;
    if (state.response_code == 0 && state.uri != NULL) {
        gchar *local_path = g_filename_from_uri(state.uri, NULL, NULL);
        if (local_path != NULL) {
            pixbuf = gdk_pixbuf_new_from_file(local_path, NULL);
            g_unlink(local_path);
            g_free(local_path);
        }
    }
    g_free(state.uri);
    return pixbuf;
}

static GdkPixbuf *capture_via_helper_argv(gchar **argv, const char *target_file) {
    gint exit_status = -1;
    GError *err = NULL;
    gboolean spawned = g_spawn_sync(
        NULL,
        argv,
        NULL,
        G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        &exit_status,
        &err
    );
    if (err) g_error_free(err);
    if (!spawned || exit_status != 0) {
        return NULL;
    }
    return gdk_pixbuf_new_from_file(target_file, NULL);
}

static GdkPixbuf *capture_with_cli_tool(ScWaylandCaptureStep step) {
    const char *bin = NULL;
    switch (step) {
        case SC_WL_CAPTURE_GRIM: bin = "grim"; break;
        case SC_WL_CAPTURE_SPECTACLE: bin = "spectacle"; break;
        case SC_WL_CAPTURE_GNOME_SCREENSHOT: bin = "gnome-screenshot"; break;
        default: return NULL;
    }
    if (!sc_has_binary_in_path(bin, NULL)) return NULL;

    gchar *tmp_path = NULL;
    GError *err = NULL;
    gint fd = g_file_open_tmp("snipclip_XXXXXX.png", &tmp_path, &err);
    if (fd < 0 || !tmp_path) {
        if (err) g_error_free(err);
        g_free(tmp_path);
        return NULL;
    }
    close(fd);

    GdkPixbuf *pix = NULL;
    if (step == SC_WL_CAPTURE_GRIM) {
        gchar *argv[] = { "grim", tmp_path, NULL };
        pix = capture_via_helper_argv(argv, tmp_path);
    } else if (step == SC_WL_CAPTURE_SPECTACLE) {
        gchar *argv[] = { "spectacle", "-f", "-b", "-n", "-o", tmp_path, NULL };
        pix = capture_via_helper_argv(argv, tmp_path);
    } else {
        gchar *argv[] = { "gnome-screenshot", "-f", tmp_path, NULL };
        pix = capture_via_helper_argv(argv, tmp_path);
    }

    g_unlink(tmp_path);
    g_free(tmp_path);
    return pix;
}

static GdkPixbuf *capture_gdk_root_window(int *out_origin_x, int *out_origin_y) {
    if (out_origin_x) *out_origin_x = 0;
    if (out_origin_y) *out_origin_y = 0;

    GdkWindow *root = gdk_get_default_root_window();
    if (!root) return NULL;

    int root_w = gdk_window_get_width(root);
    int root_h = gdk_window_get_height(root);

    /* Union of every connected monitor so multi-head desktops are captured in full */
    int ux1 = 0, uy1 = 0, ux2 = 0, uy2 = 0;
    bool have_union = false;
    GdkDisplay *disp = gdk_window_get_display(root);
    int n_mon = disp ? gdk_display_get_n_monitors(disp) : 0;
    for (int i = 0; i < n_mon; ++i) {
        GdkMonitor *mon = gdk_display_get_monitor(disp, i);
        if (!mon) continue;
        GdkRectangle g;
        gdk_monitor_get_geometry(mon, &g);
        if (g.width <= 0 || g.height <= 0) continue;
        if (!have_union) {
            ux1 = g.x;
            uy1 = g.y;
            ux2 = g.x + g.width;
            uy2 = g.y + g.height;
            have_union = true;
        } else {
            if (g.x < ux1) ux1 = g.x;
            if (g.y < uy1) uy1 = g.y;
            if (g.x + g.width > ux2) ux2 = g.x + g.width;
            if (g.y + g.height > uy2) uy2 = g.y + g.height;
        }
    }

    int x = 0, y = 0, w = root_w, h = root_h;
    if (have_union) {
        x = ux1;
        y = uy1;
        w = ux2 - ux1;
        h = uy2 - uy1;
        /* Never request pixels outside the root window (X11 root always starts at 0,0) */
        if (root_w > 0 && root_h > 0) {
            if (x < 0) { w += x; x = 0; }
            if (y < 0) { h += y; y = 0; }
            if (x + w > root_w) w = root_w - x;
            if (y + h > root_h) h = root_h - y;
            /* Prefer the full root extent when it fully covers the monitor union (X11 screen) */
            if (x == 0 && y == 0 && w <= root_w && h <= root_h) {
                w = root_w;
                h = root_h;
            }
        }
    }
    if (w <= 0 || h <= 0) return NULL;

    GdkPixbuf *pix = gdk_pixbuf_get_from_window(root, x, y, w, h);
    if (pix) {
        if (out_origin_x) *out_origin_x = x;
        if (out_origin_y) *out_origin_y = y;
    }
    return pix;
}

GdkPixbuf *sc_capture_wayland_window_interactive(void) {
    /* Interactive XDG portal picker only: NEVER falls back to a silent full-desktop capture */
    return capture_via_xdg_portal_gdbus(true);
}

GdkPixbuf *sc_capture_desktop_pixbuf(
    const ScDisplaySession *session,
    bool interactive_window,
    int *out_origin_x,
    int *out_origin_y
) {
    if (out_origin_x) *out_origin_x = 0;
    if (out_origin_y) *out_origin_y = 0;

    if (session && session->protocol == SC_PROTO_WAYLAND) {
        if (interactive_window) {
            GdkPixbuf *win_pix = capture_via_xdg_portal_gdbus(true);
            if (win_pix) return win_pix;
        }

        /* Execute the exact ordered plan reported as capture_driver telemetry */
        ScWaylandCaptureStep plan[SC_WL_CAPTURE_MAX_STEPS];
        size_t n = sc_wayland_capture_plan(
            session->desktop_environment, sc_has_binary_in_path, NULL, plan, SC_WL_CAPTURE_MAX_STEPS
        );
        for (size_t i = 0; i < n; ++i) {
            GdkPixbuf *pix = NULL;
            switch (plan[i]) {
                case SC_WL_CAPTURE_PORTAL:
                    pix = capture_via_xdg_portal_gdbus(false);
                    break;
                case SC_WL_CAPTURE_GRIM:
                case SC_WL_CAPTURE_SPECTACLE:
                case SC_WL_CAPTURE_GNOME_SCREENSHOT:
                    pix = capture_with_cli_tool(plan[i]);
                    break;
                case SC_WL_CAPTURE_GDK_ROOT:
                default:
                    pix = capture_gdk_root_window(out_origin_x, out_origin_y);
                    break;
            }
            if (pix) return pix;
        }
        return NULL;
    }

    return capture_gdk_root_window(out_origin_x, out_origin_y);
}

static void fetch_x11_window_title(Display *dpy, Window w, Atom net_wm_name, Atom utf8_str, char *out, size_t out_sz) {
    out[0] = '\0';
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long nitems = 0, bytes_after = 0;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(
            dpy,
            w,
            net_wm_name,
            0,
            128,
            False,
            utf8_str,
            &actual_type,
            &actual_format,
            &nitems,
            &bytes_after,
            &prop
        ) == Success && prop != NULL) {
        snprintf(out, out_sz, "%s", (const char *)prop);
        XFree(prop);
        if (out[0] != '\0') return;
    }

    char *wm_name = NULL;
    if (XFetchName(dpy, w, &wm_name) > 0 && wm_name != NULL) {
        snprintf(out, out_sz, "%s", wm_name);
        XFree(wm_name);
    }
    if (out[0] == '\0') {
        snprintf(out, out_sz, "Window 0x%lx", w);
    }
}

size_t sc_enumerate_x11_windows(ScWindowInfo *out_windows, size_t max_windows) {
    if (!out_windows || max_windows == 0) return 0;

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) return 0;

    int (*prev_handler)(Display *, XErrorEvent *) = XSetErrorHandler(ignore_x11_errors);
    Window root = DefaultRootWindow(dpy);

    Atom list_atom = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
    Atom fallback_atom = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
    Atom utf8_str = XInternAtom(dpy, "UTF8_STRING", False);

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long nitems = 0, bytes_after = 0;
    unsigned char *data = NULL;

    if (XGetWindowProperty(
            dpy,
            root,
            list_atom,
            0,
            1024,
            False,
            XA_WINDOW,
            &actual_type,
            &actual_format,
            &nitems,
            &bytes_after,
            &data
        ) != Success || !data || nitems == 0) {
        if (data) {
            XFree(data);
            data = NULL;
        }
        XGetWindowProperty(
            dpy,
            root,
            fallback_atom,
            0,
            1024,
            False,
            XA_WINDOW,
            &actual_type,
            &actual_format,
            &nitems,
            &bytes_after,
            &data
        );
    }

    size_t count = 0;
    if (data && actual_format == 32 && nitems > 0) {
        Window *wins = (Window *)data;
        /* Iterate in reverse stacking order so topmost window is matched first */
        for (long i = (long)nitems - 1; i >= 0 && count < max_windows; --i) {
            Window w = wins[i];
            XWindowAttributes attr;
            memset(&attr, 0, sizeof(attr));
            if (XGetWindowAttributes(dpy, w, &attr) == 0) continue;
            if (attr.map_state != IsViewable) continue;
            if (attr.width < 24 || attr.height < 24) continue;

            int abs_x = 0, abs_y = 0;
            Window child_dummy = None;
            XTranslateCoordinates(dpy, w, root, 0, 0, &abs_x, &abs_y, &child_dummy);

            out_windows[count].window_id = (unsigned long)w;
            out_windows[count].bounds.x = abs_x;
            out_windows[count].bounds.y = abs_y;
            out_windows[count].bounds.width = attr.width;
            out_windows[count].bounds.height = attr.height;
            fetch_x11_window_title(
                dpy,
                w,
                net_wm_name,
                utf8_str,
                out_windows[count].title,
                sizeof(out_windows[count].title)
            );
            count++;
        }
    }

    if (data) XFree(data);
    XSetErrorHandler(prev_handler);
    XCloseDisplay(dpy);
    return count;
}

static bool pipe_png_to_daemon(gchar **argv, const gchar *png_buf, gsize png_len) {
    gint stdin_fd = -1;
    GError *err = NULL;
    gboolean ok = g_spawn_async_with_pipes(
        NULL,
        argv,
        NULL,
        G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
        NULL,
        NULL,
        NULL,
        &stdin_fd,
        NULL,
        NULL,
        &err
    );
    if (!ok || stdin_fd < 0) {
        if (err) g_error_free(err);
        return false;
    }

    /* A clipboard helper that exits early must not kill SnipClip with SIGPIPE */
    signal(SIGPIPE, SIG_IGN);

    gsize total = 0;
    while (total < png_len) {
        ssize_t n = write(stdin_fd, png_buf + total, png_len - total);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        total += (gsize)n;
    }
    close(stdin_fd);
    return total == png_len;
}

bool sc_copy_pixbuf_to_clipboard(GdkPixbuf *pixbuf, const ScDisplaySession *session) {
    if (!pixbuf) return false;

    GtkClipboard *clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    if (clip) {
        gtk_clipboard_set_image(clip, pixbuf);
        gtk_clipboard_store(clip);
    }

    gchar *png_buf = NULL;
    gsize png_len = 0;
    if (!gdk_pixbuf_save_to_buffer(pixbuf, &png_buf, &png_len, "png", NULL, NULL)) {
        return clip != NULL;
    }

    if (session && session->protocol == SC_PROTO_WAYLAND) {
        gchar *wl = g_find_program_in_path("wl-copy");
        if (wl) {
            g_free(wl);
            gchar *argv[] = { "wl-copy", "--type", "image/png", NULL };
            pipe_png_to_daemon(argv, png_buf, png_len);
        }
    } else if (session && session->protocol == SC_PROTO_X11) {
        gchar *xc = g_find_program_in_path("xclip");
        if (xc) {
            g_free(xc);
            gchar *argv[] = { "xclip", "-selection", "clipboard", "-t", "image/png", "-i", NULL };
            pipe_png_to_daemon(argv, png_buf, png_len);
        }
    }

    g_free(png_buf);
    return true;
}
