#ifndef SNIPCLIP_GLOBAL_HOTKEY_H
#define SNIPCLIP_GLOBAL_HOTKEY_H

/*
 * System-wide capture hotkeys (PrintScreen + the user's custom shortcut) that fire even when the
 * SnipClip window is unfocused, hidden, or running resident via --background.
 *
 *  - X11:     XGrabKey on the root window for every NumLock/CapsLock combination (0, Mod2Mask,
 *             LockMask, Mod2Mask|LockMask) behind a scoped X11 error trap, so BadAccess from a key
 *             already grabbed by another client (WM / screenshot daemon) is recorded, never fatal.
 *             Key events are routed through a gdk_window_add_filter() root event filter.
 *  - Wayland: fully asynchronous org.freedesktop.portal.GlobalShortcuts session
 *             (CreateSession -> BindShortcuts -> Activated). Startup is never blocked, and nothing
 *             is printed when the compositor / portal does not implement the interface.
 */

#include <gtk/gtk.h>
#include <stdbool.h>

#include "display_detect.h"

typedef void (*ScHotkeyActivatedFn)(void *user_data);

typedef struct ScGlobalHotkey ScGlobalHotkey;

/* Create the listener for the given (post-GDK-reconciliation) session; never blocks */
ScGlobalHotkey *sc_global_hotkey_new(const ScDisplaySession *session, ScHotkeyActivatedFn on_activated, void *user_data);

/* (Re)bind PrintScreen + custom shortcut, releasing any previous grabs / portal session first.
 * Returns true when at least one binding was installed (X11) or an async bind was started (Wayland). */
bool sc_global_hotkey_bind(ScGlobalHotkey *hk, guint custom_keyval, GdkModifierType custom_mods);

/* Number of X11 key grabs rejected with BadAccess during the last bind (diagnostics / tests) */
int sc_global_hotkey_x11_conflicts(const ScGlobalHotkey *hk);

/* Ungrab everything, close the portal session, remove filters, and free */
void sc_global_hotkey_free(ScGlobalHotkey *hk);

#endif /* SNIPCLIP_GLOBAL_HOTKEY_H */
