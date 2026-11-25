#define _XOPEN_SOURCE 700

#include "config_store.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SC_CONFIG_LINE_MAX 512

void sc_config_init_defaults(ScConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->snip_mode = SC_MODE_RECTANGLE;
    cfg->delay_seconds = 0;
    snprintf(cfg->custom_shortcut, sizeof(cfg->custom_shortcut), "%s", SC_CONFIG_DEFAULT_SHORTCUT);
}

bool sc_config_is_valid_delay(int delay_seconds) {
    return delay_seconds == 0 || delay_seconds == 3 || delay_seconds == 5 || delay_seconds == 10;
}

bool sc_config_resolve_path_custom(const char *xdg_config_home, const char *home, char *out, size_t out_size) {
    if (!out || out_size == 0) return false;
    out[0] = '\0';

    int n = -1;
    if (xdg_config_home && xdg_config_home[0] == '/') {
        n = snprintf(out, out_size, "%s/snipclip/config.ini", xdg_config_home);
    } else if (home && home[0] == '/') {
        n = snprintf(out, out_size, "%s/.config/snipclip/config.ini", home);
    }
    if (n < 0 || (size_t)n >= out_size) {
        out[0] = '\0';
        return false;
    }
    return true;
}

bool sc_config_default_path(char *out, size_t out_size) {
    return sc_config_resolve_path_custom(getenv("XDG_CONFIG_HOME"), getenv("HOME"), out, out_size);
}

static char *trim_inplace(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
    return s;
}

static bool is_valid_shortcut_value(const char *v) {
    if (!v || !*v) return false;
    size_t len = strlen(v);
    if (len >= SC_CONFIG_SHORTCUT_MAX) return false;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)v[i];
        /* Printable ASCII only: accelerator names never need control characters or UTF-8 */
        if (c < 0x20 || c > 0x7e) return false;
    }
    return true;
}

static bool parse_delay_value(const char *v, int *out) {
    if (!v || !*v) return false;
    char *end = NULL;
    errno = 0;
    long d = strtol(v, &end, 10);
    if (errno != 0 || end == v || *end != '\0') return false;
    if (d < 0 || d > 3600) return false;
    if (!sc_config_is_valid_delay((int)d)) return false;
    *out = (int)d;
    return true;
}

bool sc_config_load(const char *path, ScConfig *cfg) {
    if (!path || !*path || !cfg) return false;

    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    bool in_snipclip_section = true; /* keys before any section header are accepted */
    char line[SC_CONFIG_LINE_MAX];
    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] != '\n' && !feof(fp)) {
            /* Over-long line: discard the remainder and ignore it entirely */
            int ch;
            while ((ch = fgetc(fp)) != EOF && ch != '\n') {
            }
            continue;
        }

        char *s = trim_inplace(line);
        if (*s == '\0' || *s == '#' || *s == ';') continue;

        if (*s == '[') {
            char *close = strchr(s, ']');
            if (!close) {
                in_snipclip_section = false;
                continue;
            }
            *close = '\0';
            char *name = trim_inplace(s + 1);
            in_snipclip_section = strcmp(name, "snipclip") == 0;
            continue;
        }
        if (!in_snipclip_section) continue;

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim_inplace(s);
        char *val = trim_inplace(eq + 1);

        if (strcmp(key, "snip_mode") == 0) {
            ScSnipMode m = cfg->snip_mode;
            if (sc_snip_mode_from_string(val, &m)) {
                cfg->snip_mode = m;
            }
        } else if (strcmp(key, "delay_seconds") == 0) {
            int d = 0;
            if (parse_delay_value(val, &d)) {
                cfg->delay_seconds = d;
            }
        } else if (strcmp(key, "custom_shortcut") == 0) {
            if (is_valid_shortcut_value(val)) {
                snprintf(cfg->custom_shortcut, sizeof(cfg->custom_shortcut), "%s", val);
            }
        }
    }
    fclose(fp);
    return true;
}

/* mkdir -p for every parent component of file_path, creating missing directories as 0700 */
static bool ensure_parent_dirs(const char *file_path) {
    char buf[4096];
    int n = snprintf(buf, sizeof(buf), "%s", file_path);
    if (n < 0 || (size_t)n >= sizeof(buf)) return false;

    char *last_slash = strrchr(buf, '/');
    if (!last_slash || last_slash == buf) return true;
    *last_slash = '\0';

    for (char *p = buf + 1; ; ++p) {
        bool at_end = (*p == '\0');
        if (*p == '/' || at_end) {
            char saved = *p;
            *p = '\0';
            if (mkdir(buf, 0700) != 0 && errno != EEXIST) {
                return false;
            }
            struct stat st;
            if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) {
                return false;
            }
            *p = saved;
            if (at_end) break;
        }
    }
    return true;
}

bool sc_config_save(const char *path, const ScConfig *cfg) {
    if (!path || !*path || !cfg) return false;
    if (!is_valid_shortcut_value(cfg->custom_shortcut)) return false;
    if (!sc_config_is_valid_delay(cfg->delay_seconds)) return false;
    if (!ensure_parent_dirs(path)) return false;

    char tmp_path[4200];
    int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%ld", path, (long)getpid());
    if (n < 0 || (size_t)n >= sizeof(tmp_path)) return false;

    int fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return false;

    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        close(fd);
        unlink(tmp_path);
        return false;
    }

    int written = fprintf(
        fp,
        "# SnipClip preferences (written automatically; CLI flags override for a single run)\n"
        "[snipclip]\n"
        "snip_mode=%s\n"
        "delay_seconds=%d\n"
        "custom_shortcut=%s\n",
        sc_snip_mode_to_string(cfg->snip_mode),
        cfg->delay_seconds,
        cfg->custom_shortcut
    );

    bool ok = written > 0;
    if (fflush(fp) != 0) ok = false;
    if (ok && fsync(fileno(fp)) != 0) ok = false;
    if (fclose(fp) != 0) ok = false;

    if (!ok || rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return false;
    }
    return true;
}
