#ifndef SNIPCLIP_CONFIG_STORE_H
#define SNIPCLIP_CONFIG_STORE_H

/*
 * Pure C11 / POSIX persistence for SnipClip user preferences.
 *
 * Stored at $XDG_CONFIG_HOME/snipclip/config.ini (fallback $HOME/.config/snipclip/config.ini):
 *
 *   [snipclip]
 *   snip_mode=rectangle
 *   delay_seconds=0
 *   custom_shortcut=Ctrl+Shift+S
 *
 * Deliberately GTK/GLib-free so the headless test_snipclip_native binary links with -lm only.
 * All functions degrade silently (return false) on missing / unreadable / unwritable paths and
 * never write to stdout or stderr.
 */

#include <stdbool.h>
#include <stddef.h>

#include "geometry.h"

#define SC_CONFIG_SHORTCUT_MAX 64
#define SC_CONFIG_DEFAULT_SHORTCUT "Ctrl+Shift+S"

typedef struct {
    ScSnipMode snip_mode;
    int delay_seconds;
    char custom_shortcut[SC_CONFIG_SHORTCUT_MAX];
} ScConfig;

/* Reset to built-in defaults (rectangle, 0s delay, Ctrl+Shift+S) */
void sc_config_init_defaults(ScConfig *cfg);

/* Only 0, 3, 5 and 10 second capture delays are supported */
bool sc_config_is_valid_delay(int delay_seconds);

/* Resolve "<xdg_config_home>/snipclip/config.ini", falling back to "<home>/.config/snipclip/config.ini".
 * Relative or empty XDG_CONFIG_HOME values are ignored per the XDG Base Directory specification. */
bool sc_config_resolve_path_custom(const char *xdg_config_home, const char *home, char *out, size_t out_size);

/* Same as above using the live XDG_CONFIG_HOME / HOME environment variables */
bool sc_config_default_path(char *out, size_t out_size);

/* Load values from path into cfg. Keys that are missing, malformed, or invalid keep their current
 * values in cfg (call sc_config_init_defaults first). Returns true when the file was read. */
bool sc_config_load(const char *path, ScConfig *cfg);

/* Atomically write cfg to path (temp file + fsync + rename), creating parent directories with
 * mode 0700. Returns false (leaving any existing file untouched) on failure. */
bool sc_config_save(const char *path, const ScConfig *cfg);

#endif /* SNIPCLIP_CONFIG_STORE_H */
