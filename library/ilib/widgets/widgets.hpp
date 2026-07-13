// AC ilib: widgets — C++ thin wrappers over widgets_c.h (libacwidgets.so)
// Include: #include "library/ilib/widgets/widgets.hpp"
// Link:    -L./library/ilib/widgets -lacwidgets
#pragma once
#include "widgets_c.h"
#include <string>
#include <vector>
#include <functional>
#include <cstring>

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

struct Screen {
    ac_widget_t _h;
    Screen(const std::string& title = "AC App", const std::string& geometry = "800x600") {
        _ac_widgets_init_once();
        _h = ac_widgets_screen_new(title.c_str(), geometry.c_str());
    }
    void mainloop() { ac_widgets_screen_mainloop(_h); }
    void update()   { ac_widgets_screen_update(_h); }
    void destroy()  { ac_widgets_screen_destroy(_h); }
};

struct display {
    ac_widget_t _h;
    display(Screen& master, const std::string& text = "", const char* lz = nullptr) {
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
    ask(Screen& master, int width = 20, const char* lz = nullptr) {
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
    btn(Screen& master, const std::string& text = "Button", std::function<void()> cmd = nullptr, const char* lz = nullptr) {
        _h = ac_widgets_btn_new(master._h, text.c_str());
        if (cmd) on_click(cmd);
        _ac_auto_or_lazy(_h, ac_widgets_btn_pack, lz);
    }
    void pack(int sx=0, int sy=0) { _ac_pack_or_spaced(_h, ac_widgets_btn_pack, sx, sy); }
    void on_click(std::function<void()> cb) {
        _cmd = new std::function<void()>(cb);
        ac_widgets_btn_on_click(_h,
            [](void* d){ (*static_cast<std::function<void()>*>(d))(); },
            _cmd);
    }
};

struct ckbtn {
    ac_widget_t _h;
    ckbtn(Screen& master, const std::string& text = "", const char* lz = nullptr) {
        _h = ac_widgets_ckbtn_new(master._h, text.c_str());
        _ac_auto_or_lazy(_h, ac_widgets_ckbtn_pack, lz);
    }
    void pack(int sx=0, int sy=0) { _ac_pack_or_spaced(_h, ac_widgets_ckbtn_pack, sx, sy); }
    bool get() const   { return ac_widgets_ckbtn_get(_h) != 0; }
    void set(bool v)   { ac_widgets_ckbtn_set(_h, v ? 1 : 0); }
};

struct radbtn {
    ac_widget_t _h;
    radbtn(Screen& master, const std::string& text = "", const char* lz = nullptr) {
        _h = ac_widgets_ckbtn_new(master._h, text.c_str());
        _ac_auto_or_lazy(_h, ac_widgets_ckbtn_pack, lz);
    }
    void pack(int sx=0, int sy=0) { _ac_pack_or_spaced(_h, ac_widgets_ckbtn_pack, sx, sy); }
    bool get() const { return ac_widgets_ckbtn_get(_h) != 0; }
};

struct dropdown {
    ac_widget_t _h;
    dropdown(Screen& master, const std::vector<std::string>& values = {}, const char* lz = nullptr) {
        _h = ac_widgets_dropdown_new(master._h);
        for (const auto& v : values) ac_widgets_dropdown_add(_h, v.c_str());
        _ac_auto_or_lazy(_h, ac_widgets_dropdown_pack, lz);
    }
    void pack(int sx=0, int sy=0)  { _ac_pack_or_spaced(_h, ac_widgets_dropdown_pack, sx, sy); }
    void add(const std::string& v) { ac_widgets_dropdown_add(_h, v.c_str()); }
    std::string get() const        { const char* p = ac_widgets_dropdown_get(_h); return p ? p : ""; }
    void set(const std::string& v) { ac_widgets_dropdown_set(_h, v.c_str()); }
};

struct advance {
    ac_widget_t _h;
    advance(Screen& master, int length = 200, const char* lz = nullptr) {
        _h = ac_widgets_advance_new(master._h, length);
        _ac_auto_or_lazy(_h, ac_widgets_advance_pack, lz);
    }
    void pack(int sx=0, int sy=0) { _ac_pack_or_spaced(_h, ac_widgets_advance_pack, sx, sy); }
    void set(double v)   { ac_widgets_advance_set(_h, v); }
    double get() const   { return ac_widgets_advance_get(_h); }
};

struct slider {
    ac_widget_t _h;
    slider(Screen& master, double from_val = 0, double to_val = 100, const std::string& orient = "horizontal", const char* lz = nullptr) {
        _h = ac_widgets_slider_new(master._h, from_val, to_val, orient.c_str());
        _ac_auto_or_lazy(_h, ac_widgets_slider_pack, lz);
    }
    void pack(int sx=0, int sy=0) { _ac_pack_or_spaced(_h, ac_widgets_slider_pack, sx, sy); }
    double get() const   { return ac_widgets_slider_get(_h); }
    void set(double v)   { ac_widgets_slider_set(_h, v); }
};

struct group {
    ac_widget_t _h;
    group(Screen& master, const std::string& text = "") {
        _h = ac_widgets_group_new(master._h, text.c_str());
    }
    void pack() { ac_widgets_group_pack(_h); }
};

struct tabs {
    ac_widget_t _h;
    tabs(Screen& master) { _h = ac_widgets_group_new(master._h, ""); }
    void pack()           { ac_widgets_group_pack(_h); }
    tabs& add_tab(const std::string&) { return *this; }
};

struct scroller {
    ac_widget_t _h;
    scroller(Screen& master) { _h = ac_widgets_group_new(master._h, ""); }
    void pack() { ac_widgets_group_pack(_h); }
};

struct listbox {
    ac_widget_t _h;
    listbox(Screen& master, int width = 30, int height = 5) {
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

struct table {
    ac_widget_t _h;
    table(Screen& master) { _h = ac_widgets_listbox_new(master._h, 40, 10); }
    void pack()                    { ac_widgets_listbox_pack(_h); }
    void add(const std::string& s) { ac_widgets_listbox_add(_h, s.c_str()); }
};

struct sketch {
    ac_widget_t _h;
    sketch(Screen& master, int width = 400, int height = 300) {
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
