#ifndef SNIPCLIP_EDITOR_CANVAS_H
#define SNIPCLIP_EDITOR_CANVAS_H

#include <gtk/gtk.h>
#include <stdbool.h>

#include "geometry.h"

/* Maximum number of retained undo / redo snapshots (oldest evicted first) */
#define SC_MAX_HISTORY 40

typedef struct ScEditorCanvas ScEditorCanvas;

typedef void (*ScEditorStateCb)(void *user_data);

ScEditorCanvas *sc_editor_canvas_new(ScEditorStateCb on_state_changed, void *user_data);
void sc_editor_canvas_free(ScEditorCanvas *ed);

GtkWidget *sc_editor_canvas_widget(ScEditorCanvas *ed);

void sc_editor_set_pixbuf(ScEditorCanvas *ed, GdkPixbuf *pixbuf);
void sc_editor_reset_to_empty(ScEditorCanvas *ed);
bool sc_editor_has_image(const ScEditorCanvas *ed);
void sc_editor_get_dimensions(const ScEditorCanvas *ed, int *out_w, int *out_h);

void sc_editor_set_tool(ScEditorCanvas *ed, ScToolKind tool);
ScToolKind sc_editor_get_tool(const ScEditorCanvas *ed);

void sc_editor_set_color_rgba(ScEditorCanvas *ed, const GdkRGBA *rgba);
void sc_editor_get_color_rgba(const ScEditorCanvas *ed, GdkRGBA *out_rgba);

void sc_editor_set_stroke_width(ScEditorCanvas *ed, double width);
double sc_editor_get_stroke_width(const ScEditorCanvas *ed);

void sc_editor_set_ruler_visible(ScEditorCanvas *ed, bool visible);
bool sc_editor_get_ruler_visible(const ScEditorCanvas *ed);

void sc_editor_set_zoom(ScEditorCanvas *ed, double zoom);
double sc_editor_get_zoom(const ScEditorCanvas *ed);

bool sc_editor_can_undo(const ScEditorCanvas *ed);
bool sc_editor_can_redo(const ScEditorCanvas *ed);
void sc_editor_undo(ScEditorCanvas *ed);
void sc_editor_redo(ScEditorCanvas *ed);
void sc_editor_clear_ink(ScEditorCanvas *ed);

bool sc_editor_is_crop_active(const ScEditorCanvas *ed);
void sc_editor_apply_crop(ScEditorCanvas *ed);
void sc_editor_cancel_crop(ScEditorCanvas *ed);

/* Flatten base image and all annotations into a new exportable GdkPixbuf (caller unrefs) */
GdkPixbuf *sc_editor_render_pixbuf(const ScEditorCanvas *ed);

#endif /* SNIPCLIP_EDITOR_CANVAS_H */
