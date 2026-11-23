#define _POSIX_C_SOURCE 200809L

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "capture_backend.h"
#include "display_detect.h"
#include "editor_canvas.h"
#include "geometry.h"
#include "overlay.h"
#include "snipclip_version.h"

typedef struct {
    GtkWidget *window;
    GtkWidget *new_btn;
    GtkWidget *mode_combo;
    GtkWidget *delay_combo;
    GtkWidget *tool_btns[10];
    GtkWidget *ruler_toggle;
    GtkWidget *color_btn;
    GtkWidget *width_scale;
    GtkWidget *shortcut_btn;
    GtkWidget *undo_btn;
    GtkWidget *redo_btn;
    GtkWidget *crop_banner;
    GtkWidget *status_session_lbl;
    GtkWidget *status_msg_lbl;
    GtkWidget *status_dims_lbl;
    GtkWidget *status_zoom_lbl;

    ScEditorCanvas *editor;
    ScDisplaySession session;
    ScSnipMode snip_mode;
    int delay_seconds;
    int countdown_remaining;
    guint countdown_timer_id;
    bool capture_in_progress;

    guint custom_keyval;
    GdkModifierType custom_mods;
    char custom_shortcut_label[64];
} ScApp;

static const char *APP_CSS =
    "window { background-color: #f8fafc; color: #0f172a; }"
    ".sc-cmd-bar { background-color: #ffffff; border-bottom: 1px solid #e2e8f0; padding: 6px 10px; }"
    ".sc-crop-bar { background-color: #eff6ff; border-bottom: 1px solid #bfdbfe; padding: 6px 12px; }"
    ".sc-status-bar { background-color: #ffffff; border-top: 1px solid #e2e8f0; padding: 5px 12px; font-size: 12px; color: #475569; }"
    "button.sc-primary-btn { background-image: none; background-color: #0f172a; color: #ffffff; border: 1px solid #0f172a; border-radius: 6px; padding: 5px 12px; font-weight: 700; }"
    "button.sc-primary-btn:hover { background-color: #1e293b; }"
    "button.sc-tool-btn { background-image: none; background-color: #ffffff; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 6px; padding: 5px 9px; font-weight: 600; font-size: 12px; }"
    "button.sc-tool-btn:hover { background-color: #f1f5f9; }"
    "button.sc-tool-active { background-image: none; background-color: #dbeafe; color: #1d4ed8; border-color: #2563eb; }"
    "button.sc-badge-btn { background-image: none; background-color: #f1f5f9; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 6px; padding: 5px 10px; font-family: monospace; font-size: 11px; font-weight: 700; }"
    ".sc-overlay-bar { background-color: #1e293b; border: 1px solid #334155; border-radius: 8px; padding: 5px 6px; }"
    "button.sc-overlay-btn { background-image: none; background-color: transparent; color: #e2e8f0; border: none; border-radius: 6px; padding: 6px 12px; font-weight: 600; font-size: 12px; }"
    "button.sc-overlay-btn:hover { background-color: #334155; }"
    "button.sc-overlay-active { background-image: none; background-color: #2563eb; color: #ffffff; }";

static void app_set_status_message(ScApp *app, const char *msg) {
    if (!app || !app->status_msg_lbl) return;
    gtk_label_set_text(GTK_LABEL(app->status_msg_lbl), msg ? msg : "");
}

static void app_sync_tool_buttons(ScApp *app) {
    ScToolKind active = sc_editor_get_tool(app->editor);
    for (int i = 0; i < 10; ++i) {
        if (!app->tool_btns[i]) continue;
        GtkStyleContext *sc = gtk_widget_get_style_context(app->tool_btns[i]);
        if ((int)active == i) {
            gtk_style_context_add_class(sc, "sc-tool-active");
        } else {
            gtk_style_context_remove_class(sc, "sc-tool-active");
        }
    }
}

static void on_editor_state_changed(void *user_data) {
    ScApp *app = (ScApp *)user_data;
    if (!app || !app->editor) return;

    gtk_widget_set_sensitive(app->undo_btn, sc_editor_can_undo(app->editor));
    gtk_widget_set_sensitive(app->redo_btn, sc_editor_can_redo(app->editor));
    gtk_widget_set_visible(app->crop_banner, sc_editor_is_crop_active(app->editor));
    app_sync_tool_buttons(app);

    int w = 0, h = 0;
    sc_editor_get_dimensions(app->editor, &w, &h);
    if (w > 0 && h > 0) {
        char dims[64];
        snprintf(dims, sizeof(dims), "%d x %d px", w, h);
        gtk_label_set_text(GTK_LABEL(app->status_dims_lbl), dims);
    } else {
        gtk_label_set_text(GTK_LABEL(app->status_dims_lbl), "No snip loaded");
    }

    char zbuf[32];
    snprintf(zbuf, sizeof(zbuf), "%d%%", (int)llround(sc_editor_get_zoom(app->editor) * 100.0));
    gtk_label_set_text(GTK_LABEL(app->status_zoom_lbl), zbuf);
}

static bool parse_shortcut_accelerator(
    const char *spec,
    guint *out_keyval,
    GdkModifierType *out_mods,
    char *out_display,
    size_t out_display_sz
) {
    if (!spec || !*spec) return false;

    /* Support both "Ctrl+Shift+S" human format and "<Control><Shift>s" GTK accelerator format */
    char gtk_spec[128] = "";
    if (strchr(spec, '<') != NULL) {
        snprintf(gtk_spec, sizeof(gtk_spec), "%s", spec);
    } else {
        char copy[128];
        snprintf(copy, sizeof(copy), "%s", spec);
        char *saveptr = NULL;
        for (char *tok = strtok_r(copy, "+", &saveptr); tok != NULL; tok = strtok_r(NULL, "+", &saveptr)) {
            if (g_ascii_strcasecmp(tok, "Ctrl") == 0 || g_ascii_strcasecmp(tok, "Control") == 0) {
                strncat(gtk_spec, "<Control>", sizeof(gtk_spec) - strlen(gtk_spec) - 1);
            } else if (g_ascii_strcasecmp(tok, "Shift") == 0) {
                strncat(gtk_spec, "<Shift>", sizeof(gtk_spec) - strlen(gtk_spec) - 1);
            } else if (g_ascii_strcasecmp(tok, "Alt") == 0) {
                strncat(gtk_spec, "<Alt>", sizeof(gtk_spec) - strlen(gtk_spec) - 1);
            } else if (g_ascii_strcasecmp(tok, "Super") == 0 || g_ascii_strcasecmp(tok, "Meta") == 0) {
                strncat(gtk_spec, "<Super>", sizeof(gtk_spec) - strlen(gtk_spec) - 1);
            } else if (g_ascii_strcasecmp(tok, "PrintScreen") == 0) {
                strncat(gtk_spec, "Print", sizeof(gtk_spec) - strlen(gtk_spec) - 1);
            } else {
                strncat(gtk_spec, tok, sizeof(gtk_spec) - strlen(gtk_spec) - 1);
            }
        }
    }

    guint kv = 0;
    GdkModifierType mods = 0;
    gtk_accelerator_parse(gtk_spec, &kv, &mods);
    if (kv == 0) return false;

    *out_keyval = gdk_keyval_to_lower(kv);
    *out_mods = mods & gtk_accelerator_get_default_mod_mask();
    gchar *label = gtk_accelerator_get_label(kv, *out_mods);
    snprintf(out_display, out_display_sz, "%s", label ? label : spec);
    g_free(label);
    return true;
}

static void on_snip_completed(GdkPixbuf *captured, ScSnipMode mode, void *user_data) {
    (void)mode;
    ScApp *app = (ScApp *)user_data;
    app->capture_in_progress = false;

    if (captured) {
        sc_editor_set_pixbuf(app->editor, captured);
        sc_copy_pixbuf_to_clipboard(captured, &app->session);
        app_set_status_message(app, "Snip captured and copied to clipboard.");
    }

    gtk_widget_show_all(app->window);
    gtk_widget_set_visible(app->crop_banner, sc_editor_is_crop_active(app->editor));
    gtk_window_present(GTK_WINDOW(app->window));
}

static void on_snip_cancelled(void *user_data) {
    ScApp *app = (ScApp *)user_data;
    app->capture_in_progress = false;
    app_set_status_message(app, "Snip cancelled.");
    gtk_widget_show_all(app->window);
    gtk_widget_set_visible(app->crop_banner, sc_editor_is_crop_active(app->editor));
    gtk_window_present(GTK_WINDOW(app->window));
}

static gboolean perform_delayed_desktop_grab(gpointer user_data) {
    ScApp *app = (ScApp *)user_data;
    int ox = 0, oy = 0;

    if (app->session.protocol == SC_PROTO_WAYLAND && app->snip_mode == SC_MODE_WINDOW) {
        GdkPixbuf *win_pix = sc_capture_desktop_pixbuf(&app->session, true, &ox, &oy);
        if (win_pix) {
            on_snip_completed(win_pix, SC_MODE_WINDOW, app);
            g_object_unref(win_pix);
            return G_SOURCE_REMOVE;
        }
    }

    GdkPixbuf *desktop = sc_capture_desktop_pixbuf(&app->session, false, &ox, &oy);
    if (!desktop) {
        app->capture_in_progress = false;
        gtk_widget_show_all(app->window);
        gtk_widget_set_visible(app->crop_banner, sc_editor_is_crop_active(app->editor));
        gtk_window_present(GTK_WINDOW(app->window));

        GtkWidget *dlg = gtk_message_dialog_new(
            GTK_WINDOW(app->window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_OK,
            "Unable to capture desktop pixels.\n\nOn Wayland compositors, ensure xdg-desktop-portal (or grim / spectacle) is active."
        );
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        return G_SOURCE_REMOVE;
    }

    ScWindowInfo wins[SC_MAX_WINDOWS];
    size_t win_count = 0;
    if (app->session.protocol == SC_PROTO_X11 && app->snip_mode == SC_MODE_WINDOW) {
        win_count = sc_enumerate_x11_windows(wins, SC_MAX_WINDOWS);
    }

    sc_launch_capture_overlay(
        desktop,
        ox,
        oy,
        app->snip_mode,
        wins,
        win_count,
        on_snip_completed,
        on_snip_cancelled,
        app
    );
    g_object_unref(desktop);
    return G_SOURCE_REMOVE;
}

static void begin_capture_sequence(ScApp *app) {
    app->capture_in_progress = true;
    gtk_widget_hide(app->window);
    while (gtk_events_pending()) {
        gtk_main_iteration_do(FALSE);
    }
    g_timeout_add(200, perform_delayed_desktop_grab, app);
}

static gboolean on_countdown_tick(gpointer user_data) {
    ScApp *app = (ScApp *)user_data;
    if (app->countdown_remaining <= 0) {
        app->countdown_timer_id = 0;
        gtk_button_set_label(GTK_BUTTON(app->new_btn), "+ New");
        gtk_widget_set_sensitive(app->new_btn, TRUE);
        begin_capture_sequence(app);
        return G_SOURCE_REMOVE;
    }

    char buf[48];
    snprintf(buf, sizeof(buf), "Snipping in %ds...", app->countdown_remaining);
    gtk_button_set_label(GTK_BUTTON(app->new_btn), buf);
    app->countdown_remaining--;
    return G_SOURCE_CONTINUE;
}

static void trigger_new_snip(ScApp *app) {
    if (!app || app->capture_in_progress || app->countdown_timer_id != 0) return;

    if (app->delay_seconds > 0) {
        app->countdown_remaining = app->delay_seconds - 1;
        gtk_widget_set_sensitive(app->new_btn, FALSE);
        char buf[48];
        snprintf(buf, sizeof(buf), "Snipping in %ds...", app->delay_seconds);
        gtk_button_set_label(GTK_BUTTON(app->new_btn), buf);
        app->countdown_timer_id = g_timeout_add(1000, on_countdown_tick, app);
    } else {
        begin_capture_sequence(app);
    }
}

static void on_new_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    trigger_new_snip((ScApp *)user_data);
}

static void on_mode_changed(GtkComboBox *combo, gpointer user_data) {
    ScApp *app = (ScApp *)user_data;
    const gchar *id = gtk_combo_box_get_active_id(combo);
    ScSnipMode m = SC_MODE_RECTANGLE;
    if (id && sc_snip_mode_from_string(id, &m)) {
        app->snip_mode = m;
    }
}

static void on_delay_changed(GtkComboBox *combo, gpointer user_data) {
    ScApp *app = (ScApp *)user_data;
    const gchar *id = gtk_combo_box_get_active_id(combo);
    if (id) {
        app->delay_seconds = atoi(id);
    }
}

static void on_tool_clicked(GtkButton *btn, gpointer user_data) {
    ScApp *app = (ScApp *)user_data;
    int t = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "sc-tool"));
    sc_editor_set_tool(app->editor, (ScToolKind)t);
}

static void on_ruler_toggled(GtkToggleButton *btn, gpointer user_data) {
    ScApp *app = (ScApp *)user_data;
    sc_editor_set_ruler_visible(app->editor, gtk_toggle_button_get_active(btn));
}

static void on_color_set(GtkColorButton *btn, gpointer user_data) {
    ScApp *app = (ScApp *)user_data;
    GdkRGBA rgba;
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(btn), &rgba);
    sc_editor_set_color_rgba(app->editor, &rgba);
}

static void on_width_changed(GtkRange *range, gpointer user_data) {
    ScApp *app = (ScApp *)user_data;
    sc_editor_set_stroke_width(app->editor, gtk_range_get_value(range));
}

static void on_undo_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    ScApp *app = (ScApp *)user_data;
    sc_editor_undo(app->editor);
}

static void on_redo_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    ScApp *app = (ScApp *)user_data;
    sc_editor_redo(app->editor);
}

static void on_clear_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    ScApp *app = (ScApp *)user_data;
    sc_editor_clear_ink(app->editor);
}

static void on_apply_crop_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    ScApp *app = (ScApp *)user_data;
    sc_editor_apply_crop(app->editor);
}

static void on_cancel_crop_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    ScApp *app = (ScApp *)user_data;
    sc_editor_cancel_crop(app->editor);
}

static void action_copy_clipboard(ScApp *app) {
    if (!sc_editor_has_image(app->editor)) return;
    GdkPixbuf *flat = sc_editor_render_pixbuf(app->editor);
    if (flat) {
        sc_copy_pixbuf_to_clipboard(flat, &app->session);
        g_object_unref(flat);
        app_set_status_message(app, "Copied annotated snip to clipboard.");
    }
}

static void on_copy_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    action_copy_clipboard((ScApp *)user_data);
}

static void action_save_file(ScApp *app) {
    if (!sc_editor_has_image(app->editor)) return;

    GtkWidget *dlg = gtk_file_chooser_dialog_new(
        "Save Snip As",
        GTK_WINDOW(app->window),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel",
        GTK_RESPONSE_CANCEL,
        "_Save",
        GTK_RESPONSE_ACCEPT,
        NULL
    );
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), "SnipClip-Capture.png");

    const char *pics = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
    if (pics) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), pics);
    }

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (filename) {
            GdkPixbuf *flat = sc_editor_render_pixbuf(app->editor);
            if (flat) {
                const char *fmt = "png";
                size_t flen = strlen(filename);
                if (flen > 4 && (g_ascii_strcasecmp(filename + flen - 4, ".jpg") == 0 ||
                                 g_ascii_strcasecmp(filename + flen - 5, ".jpeg") == 0)) {
                    fmt = "jpeg";
                }
                GError *err = NULL;
                if (gdk_pixbuf_save(flat, filename, fmt, &err, NULL)) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Saved snip to %s", filename);
                    app_set_status_message(app, msg);
                } else if (err) {
                    g_error_free(err);
                }
                g_object_unref(flat);
            }
            g_free(filename);
        }
    }
    gtk_widget_destroy(dlg);
}

static void on_save_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    action_save_file((ScApp *)user_data);
}

static void action_open_file(ScApp *app) {
    GtkWidget *dlg = gtk_file_chooser_dialog_new(
        "Open Image in SnipClip",
        GTK_WINDOW(app->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel",
        GTK_RESPONSE_CANCEL,
        "_Open",
        GTK_RESPONSE_ACCEPT,
        NULL
    );
    GtkFileFilter *flt = gtk_file_filter_new();
    gtk_file_filter_set_name(flt, "Images (PNG, JPEG, WebP, BMP)");
    gtk_file_filter_add_pixbuf_formats(flt);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), flt);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (filename) {
            GdkPixbuf *loaded = gdk_pixbuf_new_from_file(filename, NULL);
            if (loaded) {
                sc_editor_set_pixbuf(app->editor, loaded);
                g_object_unref(loaded);
                app_set_status_message(app, "Opened image file.");
            }
            g_free(filename);
        }
    }
    gtk_widget_destroy(dlg);
}

static void on_open_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    action_open_file((ScApp *)user_data);
}

typedef struct {
    GtkWidget *dialog;
    GtkWidget *prompt_lbl;
    guint recorded_kv;
    GdkModifierType recorded_mods;
    bool valid;
} ShortcutRecordState;

static gboolean on_record_shortcut_key(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)widget;
    ShortcutRecordState *st = (ShortcutRecordState *)user_data;
    if (event->is_modifier) return TRUE;
    if (event->keyval == GDK_KEY_Escape) {
        gtk_dialog_response(GTK_DIALOG(st->dialog), GTK_RESPONSE_CANCEL);
        return TRUE;
    }

    GdkModifierType mods = event->state & gtk_accelerator_get_default_mod_mask();
    guint lower_kv = gdk_keyval_to_lower(event->keyval);

    bool is_fn_or_print =
        (lower_kv == GDK_KEY_Print || lower_kv == GDK_KEY_Sys_Req ||
         (lower_kv >= GDK_KEY_F1 && lower_kv <= GDK_KEY_F12));

    if (!is_fn_or_print && mods == 0) {
        gtk_label_set_text(
            GTK_LABEL(st->prompt_lbl),
            "Combine regular keys with Ctrl, Alt, or Shift (or press F1-F12 / PrintScreen)."
        );
        return TRUE;
    }

    st->recorded_kv = lower_kv;
    st->recorded_mods = mods;
    st->valid = true;
    gtk_dialog_response(GTK_DIALOG(st->dialog), GTK_RESPONSE_ACCEPT);
    return TRUE;
}

static void on_shortcut_btn_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    ScApp *app = (ScApp *)user_data;

    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        "Customize Capture Shortcut",
        GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel",
        GTK_RESPONSE_CANCEL,
        NULL
    );
    gtk_window_set_default_size(GTK_WINDOW(dlg), 420, 150);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);

    char info[192];
    snprintf(
        info,
        sizeof(info),
        "Press any key combination to trigger New Snip.\n(PrintScreen is always active alongside %s.)",
        app->custom_shortcut_label
    );
    GtkWidget *lbl = gtk_label_new(info);
    gtk_box_pack_start(GTK_BOX(content), lbl, TRUE, TRUE, 8);
    gtk_widget_show_all(content);

    ShortcutRecordState st = { dlg, lbl, 0, 0, false };
    g_signal_connect(dlg, "key-press-event", G_CALLBACK(on_record_shortcut_key), &st);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT && st.valid) {
        app->custom_keyval = st.recorded_kv;
        app->custom_mods = st.recorded_mods;
        gchar *acc_lbl = gtk_accelerator_get_label(st.recorded_kv, st.recorded_mods);
        snprintf(
            app->custom_shortcut_label,
            sizeof(app->custom_shortcut_label),
            "%s",
            acc_lbl ? acc_lbl : "Custom"
        );
        g_free(acc_lbl);

        char btn_txt[80];
        snprintf(btn_txt, sizeof(btn_txt), "Key: %s", app->custom_shortcut_label);
        gtk_button_set_label(GTK_BUTTON(app->shortcut_btn), btn_txt);
        app_set_status_message(app, "Updated custom capture shortcut.");
    }
    gtk_widget_destroy(dlg);
}

static void on_about_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    ScApp *app = (ScApp *)user_data;

    GtkWidget *dlg = gtk_message_dialog_new_with_markup(
        GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "<b>SnipClip v%s (Native GTK 3 / C Binary)</b>\n"
        "Windows Snipping Tool clone for Debian-based Linux distributions.\n\n"
        "<b>GitHub Release Tag:</b> %s\n"
        "<b>Debian Package:</b> %s\n\n"
        "<b>Auto-Detected Display Server:</b> %s\n"
        "<b>Detection Source:</b> %s\n"
        "<b>Desktop Environment:</b> %s\n"
        "<b>Capture Backend:</b> %s\n"
        "<b>Clipboard Backend:</b> %s",
        SNIPCLIP_VERSION,
        SNIPCLIP_RELEASE_TAG,
        SNIPCLIP_DEB_FILENAME,
        app->session.protocol == SC_PROTO_WAYLAND
            ? "WAYLAND"
            : (app->session.protocol == SC_PROTO_X11 ? "X11" : "HEADLESS"),
        app->session.detection_source,
        app->session.desktop_environment,
        app->session.capture_driver,
        app->session.clipboard_driver
    );
    gtk_window_set_title(GTK_WINDOW(dlg), "About SnipClip & Display Diagnostics");
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static gboolean on_main_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)widget;
    ScApp *app = (ScApp *)user_data;

    guint kv = gdk_keyval_to_lower(event->keyval);
    GdkModifierType mods = event->state & gtk_accelerator_get_default_mod_mask();

    /* PrintScreen / SysRq always triggers capture immediately */
    if (kv == GDK_KEY_Print || kv == GDK_KEY_Sys_Req || event->keyval == GDK_KEY_Print) {
        trigger_new_snip(app);
        return TRUE;
    }

    /* User-configurable custom capture shortcut (default Ctrl+Shift+S) */
    if (app->custom_keyval != 0 && kv == app->custom_keyval && mods == app->custom_mods) {
        trigger_new_snip(app);
        return TRUE;
    }

    if (mods == GDK_CONTROL_MASK) {
        if (kv == GDK_KEY_n) {
            trigger_new_snip(app);
            return TRUE;
        }
        if (kv == GDK_KEY_c) {
            action_copy_clipboard(app);
            return TRUE;
        }
        if (kv == GDK_KEY_s) {
            action_save_file(app);
            return TRUE;
        }
        if (kv == GDK_KEY_o) {
            action_open_file(app);
            return TRUE;
        }
        if (kv == GDK_KEY_z) {
            sc_editor_undo(app->editor);
            return TRUE;
        }
        if (kv == GDK_KEY_y) {
            sc_editor_redo(app->editor);
            return TRUE;
        }
        if (kv == GDK_KEY_plus || kv == GDK_KEY_equal) {
            sc_editor_set_zoom(app->editor, sc_editor_get_zoom(app->editor) + 0.25);
            return TRUE;
        }
        if (kv == GDK_KEY_minus) {
            sc_editor_set_zoom(app->editor, sc_editor_get_zoom(app->editor) - 0.25);
            return TRUE;
        }
        if (kv == GDK_KEY_0) {
            sc_editor_set_zoom(app->editor, 1.0);
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean on_autostart_snip(gpointer user_data) {
    trigger_new_snip((ScApp *)user_data);
    return G_SOURCE_REMOVE;
}

static void build_main_window(ScApp *app, const char *initial_image_path, bool autostart) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, APP_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(css);

    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1120, 720);

    const char *proto_upper = app->session.protocol == SC_PROTO_WAYLAND
                                  ? "WAYLAND"
                                  : (app->session.protocol == SC_PROTO_X11 ? "X11" : "HEADLESS");
    char title[128];
    snprintf(title, sizeof(title), "SnipClip v%s — %s Auto-Detected", SNIPCLIP_VERSION, proto_upper);
    gtk_window_set_title(GTK_WINDOW(app->window), title);
    gtk_window_set_icon_name(GTK_WINDOW(app->window), "snipclip");
    if (g_file_test("/usr/share/snipclip/snipclip.png", G_FILE_TEST_EXISTS)) {
        gtk_window_set_icon_from_file(GTK_WINDOW(app->window), "/usr/share/snipclip/snipclip.png", NULL);
    }

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app->window), vbox);

    /* Top Command Bar */
    GtkWidget *cmd_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_style_context_add_class(gtk_widget_get_style_context(cmd_bar), "sc-cmd-bar");
    gtk_box_pack_start(GTK_BOX(vbox), cmd_bar, FALSE, FALSE, 0);

    app->new_btn = gtk_button_new_with_label("+ New");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->new_btn), "sc-primary-btn");
    gtk_widget_set_tooltip_text(app->new_btn, "Start a new snip (PrintScreen, Custom Shortcut, or Ctrl+N)");
    g_signal_connect(app->new_btn, "clicked", G_CALLBACK(on_new_clicked), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), app->new_btn, FALSE, FALSE, 0);

    app->mode_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->mode_combo), "rectangle", "Rectangle");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->mode_combo), "window", "Window");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->mode_combo), "fullscreen", "Fullscreen");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->mode_combo), "freeform", "Freeform");
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(app->mode_combo), sc_snip_mode_to_string(app->snip_mode));
    g_signal_connect(app->mode_combo, "changed", G_CALLBACK(on_mode_changed), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), app->mode_combo, FALSE, FALSE, 0);

    app->delay_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->delay_combo), "0", "No delay");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->delay_combo), "3", "3s delay");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->delay_combo), "5", "5s delay");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->delay_combo), "10", "10s delay");
    char delay_id[16];
    snprintf(delay_id, sizeof(delay_id), "%d", app->delay_seconds);
    if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(app->delay_combo), delay_id)) {
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(app->delay_combo), "0");
    }
    g_signal_connect(app->delay_combo, "changed", G_CALLBACK(on_delay_changed), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), app->delay_combo, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(cmd_bar), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 3);

    const struct { ScToolKind kind; const char *label; } tools[] = {
        { SC_TOOL_PEN, "Pen" },
        { SC_TOOL_HIGHLIGHTER, "Highlight" },
        { SC_TOOL_ERASER, "Eraser" },
        { SC_TOOL_RECT, "Rect" },
        { SC_TOOL_ELLIPSE, "Ellipse" },
        { SC_TOOL_LINE, "Line" },
        { SC_TOOL_ARROW, "Arrow" },
        { SC_TOOL_REDACT, "Redact" },
        { SC_TOOL_PIXELATE, "Blur" },
        { SC_TOOL_CROP, "Crop" }
    };
    for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); ++i) {
        GtkWidget *b = gtk_button_new_with_label(tools[i].label);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "sc-tool-btn");
        g_object_set_data(G_OBJECT(b), "sc-tool", GINT_TO_POINTER((int)tools[i].kind));
        g_signal_connect(b, "clicked", G_CALLBACK(on_tool_clicked), app);
        gtk_box_pack_start(GTK_BOX(cmd_bar), b, FALSE, FALSE, 0);
        app->tool_btns[tools[i].kind] = b;
    }

    app->ruler_toggle = gtk_toggle_button_new_with_label("Ruler");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->ruler_toggle), "sc-tool-btn");
    g_signal_connect(app->ruler_toggle, "toggled", G_CALLBACK(on_ruler_toggled), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), app->ruler_toggle, FALSE, FALSE, 0);

    GdkRGBA init_col;
    gdk_rgba_parse(&init_col, "#ef4444");
    app->color_btn = gtk_color_button_new_with_rgba(&init_col);
    gtk_widget_set_tooltip_text(app->color_btn, "Select ink / shape color");
    g_signal_connect(app->color_btn, "color-set", G_CALLBACK(on_color_set), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), app->color_btn, FALSE, FALSE, 0);

    app->width_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1.0, 16.0, 1.0);
    gtk_scale_set_draw_value(GTK_SCALE(app->width_scale), FALSE);
    gtk_range_set_value(GTK_RANGE(app->width_scale), 3.0);
    gtk_widget_set_size_request(app->width_scale, 68, -1);
    gtk_widget_set_tooltip_text(app->width_scale, "Stroke thickness");
    g_signal_connect(app->width_scale, "value-changed", G_CALLBACK(on_width_changed), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), app->width_scale, FALSE, FALSE, 0);

    char sc_txt[80];
    snprintf(sc_txt, sizeof(sc_txt), "Key: %s", app->custom_shortcut_label);
    app->shortcut_btn = gtk_button_new_with_label(sc_txt);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->shortcut_btn), "sc-tool-btn");
    gtk_widget_set_tooltip_text(app->shortcut_btn, "Configure PrintScreen / custom capture shortcut");
    g_signal_connect(app->shortcut_btn, "clicked", G_CALLBACK(on_shortcut_btn_clicked), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), app->shortcut_btn, FALSE, FALSE, 0);

    /* Right-aligned Undo / Redo / Export / Display Badge */
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(cmd_bar), spacer, TRUE, TRUE, 0);

    app->undo_btn = gtk_button_new_with_label("Undo");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->undo_btn), "sc-tool-btn");
    gtk_widget_set_sensitive(app->undo_btn, FALSE);
    g_signal_connect(app->undo_btn, "clicked", G_CALLBACK(on_undo_clicked), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), app->undo_btn, FALSE, FALSE, 0);

    app->redo_btn = gtk_button_new_with_label("Redo");
    gtk_style_context_add_class(gtk_widget_get_style_context(app->redo_btn), "sc-tool-btn");
    gtk_widget_set_sensitive(app->redo_btn, FALSE);
    g_signal_connect(app->redo_btn, "clicked", G_CALLBACK(on_redo_clicked), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), app->redo_btn, FALSE, FALSE, 0);

    GtkWidget *clear_btn = gtk_button_new_with_label("Clear");
    gtk_style_context_add_class(gtk_widget_get_style_context(clear_btn), "sc-tool-btn");
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_clear_clicked), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), clear_btn, FALSE, FALSE, 0);

    GtkWidget *copy_btn = gtk_button_new_with_label("Copy");
    gtk_style_context_add_class(gtk_widget_get_style_context(copy_btn), "sc-tool-btn");
    g_signal_connect(copy_btn, "clicked", G_CALLBACK(on_copy_clicked), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), copy_btn, FALSE, FALSE, 0);

    GtkWidget *save_btn = gtk_button_new_with_label("Save");
    gtk_style_context_add_class(gtk_widget_get_style_context(save_btn), "sc-tool-btn");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_clicked), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), save_btn, FALSE, FALSE, 0);

    GtkWidget *open_btn = gtk_button_new_with_label("Open");
    gtk_style_context_add_class(gtk_widget_get_style_context(open_btn), "sc-tool-btn");
    g_signal_connect(open_btn, "clicked", G_CALLBACK(on_open_clicked), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), open_btn, FALSE, FALSE, 0);

    char badge_txt[64];
    snprintf(badge_txt, sizeof(badge_txt), "%s · v%s", proto_upper, SNIPCLIP_VERSION);
    GtkWidget *about_btn = gtk_button_new_with_label(badge_txt);
    gtk_style_context_add_class(gtk_widget_get_style_context(about_btn), "sc-badge-btn");
    gtk_widget_set_tooltip_text(about_btn, "Auto-detected display server & version details");
    g_signal_connect(about_btn, "clicked", G_CALLBACK(on_about_clicked), app);
    gtk_box_pack_start(GTK_BOX(cmd_bar), about_btn, FALSE, FALSE, 0);

    /* Interactive Crop Banner */
    app->crop_banner = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->crop_banner), "sc-crop-bar");
    GtkWidget *crop_lbl = gtk_label_new("Drag on the canvas to adjust the crop region.");
    gtk_box_pack_start(GTK_BOX(app->crop_banner), crop_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(app->crop_banner), gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0), TRUE, TRUE, 0);

    GtkWidget *apply_crop_btn = gtk_button_new_with_label("Apply Crop");
    gtk_style_context_add_class(gtk_widget_get_style_context(apply_crop_btn), "sc-primary-btn");
    g_signal_connect(apply_crop_btn, "clicked", G_CALLBACK(on_apply_crop_clicked), app);
    gtk_box_pack_start(GTK_BOX(app->crop_banner), apply_crop_btn, FALSE, FALSE, 0);

    GtkWidget *cancel_crop_btn = gtk_button_new_with_label("Cancel");
    gtk_style_context_add_class(gtk_widget_get_style_context(cancel_crop_btn), "sc-tool-btn");
    g_signal_connect(cancel_crop_btn, "clicked", G_CALLBACK(on_cancel_crop_clicked), app);
    gtk_box_pack_start(GTK_BOX(app->crop_banner), cancel_crop_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), app->crop_banner, FALSE, FALSE, 0);

    /* Scrollable Editor Canvas */
    app->editor = sc_editor_canvas_new(on_editor_state_changed, app);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), sc_editor_canvas_widget(app->editor));
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    /* Bottom Status Bar */
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_style_context_add_class(gtk_widget_get_style_context(status_bar), "sc-status-bar");
    char sess_summary[512];
    snprintf(
        sess_summary,
        sizeof(sess_summary),
        "Auto-detected: %s (%s) · %s · %s",
        proto_upper,
        app->session.detection_source,
        app->session.desktop_environment,
        app->session.capture_driver
    );
    app->status_session_lbl = gtk_label_new(sess_summary);
    gtk_label_set_ellipsize(GTK_LABEL(app->status_session_lbl), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(status_bar), app->status_session_lbl, FALSE, TRUE, 0);

    app->status_msg_lbl = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(status_bar), app->status_msg_lbl, TRUE, TRUE, 0);

    app->status_dims_lbl = gtk_label_new("No snip loaded");
    gtk_box_pack_end(GTK_BOX(status_bar), app->status_dims_lbl, FALSE, FALSE, 0);

    app->status_zoom_lbl = gtk_label_new("100%");
    gtk_box_pack_end(GTK_BOX(status_bar), app->status_zoom_lbl, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(vbox), status_bar, FALSE, FALSE, 0);

    g_signal_connect(app->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(app->window, "key-press-event", G_CALLBACK(on_main_key_press), app);

    if (initial_image_path && initial_image_path[0] != '\0') {
        GdkPixbuf *loaded = gdk_pixbuf_new_from_file(initial_image_path, NULL);
        if (loaded) {
            sc_editor_set_pixbuf(app->editor, loaded);
            g_object_unref(loaded);
        }
    }

    app_sync_tool_buttons(app);
    gtk_widget_show_all(app->window);
    gtk_widget_set_visible(app->crop_banner, FALSE);
    gtk_window_present(GTK_WINDOW(app->window));

    if (autostart) {
        g_timeout_add(220, on_autostart_snip, app);
    }
}

static void print_help(void) {
    printf(
        "usage: snipclip [options] [image_file]\n\n"
        "Native GTK 3 Windows Snipping Tool clone for Debian-based Linux (X11 & Wayland).\n\n"
        "options:\n"
        "  -h, --help             Show this help message and exit\n"
        "  -v, --version          Print SnipClip version matching the GitHub .deb release\n"
        "  --detect-display       Print auto-detected X11 or Wayland display details as JSON\n"
        "  -m, --mode MODE        Initial snip mode: rectangle, window, fullscreen, freeform\n"
        "  -d, --delay SECONDS    Delay before capture: 0, 3, 5, or 10\n"
        "  --capture-now          Immediately begin capture on launch\n"
        "  --shortcut KEYS        Custom capture shortcut alongside PrintScreen (default: Ctrl+Shift+S)\n"
    );
}

int main(int argc, char **argv) {
    g_set_prgname("snipclip");
    g_set_application_name("SnipClip");

    ScSnipMode init_mode = SC_MODE_RECTANGLE;
    int init_delay = 0;
    bool autostart_snip = false;
    bool want_version = false;
    bool want_help = false;
    bool want_detect_json = false;
    char shortcut_spec[64] = "Ctrl+Shift+S";
    char initial_image[1024] = "";

    /* Resilient parser: supports optional CLI flags while ignoring unknown desktop-launcher flags */
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!arg || !*arg) continue;

        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
            want_version = true;
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            want_help = true;
        } else if (strcmp(arg, "--detect-display") == 0) {
            want_detect_json = true;
        } else if (strcmp(arg, "--capture-now") == 0) {
            autostart_snip = true;
        } else if ((strcmp(arg, "-m") == 0 || strcmp(arg, "--mode") == 0) && i + 1 < argc) {
            sc_snip_mode_from_string(argv[++i], &init_mode);
        } else if (strncmp(arg, "--mode=", 7) == 0) {
            sc_snip_mode_from_string(arg + 7, &init_mode);
        } else if ((strcmp(arg, "-d") == 0 || strcmp(arg, "--delay") == 0) && i + 1 < argc) {
            int d = atoi(argv[++i]);
            if (d == 0 || d == 3 || d == 5 || d == 10) init_delay = d;
        } else if (strncmp(arg, "--delay=", 8) == 0) {
            int d = atoi(arg + 8);
            if (d == 0 || d == 3 || d == 5 || d == 10) init_delay = d;
        } else if (strcmp(arg, "--shortcut") == 0 && i + 1 < argc) {
            snprintf(shortcut_spec, sizeof(shortcut_spec), "%s", argv[++i]);
        } else if (strncmp(arg, "--shortcut=", 11) == 0) {
            snprintf(shortcut_spec, sizeof(shortcut_spec), "%s", arg + 11);
        } else if (arg[0] != '-') {
            if (initial_image[0] == '\0') {
                sc_normalize_launcher_path(arg, initial_image, sizeof(initial_image));
            }
        }
    }

    if (want_help) {
        print_help();
        return 0;
    }
    if (want_version) {
        printf(
            "SnipClip %s (%s, release %s)\n",
            SNIPCLIP_VERSION,
            SNIPCLIP_DEB_FILENAME,
            SNIPCLIP_RELEASE_TAG
        );
        return 0;
    }

    /* Auto-detect Wayland vs. X11 automatically on startup (zero CLI required) */
    ScDisplaySession session = sc_detect_display_session();
    if (want_detect_json) {
        char json_buf[2048];
        sc_format_display_session_json(&session, json_buf, sizeof(json_buf));
        fputs(json_buf, stdout);
        return 0;
    }

    sc_bootstrap_display_env(&session);

    if (!gtk_init_check(&argc, &argv)) {
        fprintf(
            stderr,
            "SnipClip Error: Unable to connect to an active Wayland or X11 display server.\n"
        );
        return 1;
    }

    GdkDisplay *disp = gdk_display_get_default();
    if (disp) {
        sc_reconcile_with_gdk_backend_name(&session, G_OBJECT_TYPE_NAME(disp));
    }

    ScApp app;
    memset(&app, 0, sizeof(app));
    app.session = session;
    app.snip_mode = init_mode;
    app.delay_seconds = init_delay;
    if (!parse_shortcut_accelerator(
            shortcut_spec,
            &app.custom_keyval,
            &app.custom_mods,
            app.custom_shortcut_label,
            sizeof(app.custom_shortcut_label)
        )) {
        parse_shortcut_accelerator(
            "Ctrl+Shift+S",
            &app.custom_keyval,
            &app.custom_mods,
            app.custom_shortcut_label,
            sizeof(app.custom_shortcut_label)
        );
    }

    build_main_window(&app, initial_image, autostart_snip);
    gtk_main();

    sc_editor_canvas_free(app.editor);
    return 0;
}
