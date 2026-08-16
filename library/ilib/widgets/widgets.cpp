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

// Split a comma-separated string. No quoting/escaping — matches the documented
// limitation in widgets_c.h (a cell value with a literal comma isn't supported).
static std::vector<std::string> splitCsv(const char* s) {
    std::vector<std::string> out;
    if (!s) return out;
    std::string cur;
    for (const char* p = s; *p; p++) {
        if (*p == ',') { out.push_back(cur); cur.clear(); }
        else cur += *p;
    }
    out.push_back(cur);
    return out;
}
static std::string joinCsv(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); i++) { if (i) out += ","; out += v[i]; }
    return out;
}

struct AcWidget {
    enum Kind { SCREEN, DISPLAY, ASK, BTN, CKBTN, DROPDOWN, ADVANCE, SLIDER, GROUP, LISTBOX,
                SKETCH, TABS, TABPAGE, SCROLLER, TABLE, TEXTBOX } kind;
    GtkWidget*   widget    = nullptr;
    GtkWidget*   container = nullptr;  // vbox for SCREEN/GROUP/TABPAGE, scrolled for LISTBOX
    GtkListStore* store    = nullptr;  // LISTBOX / TABLE
    std::vector<std::string> items;    // LISTBOX
    std::vector<DrawCmd>     cmds;     // SKETCH
    int columnCount = 0;               // TABLE
    std::vector<std::string> rows;     // TABLE — each row stored CSV-joined, for .row()/.count()
    GtkTextBuffer* textBuffer = nullptr; // TEXTBOX
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
// `root.dimensions(400, 300)` — explicit int-pair sizing, an alternative to the ctor's
// "WxH" geometry string for callers who'd rather not build/parse that string themselves.
void ac_widgets_screen_dimensions(ac_widget_t h, int w, int hh) {
    gtk_window_set_default_size(GTK_WINDOW(U(h)->widget), w > 0 ? w : 800, hh > 0 ? hh : 600);
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
struct AcWidgetsClickData { void (*cb)(void*); void* userdata; };
static void ac_widgets_btn_trampoline(GtkWidget*, gpointer data) {
    AcWidgetsClickData* cd = static_cast<AcWidgetsClickData*>(data);
    if (cd && cd->cb) cd->cb(cd->userdata);
}
void ac_widgets_btn_on_click(ac_widget_t h, void (*cb)(void*), void* userdata) {
    AcWidgetsClickData* cd = new AcWidgetsClickData{cb, userdata};
    g_signal_connect(U(h)->widget, "clicked", G_CALLBACK(ac_widgets_btn_trampoline), cd);
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
    // gtk_combo_box_text_get_active_text returns a g_malloc'd string the caller must g_free
    // (and NULL when nothing is selected). Copy into a persistent buffer and free it → no leak.
    static thread_local std::string buf;
    char* s = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(U(h)->widget));
    buf = s ? s : "";
    if (s) g_free(s);
    return buf.c_str();
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

// ── tabs (real GtkNotebook) ───────────────────────────────────────────────────
ac_widget_t ac_widgets_tabs_new(ac_widget_t master) {
    AcWidget* w = new AcWidget{AcWidget::TABS};
    w->widget = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(container_of(master)), w->widget, TRUE, TRUE, 2);
    return W(w);
}
void ac_widgets_tabs_pack(ac_widget_t h) { gtk_widget_show(U(h)->widget); }
// Returns a new TABPAGE handle usable as `master` by any other widget constructor
// (same shape as `group`) — so `page = t.add_tab("One"); display(page, "hi")` works.
ac_widget_t ac_widgets_tabs_add_tab(ac_widget_t h, const char* name) {
    AcWidget* page = new AcWidget{AcWidget::TABPAGE};
    page->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* label = gtk_label_new(name ? name : "");
    gtk_notebook_append_page(GTK_NOTEBOOK(U(h)->widget), page->container, label);
    gtk_widget_show_all(page->container);
    gtk_widget_show(label);
    return W(page);
}

// ── scroller (real GtkScrollbar — matches the ttk.Scrollbar reference: a bare
// standalone scrollbar, not a scrollable container) ───────────────────────────
ac_widget_t ac_widgets_scroller_new(ac_widget_t master, const char* orient) {
    AcWidget* w = new AcWidget{AcWidget::SCROLLER};
    GtkOrientation o = (orient && g_strcmp0(orient, "vertical") == 0)
                     ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;
    GtkAdjustment* adj = gtk_adjustment_new(0, 0, 100, 1, 10, 10);
    w->widget = gtk_scrollbar_new(o, adj);
    gtk_box_pack_start(GTK_BOX(container_of(master)), w->widget, FALSE, FALSE, 2);
    return W(w);
}
void ac_widgets_scroller_pack(ac_widget_t h) { gtk_widget_show(U(h)->widget); }

// ── table (real multi-column GtkTreeView, headers visible) ────────────────────
ac_widget_t ac_widgets_table_new(ac_widget_t master, const char* columns_csv, int height) {
    AcWidget* w = new AcWidget{AcWidget::TABLE};
    std::vector<std::string> cols = splitCsv(columns_csv);
    if (cols.empty()) cols.push_back("");
    w->columnCount = (int)cols.size();
    std::vector<GType> types(cols.size(), G_TYPE_STRING);
    w->store = gtk_list_store_newv((gint)types.size(), types.data());
    GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(w->store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
    for (size_t c = 0; c < cols.size(); c++) {
        GtkCellRenderer* r = gtk_cell_renderer_text_new();
        gtk_tree_view_append_column(GTK_TREE_VIEW(view),
            gtk_tree_view_column_new_with_attributes(cols[c].c_str(), r, "text", (gint)c, nullptr));
    }
    GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_set_size_request(scrolled, cols.size() * 100, height > 0 ? height * 24 : 200);
    gtk_container_add(GTK_CONTAINER(scrolled), view);
    w->widget = scrolled;
    gtk_box_pack_start(GTK_BOX(container_of(master)), scrolled, TRUE, TRUE, 2);
    return W(w);
}
void ac_widgets_table_pack(ac_widget_t h) { gtk_widget_show_all(U(h)->widget); }
void ac_widgets_table_add(ac_widget_t h, const char* values_csv) {
    AcWidget* w = U(h);
    std::vector<std::string> vals = splitCsv(values_csv);
    vals.resize(w->columnCount);   // pad/truncate to the declared column count
    GtkTreeIter iter;
    gtk_list_store_append(w->store, &iter);
    for (int c = 0; c < w->columnCount; c++)
        gtk_list_store_set(w->store, &iter, c, vals[c].c_str(), -1);
    w->rows.push_back(joinCsv(vals));
}
const char* ac_widgets_table_row(ac_widget_t h, int index) {
    auto& rows = U(h)->rows;
    if (index < 0 || index >= (int)rows.size()) return "";
    return rows[index].c_str();
}
int ac_widgets_table_count(ac_widget_t h) { return (int)U(h)->rows.size(); }

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

// Tab inserts 4 spaces (code-editor convention) instead of GTK's default literal tab
// character; Enter already inserts a newline with zero extra work (GtkTextView's own
// default behavior), so this is the only key that needs intercepting.
static gboolean ac_textbox_on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer) {
    if (event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_ISO_Left_Tab) {
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
        gtk_text_buffer_insert_at_cursor(buf, "    ", 4);
        return TRUE; // handled — suppress the default tab-character insertion
    }
    return FALSE;
}

// ── textbox (plain multi-line text editor — no syntax highlighting, matches the
//    Python-IDLE-style scope this was built for, not a code-editor widget) ────
ac_widget_t ac_widgets_textbox_new(ac_widget_t master, const char* color, const char* font) {
    AcWidget* w = new AcWidget{AcWidget::TEXTBOX};
    GtkWidget* view = gtk_text_view_new();
    w->textBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    w->container = view; // the real GtkTextView — needed by .fix() to toggle editability
    g_signal_connect(view, "key-press-event", G_CALLBACK(ac_textbox_on_key_press), nullptr);
    GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_container_add(GTK_CONTAINER(scrolled), view);
    // Color + font via a scoped CSS provider — the non-deprecated GTK3 styling path (avoids
    // gtk_widget_override_font/override_color, deprecated since 3.16). Targets the buffer's
    // actual text run (`textview text`), not just the widget's surrounding chrome.
    std::string css = "textview text { color: " + std::string(color && *color ? color : "black")
                     + "; font-family: '" + std::string(font && *font ? font : "monospace") + "'; }";
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css.c_str(), -1, nullptr);
    gtk_style_context_add_provider(gtk_widget_get_style_context(view),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    w->widget = scrolled;
    // Unlike most widgets here, a text box should expand/fill available space (TRUE, TRUE) —
    // an editor pinned to its minimum size defeats the point.
    gtk_box_pack_start(GTK_BOX(container_of(master)), scrolled, TRUE, TRUE, 2);
    return W(w);
}
void ac_widgets_textbox_pack(ac_widget_t h) { gtk_widget_show_all(U(h)->widget); }
void ac_widgets_textbox_write(ac_widget_t h, const char* text) {
    gtk_text_buffer_set_text(U(h)->textBuffer, text ? text : "", -1);
}
const char* ac_widgets_textbox_get(ac_widget_t h) {
    // gtk_text_buffer_get_text returns a g_malloc'd string the caller must g_free — same
    // persistent-buffer-then-free pattern as ac_widgets_dropdown_get above.
    static thread_local std::string buf;
    GtkTextBuffer* tb = U(h)->textBuffer;
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(tb, &start, &end);
    char* s = gtk_text_buffer_get_text(tb, &start, &end, FALSE);
    buf = s ? s : "";
    if (s) g_free(s);
    return buf.c_str();
}
// find($needle$) → the matched text itself if `needle` occurs anywhere in the box's current
// content, else an empty string — "find a string" in the literal sense of returning one.
const char* ac_widgets_textbox_find(ac_widget_t h, const char* needle) {
    static thread_local std::string buf;
    buf.clear();
    if (needle && *needle) {
        GtkTextBuffer* tb = U(h)->textBuffer;
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(tb, &start, &end);
        char* full = gtk_text_buffer_get_text(tb, &start, &end, FALSE);
        if (full && strstr(full, needle)) buf = needle;
        if (full) g_free(full);
    }
    return buf.c_str();
}
// fix($text$) — writes `text` as the box's content and locks it read-only: "fixes it so it
// cannot be edited". Composes with find: `tb.fix(tb.find($x$))` sets the box to the matched
// text (or clears it, if not found) and finalizes it in one step.
void ac_widgets_textbox_fix(ac_widget_t h, const char* text) {
    AcWidget* w = U(h);
    gtk_text_buffer_set_text(w->textBuffer, text ? text : "", -1);
    if (w->container) {
        gtk_text_view_set_editable(GTK_TEXT_VIEW(w->container), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(w->container), FALSE);
    }
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
    } else if (w->kind == AcWidget::TABLE) {
        // Untyped-handle backends (Rust/Go/V/Java's i64-handle FFI) can't dispatch `.add()` on
        // widget KIND at compile time the way CStrategy's cWidgetMethod does (which already
        // special-cases table -> ac_widgets_table_add directly) — this generic entry point needs
        // its own TABLE branch so those backends' `grid.add("Alice,30")` isn't a silent no-op.
        ac_widgets_table_add(h, item);
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
