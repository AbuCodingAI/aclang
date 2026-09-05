// AC ilib: widgets — C++ thin wrappers over widgets_c.h (libacwidgets.so)
// Include: #include "library/ilib/widgets/widgets.hpp"
// Link:    -L./library/ilib/widgets -lacwidgets
#pragma once
#include "widgets_c.h"
#include <string>
#include <vector>
#include <functional>
#include <cstring>
#include <type_traits>

inline void _ac_widgets_init_once() {
    static bool _done = false;
    if (!_done) { ac_widgets_init(); _done = true; }
}

static constexpr const char* lazy = "lazy";

inline void _ac_auto_or_lazy(ac_widget_t h, void(*packFn)(ac_widget_t), const char* lz) {
    if (lz && std::strcmp(lz, "lazy") == 0) ac_widgets_set_lazy(h);
    else if (packFn) packFn(h);
}
inline void _ac_pack_or_spaced(ac_widget_t h, void(*packFn)(ac_widget_t), int sx, int sy) {
    if (sx || sy) ac_widgets_pack_spaced(h, sx, sy);
    else if (packFn) packFn(h);
}

// Common base for anything that can act as a widget's parent container — Screen,
// group, and a tabs page all qualify. Widget constructors take this instead of
// `Screen&` specifically so `display(some_tab_page, "hi")` and
// `btn(some_group, "Go")` compile, not just `display(screen, "hi")`.
struct _AcMasterHandle { ac_widget_t _h; };

struct Screen : _AcMasterHandle {
    // title is mandatory — no default, no geometry positional arg. Use .dimensions(w, h)
    // to size the window explicitly.
    Screen(const std::string& title) {
        _ac_widgets_init_once();
        _h = ac_widgets_screen_new(title.c_str());
    }
    void mainloop() { ac_widgets_screen_mainloop(_h); }
    void update()   { ac_widgets_screen_update(_h); }
    void destroy()  { ac_widgets_screen_destroy(_h); }
    void dimensions(int w, int h) { ac_widgets_screen_dimensions(_h, w, h); }
};

struct display {
    ac_widget_t _h;
    display(_AcMasterHandle& master, const std::string& text = "", const char* lz = nullptr) {
        _h = ac_widgets_display_new(master._h, text.c_str());
        _ac_auto_or_lazy(_h, ac_widgets_display_pack, lz);
    }
    void pack(int sx=0, int sy=0)  { _ac_pack_or_spaced(_h, ac_widgets_display_pack, sx, sy); }
    void set(const std::string& v) { ac_widgets_display_set(_h, v.c_str()); }
    std::string get() const        { const char* p = ac_widgets_display_get(_h); return p ? p : ""; }
    void config(const std::string& text) { set(text); }
};

struct ask {
    ac_widget_t _h;
    ask(_AcMasterHandle& master, int width = 20, const char* lz = nullptr) {
        _h = ac_widgets_ask_new(master._h, width);
        _ac_auto_or_lazy(_h, ac_widgets_ask_pack, lz);
    }
    void pack(int sx=0, int sy=0)  { _ac_pack_or_spaced(_h, ac_widgets_ask_pack, sx, sy); }
    std::string get() const        { const char* p = ac_widgets_ask_get(_h); return p ? p : ""; }
    void set(const std::string& v) { ac_widgets_ask_set(_h, v.c_str()); }
};

struct btn {
    ac_widget_t _h;
    std::function<void()>* _cmd = nullptr;
    btn(_AcMasterHandle& master, const std::string& text = "Button", const char* lz = nullptr) {
        _h = ac_widgets_btn_new(master._h, text.c_str());
        _ac_auto_or_lazy(_h, ac_widgets_btn_pack, lz);
    }
    void pack(int sx=0, int sy=0) { _ac_pack_or_spaced(_h, ac_widgets_btn_pack, sx, sy); }
    // Templated so it accepts whatever shape the AC compiler actually generated for the
    // callback function — every AC top-level `Make Name func(...)` compiles to a fixed
    // `long long Name(...)` (this codebase's uniform int-everything convention), with arity
    // matching the AC source's own parameter count: `func()` -> `long long Name()`, `func(arg)`
    // -> `long long Name(long long)`. Different examples use both shapes (applicant_form.ac's
    // `OnSubmit func()` vs widgets_test.ac's `OnClick func(arg)`), but
    // `ac_widgets_btn_on_click` needs one fixed `std::function<void()>` — a raw function
    // pointer of either AC shape has NO implicit conversion to that (verified: hard compile
    // error, "no known conversion ... to std::function<void()>"). `if constexpr` picks the
    // right adapter at compile time per callback, discarding the dummy 0 argument / return
    // value either way (AC button click handlers don't consume either).
    template<typename F>
    btn(_AcMasterHandle& master, const std::string& text, F cmd, const char* lz = nullptr) {
        _h = ac_widgets_btn_new(master._h, text.c_str());
        on_click(cmd);
        _ac_auto_or_lazy(_h, ac_widgets_btn_pack, lz);
    }
    template<typename F>
    void on_click(F cb) {
        std::function<void()> wrapped;
        if constexpr (std::is_invocable_v<F>) wrapped = [cb]() { cb(); };
        else                                  wrapped = [cb]() { cb(0); };
        _cmd = new std::function<void()>(std::move(wrapped));
        ac_widgets_btn_on_click(_h,
            [](void* d){ (*static_cast<std::function<void()>*>(d))(); },
            _cmd);
    }
};

struct ckbtn {
    ac_widget_t _h;
    ckbtn(_AcMasterHandle& master, const std::string& text = "", const char* lz = nullptr) {
        _h = ac_widgets_ckbtn_new(master._h, text.c_str());
        _ac_auto_or_lazy(_h, ac_widgets_ckbtn_pack, lz);
    }
    void pack(int sx=0, int sy=0) { _ac_pack_or_spaced(_h, ac_widgets_ckbtn_pack, sx, sy); }
    bool get() const   { return ac_widgets_ckbtn_get(_h) != 0; }
    void set(bool v)   { ac_widgets_ckbtn_set(_h, v ? 1 : 0); }
};

struct radbtn {
    ac_widget_t _h;
    radbtn(_AcMasterHandle& master, const std::string& text = "", const char* lz = nullptr) {
        _h = ac_widgets_ckbtn_new(master._h, text.c_str());
        _ac_auto_or_lazy(_h, ac_widgets_ckbtn_pack, lz);
    }
    void pack(int sx=0, int sy=0) { _ac_pack_or_spaced(_h, ac_widgets_ckbtn_pack, sx, sy); }
    bool get() const { return ac_widgets_ckbtn_get(_h) != 0; }
};

struct dropdown {
    ac_widget_t _h;
    dropdown(_AcMasterHandle& master, const std::vector<std::string>& values = {}, const char* lz = nullptr) {
        _h = ac_widgets_dropdown_new(master._h);
        for (const auto& v : values) ac_widgets_dropdown_add(_h, v.c_str());
        _ac_auto_or_lazy(_h, ac_widgets_dropdown_pack, lz);
    }
    // AC's own calling convention never populates `values` at construction time (items are always
    // appended one at a time via `.add()` afterward — see every widgets example) — a call site like
    // `dropdown(root, lazy)` passes a bare `const char*` positionally into the SECOND slot, which
    // is `values` (a `vector<string>`) in the constructor above, with no implicit conversion from
    // `const char*` — a hard compile error (verified: `examples/applicant_form.ac`/`widgets_test*.ac`
    // all failed to build on C++ this way). This overload gives `(master, lz)` its own exact-type
    // match so overload resolution picks it directly instead of trying (and failing) to convert.
    dropdown(_AcMasterHandle& master, const char* lz) {
        _h = ac_widgets_dropdown_new(master._h);
        _ac_auto_or_lazy(_h, ac_widgets_dropdown_pack, lz);
    }
    void pack(int sx=0, int sy=0)  { _ac_pack_or_spaced(_h, ac_widgets_dropdown_pack, sx, sy); }
    void add(const std::string& v) { ac_widgets_dropdown_add(_h, v.c_str()); }
    std::string get() const        { const char* p = ac_widgets_dropdown_get(_h); return p ? p : ""; }
    void set(const std::string& v) { ac_widgets_dropdown_set(_h, v.c_str()); }
};

struct advance {
    ac_widget_t _h;
    advance(_AcMasterHandle& master, int length = 200, const char* lz = nullptr) {
        _h = ac_widgets_advance_new(master._h, length);
        _ac_auto_or_lazy(_h, ac_widgets_advance_pack, lz);
    }
    void pack(int sx=0, int sy=0) { _ac_pack_or_spaced(_h, ac_widgets_advance_pack, sx, sy); }
    void set(double v)   { ac_widgets_advance_set(_h, v); }
    double get() const   { return ac_widgets_advance_get(_h); }
};

struct slider {
    ac_widget_t _h;
    slider(_AcMasterHandle& master, double from_val = 0, double to_val = 100, const std::string& orient = "horizontal", const char* lz = nullptr) {
        _h = ac_widgets_slider_new(master._h, from_val, to_val, orient.c_str());
        _ac_auto_or_lazy(_h, ac_widgets_slider_pack, lz);
    }
    void pack(int sx=0, int sy=0) { _ac_pack_or_spaced(_h, ac_widgets_slider_pack, sx, sy); }
    double get() const   { return ac_widgets_slider_get(_h); }
    void set(double v)   { ac_widgets_slider_set(_h, v); }
};

struct group : _AcMasterHandle {
    group(_AcMasterHandle& master, const std::string& text = "") {
        _h = ac_widgets_group_new(master._h, text.c_str());
    }
    void pack() { ac_widgets_group_pack(_h); }
};

// A tab page is itself a usable "master" — mirrors `group`'s shape so any other
// widget constructor (display, btn, ask, ...) can be parented into a specific tab.
struct tabpage : _AcMasterHandle {
    explicit tabpage(ac_widget_t h) { _h = h; }
};

struct tabs {
    ac_widget_t _h;
    tabs(_AcMasterHandle& master) { _h = ac_widgets_tabs_new(master._h); }
    void pack() { ac_widgets_tabs_pack(_h); }
    tabpage add_tab(const std::string& name) { return tabpage(ac_widgets_tabs_add_tab(_h, name.c_str())); }
};

struct scroller {
    ac_widget_t _h;
    scroller(_AcMasterHandle& master, const std::string& orient = "vertical") {
        _h = ac_widgets_scroller_new(master._h, orient.c_str());
    }
    void pack() { ac_widgets_scroller_pack(_h); }
};

struct listbox {
    ac_widget_t _h;
    listbox(_AcMasterHandle& master, int width = 30, int height = 5) {
        _h = ac_widgets_listbox_new(master._h, width, height);
    }
    void pack()                    { ac_widgets_listbox_pack(_h); }
    void add(const std::string& s) { ac_widgets_listbox_add(_h, s.c_str()); }
    std::vector<std::string> get() const {
        int n = ac_widgets_listbox_count(_h);
        std::vector<std::string> out;
        out.reserve(n);
        for (int i = 0; i < n; i++) {
            const char* p = ac_widgets_listbox_item(_h, i);
            out.emplace_back(p ? p : "");
        }
        return out;
    }
};

// Real multi-column table (GtkTreeView with visible headers) — `columns` is a
// comma-separated header list ("Name,Age"), `add(row)` a comma-separated value
// list ("Alice,30"). No quoting/escaping: a cell value with a literal comma
// isn't supported (documented limitation, matches widgets_c.h).
struct table {
    ac_widget_t _h;
    table(_AcMasterHandle& master, const std::string& columns = "", int height = 10) {
        _h = ac_widgets_table_new(master._h, columns.c_str(), height);
    }
    void pack()                      { ac_widgets_table_pack(_h); }
    void add(const std::string& row) { ac_widgets_table_add(_h, row.c_str()); }
    std::vector<std::string> get() const {
        int n = ac_widgets_table_count(_h);
        std::vector<std::string> out;
        out.reserve(n);
        for (int i = 0; i < n; i++) {
            const char* p = ac_widgets_table_row(_h, i);
            out.emplace_back(p ? p : "");
        }
        return out;
    }
};

struct sketch {
    ac_widget_t _h;
    sketch(_AcMasterHandle& master, int width = 400, int height = 300) {
        _h = ac_widgets_sketch_new(master._h, width, height);
    }
    void pack()                                            { ac_widgets_sketch_pack(_h); }
    void clear()                                           { ac_widgets_sketch_clear(_h); }
    void line(double x1, double y1, double x2, double y2, uint8_t r=0, uint8_t g=0, uint8_t b=0) {
        ac_widgets_sketch_line(_h, x1, y1, x2, y2, r, g, b);
    }
    void rect(double x1, double y1, double x2, double y2, uint8_t r=0, uint8_t g=0, uint8_t b=0) {
        ac_widgets_sketch_rect(_h, x1, y1, x2, y2, r, g, b);
    }
    void circle(double cx, double cy, double rad, uint8_t r=0, uint8_t g=0, uint8_t b=0) {
        ac_widgets_sketch_circle(_h, cx, cy, rad, r, g, b);
    }
    void text_at(double x, double y, const std::string& t, uint8_t r=0, uint8_t g=0, uint8_t b=0) {
        ac_widgets_sketch_text(_h, x, y, t.c_str(), r, g, b);
    }
};

struct textbox {
    ac_widget_t _h;
    textbox(_AcMasterHandle& master, const std::string& color = "black", const std::string& font = "monospace", const char* lz = nullptr) {
        _h = ac_widgets_textbox_new(master._h, color.c_str(), font.c_str());
        _ac_auto_or_lazy(_h, ac_widgets_textbox_pack, lz);
    }
    void pack()                     { ac_widgets_textbox_pack(_h); }
    void write(const std::string& s){ ac_widgets_textbox_write(_h, s.c_str()); }
    std::string get() const         { const char* p = ac_widgets_textbox_get(_h); return p ? p : ""; }
    std::string find(const std::string& needle) const {
        const char* p = ac_widgets_textbox_find(_h, needle.c_str());
        return p ? p : "";
    }
    void fix(const std::string& s)  { ac_widgets_textbox_fix(_h, s.c_str()); }
};
