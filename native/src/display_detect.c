#define _POSIX_C_SOURCE 200809L

#include "display_detect.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static void copy_trimmed(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    dst[0] = '\0';
    if (!src) return;
    while (*src && isspace((unsigned char)*src)) {
        src++;
    }
    size_t len = strlen(src);
    while (len > 0 && isspace((unsigned char)src[len - 1])) {
        len--;
    }
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void lowercase_inplace(char *s) {
    for (; s && *s; ++s) {
        *s = (char)tolower((unsigned char)*s);
    }
}

static const char *os_getenv_cb(const char *name, void *user_data) {
    (void)user_data;
    return getenv(name);
}

static bool os_has_binary_cb(const char *bin_name, void *user_data) {
    (void)user_data;
    if (!bin_name || !*bin_name) return false;
    const char *path_env = getenv("PATH");
    if (!path_env || !*path_env) {
        path_env = "/usr/local/bin:/usr/bin:/bin";
    }

    char path_copy[4096];
    snprintf(path_copy, sizeof(path_copy), "%s", path_env);

    char *saveptr = NULL;
    for (char *dir = strtok_r(path_copy, ":", &saveptr); dir != NULL; dir = strtok_r(NULL, ":", &saveptr)) {
        char candidate[1024];
        snprintf(candidate, sizeof(candidate), "%s/%s", dir, bin_name);
        if (access(candidate, X_OK) == 0) {
            struct stat st;
            if (stat(candidate, &st) == 0 && S_ISREG(st.st_mode)) {
                return true;
            }
        }
    }
    return false;
}

static bool discover_wayland_socket_in_dir(const char *dir_path, char *out_name, size_t out_size) {
    if (!dir_path || !*dir_path) return false;
    DIR *d = opendir(dir_path);
    if (!d) return false;

    char best[64] = "";
    struct dirent *ent = NULL;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, "wayland-", 8) != 0) continue;
        if (!isdigit((unsigned char)ent->d_name[8])) continue;
        size_t nlen = strlen(ent->d_name);
        if (nlen > 5 && strcmp(ent->d_name + nlen - 5, ".lock") == 0) continue;

        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", dir_path, ent->d_name);
        struct stat st;
        if (stat(full, &st) == 0 && S_ISSOCK(st.st_mode)) {
            if (best[0] == '\0' || strcmp(ent->d_name, best) < 0) {
                snprintf(best, sizeof(best), "%s", ent->d_name);
            }
        }
    }
    closedir(d);

    if (best[0] != '\0') {
        snprintf(out_name, out_size, "%s", best);
        return true;
    }
    return false;
}

static bool discover_x11_socket(char *out_display, size_t out_size) {
    DIR *d = opendir("/tmp/.X11-unix");
    if (!d) return false;

    int best_num = -1;
    struct dirent *ent = NULL;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] != 'X' || !isdigit((unsigned char)ent->d_name[1])) continue;
        bool all_digits = true;
        for (size_t i = 1; ent->d_name[i] != '\0'; ++i) {
            if (!isdigit((unsigned char)ent->d_name[i])) {
                all_digits = false;
                break;
            }
        }
        if (!all_digits) continue;

        char full[512];
        snprintf(full, sizeof(full), "/tmp/.X11-unix/%s", ent->d_name);
        struct stat st;
        if (stat(full, &st) == 0 && S_ISSOCK(st.st_mode)) {
            int num = atoi(ent->d_name + 1);
            if (best_num < 0 || num < best_num) {
                best_num = num;
            }
        }
    }
    closedir(d);

    if (best_num >= 0) {
        snprintf(out_display, out_size, ":%d", best_num);
        return true;
    }
    return false;
}

static bool run_single_line_cmd(const char *cmd, char *out, size_t out_size) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return false;
    char raw[128] = "";
    bool ok = fgets(raw, sizeof(raw), fp) != NULL;
    int status = pclose(fp);
    if (!ok || status != 0) return false;
    copy_trimmed(out, out_size, raw);
    return out[0] != '\0';
}

static void probe_loginctl_type(char *out_type, size_t out_size) {
    out_type[0] = '\0';
    if (!os_has_binary_cb("loginctl", NULL)) return;

    char session_id[64] = "";
    copy_trimmed(session_id, sizeof(session_id), getenv("XDG_SESSION_ID"));

    /* Safe validation so session_id only contains alphanumeric / dash / underscore characters */
    for (size_t i = 0; session_id[i] != '\0'; ++i) {
        char c = session_id[i];
        if (!isalnum((unsigned char)c) && c != '-' && c != '_') {
            session_id[0] = '\0';
            break;
        }
    }

    if (session_id[0] == '\0') {
        char user_cmd[128];
        snprintf(
            user_cmd,
            sizeof(user_cmd),
            "loginctl show-user %u -p Display --value 2>/dev/null",
            (unsigned int)getuid()
        );
        run_single_line_cmd(user_cmd, session_id, sizeof(session_id));
        for (size_t i = 0; session_id[i] != '\0'; ++i) {
            char c = session_id[i];
            if (!isalnum((unsigned char)c) && c != '-' && c != '_') {
                session_id[0] = '\0';
                break;
            }
        }
    }

    if (session_id[0] == '\0') return;

    char sess_cmd[192];
    snprintf(
        sess_cmd,
        sizeof(sess_cmd),
        "loginctl show-session %s -p Type --value 2>/dev/null",
        session_id
    );
    char detected[32] = "";
    if (run_single_line_cmd(sess_cmd, detected, sizeof(detected))) {
        lowercase_inplace(detected);
        if (strcmp(detected, "wayland") == 0 || strcmp(detected, "x11") == 0) {
            snprintf(out_type, out_size, "%s", detected);
        }
    }
}

static void populate_protocol_drivers(
    ScDisplaySession *s,
    ScHasBinaryFn has_bin_fn,
    void *user_data
) {
    if (s->protocol == SC_PROTO_WAYLAND) {
        snprintf(s->gdk_backend, sizeof(s->gdk_backend), "wayland,x11");
        if (has_bin_fn("dbus-daemon", user_data) || has_bin_fn("gdbus", user_data)) {
            snprintf(
                s->capture_driver,
                sizeof(s->capture_driver),
                "Native GDBus XDG Portal (org.freedesktop.portal.Screenshot)"
            );
        } else if (has_bin_fn("grim", user_data)) {
            snprintf(s->capture_driver, sizeof(s->capture_driver), "grim (wlroots Wayland)");
        } else if (has_bin_fn("spectacle", user_data)) {
            snprintf(s->capture_driver, sizeof(s->capture_driver), "spectacle (KDE Wayland)");
        } else if (has_bin_fn("gnome-screenshot", user_data)) {
            snprintf(s->capture_driver, sizeof(s->capture_driver), "gnome-screenshot");
        } else {
            snprintf(s->capture_driver, sizeof(s->capture_driver), "Native GDBus Portal / GDK Fallback");
        }

        if (has_bin_fn("wl-copy", user_data)) {
            snprintf(s->clipboard_driver, sizeof(s->clipboard_driver), "GtkClipboard + wl-copy");
        } else {
            snprintf(s->clipboard_driver, sizeof(s->clipboard_driver), "GtkClipboard (Wayland)");
        }
        s->supports_window_enumeration = false;
    } else if (s->protocol == SC_PROTO_X11) {
        snprintf(s->gdk_backend, sizeof(s->gdk_backend), "x11");
        snprintf(
            s->capture_driver,
            sizeof(s->capture_driver),
            "Native GDK/X11 Root Pixbuf + libX11 Window Tree"
        );
        if (has_bin_fn("xclip", user_data)) {
            snprintf(s->clipboard_driver, sizeof(s->clipboard_driver), "GtkClipboard + xclip");
        } else if (has_bin_fn("xsel", user_data)) {
            snprintf(s->clipboard_driver, sizeof(s->clipboard_driver), "GtkClipboard + xsel");
        } else {
            snprintf(s->clipboard_driver, sizeof(s->clipboard_driver), "GtkClipboard (X11)");
        }
        /* Native libX11 directly queries _NET_CLIENT_LIST_STACKING without needing external x11-utils */
        s->supports_window_enumeration = true;
    } else {
        snprintf(s->gdk_backend, sizeof(s->gdk_backend), "broadway");
        snprintf(s->capture_driver, sizeof(s->capture_driver), "offscreen-cairo-surface");
        snprintf(s->clipboard_driver, sizeof(s->clipboard_driver), "GtkClipboard");
        s->supports_window_enumeration = false;
    }
}

ScDisplaySession sc_detect_display_session_custom(
    ScGetenvFn getenv_fn,
    ScHasBinaryFn has_bin_fn,
    const char *discovered_wayland_sock,
    const char *discovered_x11_sock,
    const char *loginctl_type,
    void *user_data
) {
    ScDisplaySession s;
    memset(&s, 0, sizeof(s));

    char session_type[32] = "";
    copy_trimmed(session_type, sizeof(session_type), getenv_fn("XDG_SESSION_TYPE", user_data));
    lowercase_inplace(session_type);

    char lctl[32] = "";
    copy_trimmed(lctl, sizeof(lctl), loginctl_type);
    lowercase_inplace(lctl);

    copy_trimmed(s.wayland_display, sizeof(s.wayland_display), getenv_fn("WAYLAND_DISPLAY", user_data));
    copy_trimmed(s.x11_display, sizeof(s.x11_display), getenv_fn("DISPLAY", user_data));

    const char *xdg_desktop = getenv_fn("XDG_CURRENT_DESKTOP", user_data);
    const char *desk_sess = getenv_fn("DESKTOP_SESSION", user_data);
    if (xdg_desktop && *xdg_desktop) {
        copy_trimmed(s.desktop_environment, sizeof(s.desktop_environment), xdg_desktop);
    } else if (desk_sess && *desk_sess) {
        copy_trimmed(s.desktop_environment, sizeof(s.desktop_environment), desk_sess);
    }
    if (s.desktop_environment[0] == '\0') {
        snprintf(s.desktop_environment, sizeof(s.desktop_environment), "Generic Linux Desktop");
    }

    s.protocol = SC_PROTO_HEADLESS;
    snprintf(s.protocol_name, sizeof(s.protocol_name), "headless");
    snprintf(
        s.detection_source,
        sizeof(s.detection_source),
        "no display environment variables or sockets found"
    );

    if (strcmp(session_type, "wayland") == 0) {
        s.protocol = SC_PROTO_WAYLAND;
        snprintf(s.protocol_name, sizeof(s.protocol_name), "wayland");
        snprintf(s.detection_source, sizeof(s.detection_source), "XDG_SESSION_TYPE=wayland");
        if (s.wayland_display[0] == '\0' && discovered_wayland_sock && *discovered_wayland_sock) {
            snprintf(s.wayland_display, sizeof(s.wayland_display), "%s", discovered_wayland_sock);
        }
    } else if (strcmp(session_type, "x11") == 0) {
        s.protocol = SC_PROTO_X11;
        snprintf(s.protocol_name, sizeof(s.protocol_name), "x11");
        snprintf(s.detection_source, sizeof(s.detection_source), "XDG_SESSION_TYPE=x11");
        if (s.x11_display[0] == '\0' && discovered_x11_sock && *discovered_x11_sock) {
            snprintf(s.x11_display, sizeof(s.x11_display), "%s", discovered_x11_sock);
        }
    } else if (s.wayland_display[0] != '\0') {
        s.protocol = SC_PROTO_WAYLAND;
        snprintf(s.protocol_name, sizeof(s.protocol_name), "wayland");
        snprintf(s.detection_source, sizeof(s.detection_source), "WAYLAND_DISPLAY=%s", s.wayland_display);
    } else if (s.x11_display[0] != '\0' && strcmp(lctl, "wayland") != 0) {
        s.protocol = SC_PROTO_X11;
        snprintf(s.protocol_name, sizeof(s.protocol_name), "x11");
        snprintf(s.detection_source, sizeof(s.detection_source), "DISPLAY=%s", s.x11_display);
    } else if (strcmp(lctl, "wayland") == 0 || strcmp(lctl, "x11") == 0) {
        if (strcmp(lctl, "wayland") == 0) {
            s.protocol = SC_PROTO_WAYLAND;
            snprintf(s.protocol_name, sizeof(s.protocol_name), "wayland");
            if (s.wayland_display[0] == '\0' && discovered_wayland_sock && *discovered_wayland_sock) {
                snprintf(s.wayland_display, sizeof(s.wayland_display), "%s", discovered_wayland_sock);
            }
        } else {
            s.protocol = SC_PROTO_X11;
            snprintf(s.protocol_name, sizeof(s.protocol_name), "x11");
            if (s.x11_display[0] == '\0' && discovered_x11_sock && *discovered_x11_sock) {
                snprintf(s.x11_display, sizeof(s.x11_display), "%s", discovered_x11_sock);
            }
        }
        snprintf(s.detection_source, sizeof(s.detection_source), "loginctl session Type=%s", lctl);
    } else if (discovered_wayland_sock && *discovered_wayland_sock) {
        s.protocol = SC_PROTO_WAYLAND;
        snprintf(s.protocol_name, sizeof(s.protocol_name), "wayland");
        snprintf(s.wayland_display, sizeof(s.wayland_display), "%s", discovered_wayland_sock);
        snprintf(
            s.detection_source,
            sizeof(s.detection_source),
            "runtime socket %s",
            discovered_wayland_sock
        );
    } else if (discovered_x11_sock && *discovered_x11_sock) {
        s.protocol = SC_PROTO_X11;
        snprintf(s.protocol_name, sizeof(s.protocol_name), "x11");
        snprintf(s.x11_display, sizeof(s.x11_display), "%s", discovered_x11_sock);
        snprintf(s.detection_source, sizeof(s.detection_source), "/tmp/.X11-unix socket (%s)", discovered_x11_sock);
    }

    populate_protocol_drivers(&s, has_bin_fn, user_data);
    return s;
}

ScDisplaySession sc_detect_display_session(void) {
    char wl_sock[64] = "";
    const char *xdg_rt = getenv("XDG_RUNTIME_DIR");
    if (xdg_rt && *xdg_rt) {
        discover_wayland_socket_in_dir(xdg_rt, wl_sock, sizeof(wl_sock));
    } else {
        char uid_rt[128];
        snprintf(uid_rt, sizeof(uid_rt), "/run/user/%u", (unsigned int)getuid());
        discover_wayland_socket_in_dir(uid_rt, wl_sock, sizeof(wl_sock));
    }

    char x11_sock[64] = "";
    discover_x11_socket(x11_sock, sizeof(x11_sock));

    char sess_type[32] = "";
    copy_trimmed(sess_type, sizeof(sess_type), getenv("XDG_SESSION_TYPE"));
    lowercase_inplace(sess_type);

    char wl_env[64] = "";
    copy_trimmed(wl_env, sizeof(wl_env), getenv("WAYLAND_DISPLAY"));

    char loginctl_type[32] = "";
    if (strcmp(sess_type, "wayland") != 0 &&
        strcmp(sess_type, "x11") != 0 &&
        wl_env[0] == '\0') {
        probe_loginctl_type(loginctl_type, sizeof(loginctl_type));
    }

    return sc_detect_display_session_custom(
        os_getenv_cb,
        os_has_binary_cb,
        wl_sock,
        x11_sock,
        loginctl_type,
        NULL
    );
}

void sc_bootstrap_display_env(const ScDisplaySession *session) {
    if (!session) return;
    const char *gdk_env = getenv("GDK_BACKEND");
    if ((!gdk_env || !*gdk_env) && session->protocol != SC_PROTO_HEADLESS && session->gdk_backend[0] != '\0') {
        setenv("GDK_BACKEND", session->gdk_backend, 1);
    }
    if (session->protocol == SC_PROTO_WAYLAND) {
        const char *wl_env = getenv("WAYLAND_DISPLAY");
        if ((!wl_env || !*wl_env) && session->wayland_display[0] != '\0') {
            setenv("WAYLAND_DISPLAY", session->wayland_display, 1);
        }
        const char *rt_env = getenv("XDG_RUNTIME_DIR");
        if (!rt_env || !*rt_env) {
            char uid_rt[128];
            snprintf(uid_rt, sizeof(uid_rt), "/run/user/%u", (unsigned int)getuid());
            struct stat st;
            if (stat(uid_rt, &st) == 0 && S_ISDIR(st.st_mode)) {
                setenv("XDG_RUNTIME_DIR", uid_rt, 1);
            }
        }
    } else if (session->protocol == SC_PROTO_X11) {
        const char *x11_env = getenv("DISPLAY");
        if ((!x11_env || !*x11_env) && session->x11_display[0] != '\0') {
            setenv("DISPLAY", session->x11_display, 1);
        }
    }
}

void sc_reconcile_with_gdk_backend_name(ScDisplaySession *session, const char *gdk_type_name) {
    if (!session || !gdk_type_name || !*gdk_type_name) return;
    bool is_wayland = (strstr(gdk_type_name, "Wayland") != NULL || strstr(gdk_type_name, "wayland") != NULL);
    bool is_x11 = (strstr(gdk_type_name, "X11") != NULL || strstr(gdk_type_name, "x11") != NULL);

    if (is_wayland && session->protocol != SC_PROTO_WAYLAND) {
        session->protocol = SC_PROTO_WAYLAND;
        snprintf(session->protocol_name, sizeof(session->protocol_name), "wayland");
        snprintf(session->detection_source, sizeof(session->detection_source), "GdkDisplay=%s", gdk_type_name);
        populate_protocol_drivers(session, os_has_binary_cb, NULL);
    } else if (is_x11 && session->protocol != SC_PROTO_X11) {
        session->protocol = SC_PROTO_X11;
        snprintf(session->protocol_name, sizeof(session->protocol_name), "x11");
        snprintf(session->detection_source, sizeof(session->detection_source), "GdkDisplay=%s", gdk_type_name);
        populate_protocol_drivers(session, os_has_binary_cb, NULL);
    }
}

static void json_escape_copy(const char *src, char *dst, size_t dst_size) {
    size_t j = 0;
    for (size_t i = 0; src && src[i] != '\0' && j + 2 < dst_size; ++i) {
        if (src[i] == '"' || src[i] == '\\') {
            dst[j++] = '\\';
        }
        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

int sc_format_display_session_json(const ScDisplaySession *session, char *buf, size_t buf_size) {
    if (!session || !buf || buf_size == 0) return 0;
    char src_esc[256], desk_esc[128], cap_esc[192], clip_esc[128];
    json_escape_copy(session->detection_source, src_esc, sizeof(src_esc));
    json_escape_copy(session->desktop_environment, desk_esc, sizeof(desk_esc));
    json_escape_copy(session->capture_driver, cap_esc, sizeof(cap_esc));
    json_escape_copy(session->clipboard_driver, clip_esc, sizeof(clip_esc));

    return snprintf(
        buf,
        buf_size,
        "{\n"
        "  \"protocol\": \"%s\",\n"
        "  \"detection_source\": \"%s\",\n"
        "  \"gdk_backend\": \"%s\",\n"
        "  \"desktop_environment\": \"%s\",\n"
        "  \"wayland_display\": \"%s\",\n"
        "  \"x11_display\": \"%s\",\n"
        "  \"capture_driver\": \"%s\",\n"
        "  \"clipboard_driver\": \"%s\",\n"
        "  \"supports_window_enumeration\": %s\n"
        "}\n",
        session->protocol_name,
        src_esc,
        session->gdk_backend,
        desk_esc,
        session->wayland_display,
        session->x11_display,
        cap_esc,
        clip_esc,
        session->supports_window_enumeration ? "true" : "false"
    );
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool sc_normalize_launcher_path(const char *raw_arg, char *out_path, size_t out_size) {
    if (!out_path || out_size == 0) return false;
    out_path[0] = '\0';
    if (!raw_arg) return false;

    char trimmed[1024];
    copy_trimmed(trimmed, sizeof(trimmed), raw_arg);
    if (trimmed[0] == '\0') return false;

    /* Ignore unexpanded Freedesktop field codes (%f, %F, %u, %U, %i, %c, %k) */
    if (trimmed[0] == '%' && trimmed[1] != '\0' && trimmed[2] == '\0') {
        return false;
    }

    const char *src = trimmed;
    if (strncmp(trimmed, "file://", 7) == 0) {
        src = trimmed + 7;
        if (strncmp(src, "localhost/", 10) == 0) {
            src += 9;
        }
    }

    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < out_size; ++i) {
        if (src[i] == '%' && hex_val(src[i + 1]) >= 0 && hex_val(src[i + 2]) >= 0) {
            out_path[j++] = (char)((hex_val(src[i + 1]) << 4) | hex_val(src[i + 2]));
            i += 2;
        } else {
            out_path[j++] = src[i];
        }
    }
    out_path[j] = '\0';
    return out_path[0] != '\0';
}
