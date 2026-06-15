// AC ilib: widgets — GTK3 implementation (libacwidgets.so / acwidgets.dll)
#include "widgets_c.h"
#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

struct DrawCmd {
    enum Type { LINE, RECT, CIRCLE, TEXT } type;
    double x1, y1, x2, y2;
    uint8_t r, g, b;
    std::string text;
};

struct AcWidget {
    enum Kind { SCREEN, DISPLAY, ASK, BTN, CKBTN, DROPDOWN, ADVANCE, SLIDER, GROUP, LISTBOX, SKETCH } kind;
    GtkWidget*   widget    = nullptr;
    GtkWidget*   container = nullptr;  // vbox for SCREEN/GROUP, scrolled for LISTBOX
    GtkListStore* store    = nullptr;  // LISTBOX
    std::vector<std::string> items;    // LISTBOX
    std::vector<DrawCmd>     cmds;     // SKETCH
};

static AcWidget* U(ac_widget_t h) { return reinterpret_cast<AcWidget*>(h); }
static ac_widget_t W(AcWidget* w) { return reinterpret_cast<ac_widget_t>(w); }

static GtkWidget* container_of(ac_widget_t master) {
    AcWidget* m = U(master);
    return m->container ? m->container : m->widget;
}

// ── Sketch draw callback ────────────────────────────────────────────────────
static gboolean on_sketch_draw(GtkWidget*, cairo_t* cr, gpointer data) {
    AcWidget* w = static_cast<AcWidget*>(data);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    for (auto& c : w->cmds) {
        cairo_set_source_rgb(cr, c.r/255.0, c.g/255.0, c.b/255.0);
        switch (c.type) {
            case DrawCmd::LINE:
                cairo_move_to(cr, c.x1, c.y1);
                cairo_line_to(cr, c.x2, c.y2);
                cairo_stroke(cr);
                break;
            case DrawCmd::RECT:
                cairo_rectangle(cr, c.x1, c.y1, c.x2 - c.x1, c.y2 - c.y1);
                cairo_fill(cr);
                break;
            case DrawCmd::CIRCLE:
                cairo_arc(cr, c.x1, c.y1, c.x2, 0, 2 * G_PI);
                cairo_fill(cr);
                break;
            case DrawCmd::TEXT:
                cairo_move_to(cr, c.x1, c.y1);
                cairo_show_text(cr, c.text.c_str());
                break;
        }
    }
    return FALSE;
}

extern "C" {

// ── Init ─────────────────────────────────────────────────────────────────────
void ac_widgets_init(void) {
    gtk_init(NULL, NULL);
}

// ── Screen ────────────────────────────────────────────────────────────────────
ac_widget_t ac_widgets_screen_new(const char* title, const char* geometry) {
    AcWidget* w = new AcWidget{AcWidget::SCREEN};
    w->widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w->widget), title ? title : "AC App");
    int ww = 800, hh = 600;
    if (geometry) sscanf(geometry, "%dx%d", &ww, &hh);
    gtk_window_set_default_size(GTK_WINDOW(w->widget), ww, hh);
    g_signal_connect(w->widget, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
    gtk_container_set_border_width(GTK_CONTAINER(w->widget), 8);
    w->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add(GTK_CONTAINER(w->widget), w->container);
    return W(w);
}
void ac_widgets_screen_mainloop(ac_widget_t h) {
    gtk_widget_show_all(U(h)->widget);
    gtk_main();
}
void ac_widgets_screen_update(ac_widget_t) {
    while (gtk_events_pending()) gtk_main_iteration();
}
void ac_widgets_screen_destroy(ac_widget_t h) {
    gtk_widget_destroy(U(h)->widget);
    delete U(h);
}

// ── display ───────────────────────────────────────────────────────────────────
ac_widget_t ac_widgets_display_new(ac_widget_t master, const char* text) {
    AcWidget* w = new AcWidget{AcWidget::DISPLAY};
    w->widget = gtk_label_new(text ? text : "");
    gtk_widget_set_halign(w->widget, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(container_of(master)), w->widget, FALSE, FALSE, 2);
    return W(w);
}
void        ac_widgets_display_pack(ac_widget_t h) { gtk_widget_show(U(h)->widget); }
void        ac_widgets_display_set(ac_widget_t h, const char* t) { gtk_label_set_text(GTK_LABEL(U(h)->widget), t ? t : ""); }
const char* ac_widgets_display_get(ac_widget_t h) { return gtk_label_get_text(GTK_LABEL(U(h)->widget)); }

// ── ask ───────────────────────────────────────────────────────────────────────
ac_widget_t ac_widgets_ask_new(ac_widget_t master, int width) {
    AcWidget* w = new AcWidget{AcWidget::ASK};
    w->widget = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(w->widget), width > 0 ? width : 20);
    gtk_box_pack_start(GTK_BOX(container_of(master)), w->widget, FALSE, FALSE, 2);
    return W(w);
}
void        ac_widgets_ask_pack(ac_widget_t h) { gtk_widget_show(U(h)->widget); }
const char* ac_widgets_ask_get(ac_widget_t h)  { return gtk_entry_get_text(GTK_ENTRY(U(h)->widget)); }
void        ac_widgets_ask_set(ac_widget_t h, const char* t) { gtk_entry_set_text(GTK_ENTRY(U(h)->widget), t ? t : ""); }

// ── btn ───────────────────────────────────────────────────────────────────────
ac_widget_t ac_widgets_btn_new(ac_widget_t master, const char* text) {
    AcWidget* w = new AcWidget{AcWidget::BTN};
    w->widget = gtk_button_new_with_label(text ? text : "Button");
    gtk_box_pack_start(GTK_BOX(container_of(master)), w->widget, FALSE, FALSE, 2);
    return W(w);
}
void ac_widgets_btn_pack(ac_widget_t h) { gtk_widget_show(U(h)->widget); }
void ac_widgets_btn_on_click(ac_widget_t h, void (*cb)(void*), void* userdata) {
    g_signal_connect(U(h)->widget, "clicked", G_CALLBACK(cb), userdata);
}

// ── ckbtn ─────────────────────────────────────────────────────────────────────
ac_widget_t ac_widgets_ckbtn_new(ac_widget_t master, const char* text) {
    AcWidget* w = new AcWidget{AcWidget::CKBTN};
    w->widget = gtk_check_button_new_with_label(text ? text : "");
    gtk_box_pack_start(GTK_BOX(container_of(master)), w->widget, FALSE, FALSE, 2);
    return W(w);
}
void ac_widgets_ckbtn_pack(ac_widget_t h) { gtk_widget_show(U(h)->widget); }
int  ac_widgets_ckbtn_get(ac_widget_t h)  { return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(U(h)->widget)); }
void ac_widgets_ckbtn_set(ac_widget_t h, int v) { gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(U(h)->widget), v); }

// ── dropdown ──────────────────────────────────────────────────────────────────
ac_widget_t ac_widgets_dropdown_new(ac_widget_t master) {
    AcWidget* w = new AcWidget{AcWidget::DROPDOWN};
    w->widget = gtk_combo_box_text_new();
    gtk_box_pack_start(GTK_BOX(container_of(master)), w->widget, FALSE, FALSE, 2);
    return W(w);
}
void ac_widgets_dropdown_pack(ac_widget_t h) { gtk_widget_show(U(h)->widget); }
void ac_widgets_dropdown_add(ac_widget_t h, const char* item) {
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(U(h)->widget), item ? item : "");
}
const char* ac_widgets_dropdown_get(ac_widget_t h) {
    return gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(U(h)->widget));
}
void ac_widgets_dropdown_set(ac_widget_t h, const char* item) {
    GtkComboBoxText* combo = GTK_COMBO_BOX_TEXT(U(h)->widget);
    GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first(model, &iter)) return;
    do {
        gchar* val = nullptr;
        gtk_tree_model_get(model, &iter, 0, &val, -1);
        if (val && item && g_strcmp0(val, item) == 0) {
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo), &iter);
            g_free(val); return;
        }
        g_free(val);
    } while (gtk_tree_model_iter_next(model, &iter));
}

// ── advance ───────────────────────────────────────────────────────────────────
ac_widget_t ac_widgets_advance_new(ac_widget_t master, int length) {
    AcWidget* w = new AcWidget{AcWidget::ADVANCE};
    w->widget = gtk_progress_bar_new();
    gtk_widget_set_size_request(w->widget, length > 0 ? length : 200, -1);
    gtk_box_pack_start(GTK_BOX(container_of(master)), w->widget, FALSE, FALSE, 2);
    return W(w);
}
void   ac_widgets_advance_pack(ac_widget_t h) { gtk_widget_show(U(h)->widget); }
void   ac_widgets_advance_set(ac_widget_t h, double v) { gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(U(h)->widget), v / 100.0); }
double ac_widgets_advance_get(ac_widget_t h) { return gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(U(h)->widget)) * 100.0; }

// ── slider ────────────────────────────────────────────────────────────────────
ac_widget_t ac_widgets_slider_new(ac_widget_t master, double from_val, double to_val, const char* orient) {
    AcWidget* w = new AcWidget{AcWidget::SLIDER};
    GtkOrientation o = (orient && g_strcmp0(orient, "vertical") == 0)
                     ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
    w->widget = gtk_scale_new_with_range(o, from_val, to_val, 1.0);
    gtk_box_pack_start(GTK_BOX(container_of(master)), w->widget, FALSE, FALSE, 2);
    return W(w);
}
void   ac_widgets_slider_pack(ac_widget_t h) { gtk_widget_show(U(h)->widget); }
double ac_widgets_slider_get(ac_widget_t h)  { return gtk_range_get_value(GTK_RANGE(U(h)->widget)); }
void   ac_widgets_slider_set(ac_widget_t h, double v) { gtk_range_set_value(GTK_RANGE(U(h)->widget), v); }

// ── group ─────────────────────────────────────────────────────────────────────
ac_widget_t ac_widgets_group_new(ac_widget_t master, const char* text) {
    AcWidget* w = new AcWidget{AcWidget::GROUP};
    w->widget    = gtk_frame_new((text && text[0]) ? text : nullptr);
    w->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_add(GTK_CONTAINER(w->widget), w->container);
    gtk_box_pack_start(GTK_BOX(container_of(master)), w->widget, FALSE, FALSE, 4);
    return W(w);
}
void ac_widgets_group_pack(ac_widget_t h) { gtk_widget_show_all(U(h)->widget); }

// ── listbox ───────────────────────────────────────────────────────────────────
ac_widget_t ac_widgets_listbox_new(ac_widget_t master, int width, int height) {
    AcWidget* w = new AcWidget{AcWidget::LISTBOX};
    w->store = gtk_list_store_new(1, G_TYPE_STRING);
    GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(w->store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
    GtkCellRenderer* r = gtk_cell_renderer_text_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(view),
        gtk_tree_view_column_new_with_attributes("", r, "text", 0, nullptr));
    GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_set_size_request(scrolled, width > 0 ? width * 8 : 240, height > 0 ? height * 24 : 120);
    gtk_container_add(GTK_CONTAINER(scrolled), view);
    w->widget = scrolled;
    gtk_box_pack_start(GTK_BOX(container_of(master)), scrolled, FALSE, FALSE, 2);
    return W(w);
}
void        ac_widgets_listbox_pack(ac_widget_t h) { gtk_widget_show_all(U(h)->widget); }
void        ac_widgets_listbox_add(ac_widget_t h, const char* item) {
    GtkTreeIter iter;
    gtk_list_store_append(U(h)->store, &iter);
    gtk_list_store_set(U(h)->store, &iter, 0, item ? item : "", -1);
    U(h)->items.push_back(item ? item : "");
}
const char* ac_widgets_listbox_item(ac_widget_t h, int index) {
    auto& items = U(h)->items;
    if (index < 0 || index >= (int)items.size()) return "";
    return items[index].c_str();
}
int         ac_widgets_listbox_count(ac_widget_t h) { return (int)U(h)->items.size(); }

// ── sketch ────────────────────────────────────────────────────────────────────
ac_widget_t ac_widgets_sketch_new(ac_widget_t master, int width, int height) {
    AcWidget* w = new AcWidget{AcWidget::SKETCH};
    w->widget = gtk_drawing_area_new();
    gtk_widget_set_size_request(w->widget, width > 0 ? width : 400, height > 0 ? height : 300);
    g_signal_connect(w->widget, "draw", G_CALLBACK(on_sketch_draw), w);
    gtk_box_pack_start(GTK_BOX(container_of(master)), w->widget, FALSE, FALSE, 2);
    return W(w);
}
void ac_widgets_sketch_pack(ac_widget_t h) { gtk_widget_show(U(h)->widget); }
void ac_widgets_sketch_clear(ac_widget_t h) {
    U(h)->cmds.clear();
    gtk_widget_queue_draw(U(h)->widget);
}
void ac_widgets_sketch_line(ac_widget_t h, double x1, double y1, double x2, double y2, uint8_t r, uint8_t g, uint8_t b) {
    U(h)->cmds.push_back({DrawCmd::LINE, x1, y1, x2, y2, r, g, b, ""});
    gtk_widget_queue_draw(U(h)->widget);
}
void ac_widgets_sketch_rect(ac_widget_t h, double x1, double y1, double x2, double y2, uint8_t r, uint8_t g, uint8_t b) {
    U(h)->cmds.push_back({DrawCmd::RECT, x1, y1, x2, y2, r, g, b, ""});
    gtk_widget_queue_draw(U(h)->widget);
}
void ac_widgets_sketch_circle(ac_widget_t h, double cx, double cy, double radius, uint8_t r, uint8_t g, uint8_t b) {
    U(h)->cmds.push_back({DrawCmd::CIRCLE, cx, cy, radius, 0, r, g, b, ""});
    gtk_widget_queue_draw(U(h)->widget);
}
void ac_widgets_sketch_text(ac_widget_t h, double x, double y, const char* text, uint8_t r, uint8_t g, uint8_t b) {
    U(h)->cmds.push_back({DrawCmd::TEXT, x, y, 0, 0, r, g, b, text ? text : ""});
    gtk_widget_queue_draw(U(h)->widget);
}

// ── universal helpers (used by Rust FFI and other untyped backends) ──────────
void ac_widgets_pack(ac_widget_t h) {
    gtk_widget_show(U(h)->widget);
}

void ac_widgets_add(ac_widget_t h, const char* item) {
    AcWidget* w = U(h);
    if (w->kind == AcWidget::DROPDOWN) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->widget), item ? item : "");
    } else if (w->kind == AcWidget::LISTBOX) {
        GtkTreeIter iter;
        gtk_list_store_append(w->store, &iter);
        gtk_list_store_set(w->store, &iter, 0, item ? item : "", -1);
        w->items.push_back(item ? item : "");
    }
}

void ac_widgets_set_d(ac_widget_t h, double v) {
    AcWidget* w = U(h);
    if (w->kind == AcWidget::ADVANCE)
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w->widget), v / 100.0);
    else if (w->kind == AcWidget::SLIDER)
        gtk_range_set_value(GTK_RANGE(w->widget), v);
}

// ── lazy / spaced-pack ────────────────────────────────────────────────────────
void ac_widgets_set_lazy(ac_widget_t h) {
    gtk_widget_set_no_show_all(U(h)->widget, TRUE);
}

void ac_widgets_pack_spaced(ac_widget_t h, int sx, int sy) {
    GtkWidget* w = U(h)->widget;
    gtk_widget_set_margin_start(w, sx);
    gtk_widget_set_margin_end(w, sx);
    gtk_widget_set_margin_top(w, sy);
    gtk_widget_set_margin_bottom(w, sy);
    gtk_widget_set_no_show_all(w, FALSE);
    gtk_widget_show(w);
}

} // extern "C"
