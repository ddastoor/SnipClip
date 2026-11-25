#define _POSIX_C_SOURCE 200809L

#include "global_hotkey.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SC_HK_MAX_GRABS 2
#define SC_HK_LOCK_VARIANTS 4

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_GS_IFACE "org.freedesktop.portal.GlobalShortcuts"
#define PORTAL_REQUEST_IFACE "org.freedesktop.portal.Request"
#define PORTAL_SESSION_IFACE "org.freedesktop.portal.Session"

/* Weak back-reference shared with async GDBus callbacks so a callback that fires after
 * sc_global_hotkey_free() never touches freed memory. */
typedef struct {
    ScGlobalHotkey *hk;
    gint refs;
} HkBox;

typedef struct {
    KeyCode keycode;
    unsigned int mods;
    bool grabbed[SC_HK_LOCK_VARIANTS];
} ScX11Grab;

struct ScGlobalHotkey {
    ScDisplayProtocol protocol;
    ScHotkeyActivatedFn on_activated;
    void *user_data;
    HkBox *box;
    guint dispatch_idle_id;

    /* X11 */
    GdkDisplay *gdk_display;
    Display *xdpy;
    Window xroot;
    GdkWindow *gdk_root;
    bool filter_installed;
    ScX11Grab grabs[SC_HK_MAX_GRABS];
    int grab_count;
    int conflicts;

    /* Wayland org.freedesktop.portal.GlobalShortcuts */
    GCancellable *cancellable;
    GDBusConnection *bus;
    bool bus_pending;
    bool bind_requested;
    char *session_handle;
    guint request_sub_id;
    guint activated_sub_id;
    guint token_seq;
    char custom_trigger[96];
};

static const unsigned int k_lock_variants[SC_HK_LOCK_VARIANTS] = {
    0,
    Mod2Mask,
    LockMask,
    Mod2Mask | LockMask
};

static HkBox *box_ref(HkBox *b) {
    g_atomic_int_inc(&b->refs);
    return b;
}

static void box_unref(gpointer p) {
    HkBox *b = (HkBox *)p;
    if (b && g_atomic_int_dec_and_test(&b->refs)) {
        g_free(b);
    }
}

static gboolean dispatch_activation_idle(gpointer user_data) {
    ScGlobalHotkey *hk = (ScGlobalHotkey *)user_data;
    hk->dispatch_idle_id = 0;
    if (hk->on_activated) {
        hk->on_activated(hk->user_data);
    }
    return G_SOURCE_REMOVE;
}

static void schedule_activation(ScGlobalHotkey *hk) {
    if (hk->dispatch_idle_id == 0) {
        hk->dispatch_idle_id = g_idle_add(dispatch_activation_idle, hk);
    }
}

/* ------------------------------------------------------------------------------------------ */
/* X11: XGrabKey + root window event filter                                                   */
/* ------------------------------------------------------------------------------------------ */

static unsigned int gdk_mods_to_x11(GdkModifierType m) {
    unsigned int x = 0;
    if (m & GDK_SHIFT_MASK) x |= ShiftMask;
    if (m & GDK_CONTROL_MASK) x |= ControlMask;
    if (m & (GDK_MOD1_MASK | GDK_META_MASK)) x |= Mod1Mask;
    if (m & (GDK_SUPER_MASK | GDK_HYPER_MASK | GDK_MOD4_MASK)) x |= Mod4Mask;
    return x;
}

static GdkFilterReturn x11_root_key_filter(GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data) {
    (void)event;
    ScGlobalHotkey *hk = (ScGlobalHotkey *)data;
    XEvent *xev = (XEvent *)gdk_xevent;
    if (!hk || !xev || (xev->type != KeyPress && xev->type != KeyRelease)) {
        return GDK_FILTER_CONTINUE;
    }

    const unsigned int relevant = ShiftMask | ControlMask | Mod1Mask | Mod3Mask | Mod4Mask | Mod5Mask;
    unsigned int state = xev->xkey.state & relevant;
    for (int i = 0; i < hk->grab_count; ++i) {
        const ScX11Grab *g = &hk->grabs[i];
        if (g->keycode == xev->xkey.keycode && g->mods == state) {
            if (xev->type == KeyPress) {
                schedule_activation(hk);
            }
            return GDK_FILTER_REMOVE;
        }
    }
    return GDK_FILTER_CONTINUE;
}

static void x11_ungrab_all(ScGlobalHotkey *hk) {
    if (!hk->xdpy || !hk->gdk_display) return;
    for (int i = 0; i < hk->grab_count; ++i) {
        ScX11Grab *g = &hk->grabs[i];
        for (int v = 0; v < SC_HK_LOCK_VARIANTS; ++v) {
            if (!g->grabbed[v]) continue;
            gdk_x11_display_error_trap_push(hk->gdk_display);
            XUngrabKey(hk->xdpy, g->keycode, g->mods | k_lock_variants[v], hk->xroot);
            gdk_x11_display_error_trap_pop_ignored(hk->gdk_display);
            g->grabbed[v] = false;
        }
    }
    hk->grab_count = 0;
    XFlush(hk->xdpy);
}

static bool x11_grab_key(ScGlobalHotkey *hk, KeySym sym, unsigned int mods) {
    if (hk->grab_count >= SC_HK_MAX_GRABS) return false;
    KeyCode kc = XKeysymToKeycode(hk->xdpy, sym);
    if (kc == 0) return false;
    for (int i = 0; i < hk->grab_count; ++i) {
        if (hk->grabs[i].keycode == kc && hk->grabs[i].mods == mods) return true;
    }

    ScX11Grab *g = &hk->grabs[hk->grab_count];
    memset(g, 0, sizeof(*g));
    g->keycode = kc;
    g->mods = mods;

    bool any = false;
    for (int v = 0; v < SC_HK_LOCK_VARIANTS; ++v) {
        /* Scoped error trap: BadAccess (key held by another client) is recorded, never fatal */
        gdk_x11_display_error_trap_push(hk->gdk_display);
        XGrabKey(hk->xdpy, kc, mods | k_lock_variants[v], hk->xroot, False, GrabModeAsync, GrabModeAsync);
        gint err = gdk_x11_display_error_trap_pop(hk->gdk_display);
        if (err == 0) {
            g->grabbed[v] = true;
            any = true;
        } else {
            g->grabbed[v] = false;
            if (err == BadAccess) hk->conflicts++;
        }
    }
    hk->grab_count++;
    return any;
}

static bool x11_bind(ScGlobalHotkey *hk, guint custom_keyval, GdkModifierType custom_mods) {
    if (!hk->xdpy) return false;
    x11_ungrab_all(hk);
    hk->conflicts = 0;

    if (!hk->filter_installed && hk->gdk_root) {
        gdk_window_add_filter(hk->gdk_root, x11_root_key_filter, hk);
        hk->filter_installed = true;
    }

    bool ok = x11_grab_key(hk, XK_Print, 0);
    if (custom_keyval != 0) {
        unsigned int xmods = gdk_mods_to_x11(custom_mods);
        if (!(custom_keyval == GDK_KEY_Print && xmods == 0)) {
            ok = x11_grab_key(hk, (KeySym)custom_keyval, xmods) || ok;
        }
    }
    XFlush(hk->xdpy);
    return ok;
}

/* ------------------------------------------------------------------------------------------ */
/* Wayland: asynchronous org.freedesktop.portal.GlobalShortcuts                               */
/* ------------------------------------------------------------------------------------------ */

static void portal_build_trigger(guint keyval, GdkModifierType mods, char *out, size_t out_sz) {
    out[0] = '\0';
    if (keyval == 0) return;
    const char *name = gdk_keyval_name(gdk_keyval_to_lower(keyval));
    if (!name) return;
    snprintf(
        out,
        out_sz,
        "%s%s%s%s%s",
        (mods & GDK_CONTROL_MASK) ? "CTRL+" : "",
        (mods & (GDK_MOD1_MASK | GDK_META_MASK)) ? "ALT+" : "",
        (mods & GDK_SHIFT_MASK) ? "SHIFT+" : "",
        (mods & (GDK_SUPER_MASK | GDK_MOD4_MASK)) ? "LOGO+" : "",
        name
    );
}

static bool portal_request_path(ScGlobalHotkey *hk, const char *token, char *out, size_t out_sz) {
    const gchar *unique = hk->bus ? g_dbus_connection_get_unique_name(hk->bus) : NULL;
    if (!unique || unique[0] != ':') return false;
    char sender[128];
    snprintf(sender, sizeof(sender), "%s", unique + 1);
    for (size_t i = 0; sender[i] != '\0'; ++i) {
        if (sender[i] == '.') sender[i] = '_';
    }
    int n = snprintf(out, out_sz, "/org/freedesktop/portal/desktop/request/%s/%s", sender, token);
    return n > 0 && (size_t)n < out_sz;
}

static void portal_clear_request_sub(ScGlobalHotkey *hk) {
    if (hk->request_sub_id != 0 && hk->bus) {
        g_dbus_connection_signal_unsubscribe(hk->bus, hk->request_sub_id);
    }
    hk->request_sub_id = 0;
}

static void portal_close_session(ScGlobalHotkey *hk) {
    if (hk->activated_sub_id != 0 && hk->bus) {
        g_dbus_connection_signal_unsubscribe(hk->bus, hk->activated_sub_id);
    }
    hk->activated_sub_id = 0;
    if (hk->session_handle && hk->bus) {
        g_dbus_connection_call(
            hk->bus,
            PORTAL_BUS_NAME,
            hk->session_handle,
            PORTAL_SESSION_IFACE,
            "Close",
            NULL,
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            NULL,
            NULL
        );
    }
    g_free(hk->session_handle);
    hk->session_handle = NULL;
}

static void portal_call_done(GObject *source, GAsyncResult *res, gpointer user_data) {
    HkBox *box = (HkBox *)user_data;
    GError *err = NULL;
    GVariant *ret = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), res, &err);
    if (ret) {
        g_variant_unref(ret);
    }
    if (err) {
        /* Portal / interface not implemented (or cancelled): degrade silently */
        if (box->hk && !g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            portal_clear_request_sub(box->hk);
        }
        g_error_free(err);
    }
    box_unref(box);
}

static void on_portal_activated(
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
    HkBox *box = (HkBox *)user_data;
    ScGlobalHotkey *hk = box ? box->hk : NULL;
    if (!hk || !parameters || !g_variant_is_of_type(parameters, G_VARIANT_TYPE("(osta{sv})"))) return;

    const gchar *session = NULL;
    const gchar *shortcut_id = NULL;
    guint64 timestamp = 0;
    GVariant *opts = NULL;
    g_variant_get(parameters, "(&o&st@a{sv})", &session, &shortcut_id, &timestamp, &opts);
    if (opts) g_variant_unref(opts);

    if (hk->session_handle && session && strcmp(session, hk->session_handle) == 0 &&
        shortcut_id && strncmp(shortcut_id, "snipclip-", 9) == 0) {
        schedule_activation(hk);
    }
}

static void portal_add_shortcut(GVariantBuilder *b, const char *id, const char *desc, const char *trigger) {
    g_variant_builder_open(b, G_VARIANT_TYPE("(sa{sv})"));
    g_variant_builder_add(b, "s", id);
    g_variant_builder_open(b, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(b, "{sv}", "description", g_variant_new_string(desc));
    if (trigger && *trigger) {
        g_variant_builder_add(b, "{sv}", "preferred_trigger", g_variant_new_string(trigger));
    }
    g_variant_builder_close(b);
    g_variant_builder_close(b);
}

static void on_bind_response(
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
    (void)parameters;
    HkBox *box = (HkBox *)user_data;
    if (box && box->hk) {
        portal_clear_request_sub(box->hk);
    }
}

static void portal_bind_shortcuts(ScGlobalHotkey *hk) {
    if (!hk->bus || !hk->session_handle) return;

    char token[64];
    snprintf(token, sizeof(token), "snipclip_hkb_%ld_%u", (long)getpid(), ++hk->token_seq);
    char req_path[256];
    if (!portal_request_path(hk, token, req_path, sizeof(req_path))) return;

    portal_clear_request_sub(hk);
    hk->request_sub_id = g_dbus_connection_signal_subscribe(
        hk->bus,
        PORTAL_BUS_NAME,
        PORTAL_REQUEST_IFACE,
        "Response",
        req_path,
        NULL,
        G_DBUS_SIGNAL_FLAGS_NONE,
        on_bind_response,
        box_ref(hk->box),
        box_unref
    );

    GVariantBuilder shortcuts;
    g_variant_builder_init(&shortcuts, G_VARIANT_TYPE("a(sa{sv})"));
    portal_add_shortcut(&shortcuts, "snipclip-print", "Start a new SnipClip snip (PrintScreen)", "Print");
    if (hk->custom_trigger[0] != '\0' && strcmp(hk->custom_trigger, "Print") != 0) {
        portal_add_shortcut(&shortcuts, "snipclip-custom", "Start a new SnipClip snip (custom shortcut)", hk->custom_trigger);
    }

    GVariantBuilder opts;
    g_variant_builder_init(&opts, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&opts, "{sv}", "handle_token", g_variant_new_string(token));

    g_dbus_connection_call(
        hk->bus,
        PORTAL_BUS_NAME,
        PORTAL_OBJECT_PATH,
        PORTAL_GS_IFACE,
        "BindShortcuts",
        g_variant_new("(oa(sa{sv})sa{sv})", hk->session_handle, &shortcuts, "", &opts),
        G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1,
        hk->cancellable,
        portal_call_done,
        box_ref(hk->box)
    );
}

static void on_create_session_response(
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
    HkBox *box = (HkBox *)user_data;
    ScGlobalHotkey *hk = box ? box->hk : NULL;
    if (!hk) return;
    portal_clear_request_sub(hk);
    if (!parameters || !g_variant_is_of_type(parameters, G_VARIANT_TYPE("(ua{sv})"))) return;

    guint32 response = 2;
    GVariant *results = NULL;
    g_variant_get(parameters, "(u@a{sv})", &response, &results);
    if (response == 0 && results) {
        GVariant *handle = g_variant_lookup_value(results, "session_handle", NULL);
        if (handle) {
            if (g_variant_is_of_type(handle, G_VARIANT_TYPE_STRING) ||
                g_variant_is_of_type(handle, G_VARIANT_TYPE_OBJECT_PATH)) {
                g_free(hk->session_handle);
                hk->session_handle = g_variant_dup_string(handle, NULL);
            }
            g_variant_unref(handle);
        }
    }
    if (results) g_variant_unref(results);

    if (hk->session_handle && g_variant_is_object_path(hk->session_handle)) {
        hk->activated_sub_id = g_dbus_connection_signal_subscribe(
            hk->bus,
            PORTAL_BUS_NAME,
            PORTAL_GS_IFACE,
            "Activated",
            PORTAL_OBJECT_PATH,
            NULL,
            G_DBUS_SIGNAL_FLAGS_NONE,
            on_portal_activated,
            box_ref(hk->box),
            box_unref
        );
        portal_bind_shortcuts(hk);
    } else {
        g_free(hk->session_handle);
        hk->session_handle = NULL;
    }
}

static void portal_start_session(ScGlobalHotkey *hk) {
    if (!hk->bus) return;
    hk->bind_requested = false;
    portal_close_session(hk);

    char token[64];
    char session_token[64];
    ++hk->token_seq;
    snprintf(token, sizeof(token), "snipclip_hkc_%ld_%u", (long)getpid(), hk->token_seq);
    snprintf(session_token, sizeof(session_token), "snipclip_hks_%ld_%u", (long)getpid(), hk->token_seq);
    char req_path[256];
    if (!portal_request_path(hk, token, req_path, sizeof(req_path))) return;

    portal_clear_request_sub(hk);
    hk->request_sub_id = g_dbus_connection_signal_subscribe(
        hk->bus,
        PORTAL_BUS_NAME,
        PORTAL_REQUEST_IFACE,
        "Response",
        req_path,
        NULL,
        G_DBUS_SIGNAL_FLAGS_NONE,
        on_create_session_response,
        box_ref(hk->box),
        box_unref
    );

    GVariantBuilder opts;
    g_variant_builder_init(&opts, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&opts, "{sv}", "handle_token", g_variant_new_string(token));
    g_variant_builder_add(&opts, "{sv}", "session_handle_token", g_variant_new_string(session_token));

    g_dbus_connection_call(
        hk->bus,
        PORTAL_BUS_NAME,
        PORTAL_OBJECT_PATH,
        PORTAL_GS_IFACE,
        "CreateSession",
        g_variant_new("(a{sv})", &opts),
        G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_NO_AUTO_START,
        -1,
        hk->cancellable,
        portal_call_done,
        box_ref(hk->box)
    );
}

static void on_session_bus_ready(GObject *source, GAsyncResult *res, gpointer user_data) {
    (void)source;
    HkBox *box = (HkBox *)user_data;
    GError *err = NULL;
    GDBusConnection *bus = g_bus_get_finish(res, &err);
    ScGlobalHotkey *hk = box->hk;
    if (!hk) {
        if (bus) g_object_unref(bus);
        if (err) g_error_free(err);
        box_unref(box);
        return;
    }
    hk->bus_pending = false;
    if (!bus) {
        if (err) g_error_free(err);
        box_unref(box);
        return;
    }
    hk->bus = bus;
    if (hk->bind_requested) {
        portal_start_session(hk);
    }
    box_unref(box);
}

static bool wayland_bind(ScGlobalHotkey *hk, guint custom_keyval, GdkModifierType custom_mods) {
    portal_build_trigger(custom_keyval, custom_mods, hk->custom_trigger, sizeof(hk->custom_trigger));
    hk->bind_requested = true;
    if (hk->bus) {
        portal_start_session(hk);
        return true;
    }
    if (!hk->bus_pending) {
        hk->bus_pending = true;
        g_bus_get(G_BUS_TYPE_SESSION, hk->cancellable, on_session_bus_ready, box_ref(hk->box));
    }
    return true;
}

/* ------------------------------------------------------------------------------------------ */
/* Public API                                                                                 */
/* ------------------------------------------------------------------------------------------ */

ScGlobalHotkey *sc_global_hotkey_new(const ScDisplaySession *session, ScHotkeyActivatedFn on_activated, void *user_data) {
    ScGlobalHotkey *hk = (ScGlobalHotkey *)calloc(1, sizeof(ScGlobalHotkey));
    if (!hk) return NULL;
    hk->protocol = session ? session->protocol : SC_PROTO_HEADLESS;
    hk->on_activated = on_activated;
    hk->user_data = user_data;
    hk->box = g_new0(HkBox, 1);
    hk->box->hk = hk;
    hk->box->refs = 1;
    hk->cancellable = g_cancellable_new();

    GdkDisplay *disp = gdk_display_get_default();
    if (hk->protocol == SC_PROTO_X11 && disp && GDK_IS_X11_DISPLAY(disp)) {
        hk->gdk_display = disp;
        hk->xdpy = gdk_x11_display_get_xdisplay(disp);
        hk->xroot = DefaultRootWindow(hk->xdpy);
        hk->gdk_root = gdk_get_default_root_window();
    }
    return hk;
}

bool sc_global_hotkey_bind(ScGlobalHotkey *hk, guint custom_keyval, GdkModifierType custom_mods) {
    if (!hk) return false;
    if (hk->protocol == SC_PROTO_X11) {
        return x11_bind(hk, custom_keyval, custom_mods);
    }
    if (hk->protocol == SC_PROTO_WAYLAND) {
        return wayland_bind(hk, custom_keyval, custom_mods);
    }
    return false;
}

int sc_global_hotkey_x11_conflicts(const ScGlobalHotkey *hk) {
    return hk ? hk->conflicts : 0;
}

void sc_global_hotkey_free(ScGlobalHotkey *hk) {
    if (!hk) return;

    if (hk->dispatch_idle_id != 0) {
        g_source_remove(hk->dispatch_idle_id);
        hk->dispatch_idle_id = 0;
    }

    x11_ungrab_all(hk);
    if (hk->filter_installed && hk->gdk_root) {
        gdk_window_remove_filter(hk->gdk_root, x11_root_key_filter, hk);
        hk->filter_installed = false;
    }

    g_cancellable_cancel(hk->cancellable);
    portal_clear_request_sub(hk);
    portal_close_session(hk);
    if (hk->bus) {
        g_dbus_connection_flush(hk->bus, NULL, NULL, NULL);
        g_object_unref(hk->bus);
        hk->bus = NULL;
    }
    g_object_unref(hk->cancellable);

    hk->box->hk = NULL;
    box_unref(hk->box);
    free(hk);
}
