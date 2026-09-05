// AC ilib: widgets — Java Panama FFI (libacwidgets.so / acwidgets.dll)
// Inlined by AC->Java compiler when "use ilib widgets" is declared
// Requires: Java 22+, --enable-preview, java.lang.foreign module
import java.lang.foreign.*;
import java.lang.invoke.*;
import java.nio.file.*;
import java.util.*;

final class AcWidgets {
    private static final Linker     _L   = Linker.nativeLinker();
    private static final SymbolLookup _SYM;
    // AC_PATH-aware resolution — see camera_ffi.java's _resolveLib for why the plain
    // user.dir-relative path broke whenever `ac` ran from anywhere but the project root.
    private static Path _resolveLib(String rel) {
        Path relP = Path.of("library", "ilib", "widgets", rel);
        String acp = System.getenv("AC_PATH");
        if (acp != null) {
            Path cand = Path.of(acp).resolve(relP);
            if (Files.exists(cand)) return cand.toAbsolutePath();
        }
        Path cwdCand = Path.of(".").resolve(relP);
        if (Files.exists(cwdCand)) return cwdCand.toAbsolutePath();
        try {
            Path self = Path.of(AcWidgets.class.getProtectionDomain().getCodeSource().getLocation().toURI());
            Path base = Files.isDirectory(self) ? self : self.getParent();
            Path fromClass = base.resolve("..").resolve(relP).normalize();
            if (Files.exists(fromClass)) return fromClass.toAbsolutePath();
        } catch (Exception ignored) {}
        return cwdCand.toAbsolutePath();
    }
    static {
        String _os  = System.getProperty("os.name").toLowerCase();
        String _lib = _os.contains("win") ? "acwidgets.dll" : "libacwidgets.so";
        Path   _p   = _resolveLib(_lib);
        _SYM = SymbolLookup.libraryLookup(_p, Arena.global());
    }
    private static MethodHandle _mh(String n, FunctionDescriptor fd) {
        return _L.downcallHandle(_SYM.find(n).orElseThrow(), fd);
    }

    private static final ValueLayout.OfLong  W  = ValueLayout.JAVA_LONG;  // ac_widget_t = intptr_t
    private static final ValueLayout.OfInt   I  = ValueLayout.JAVA_INT;
    private static final ValueLayout.OfDouble D  = ValueLayout.JAVA_DOUBLE;
    private static final ValueLayout.OfByte  B  = ValueLayout.JAVA_BYTE;
    private static final AddressLayout        A  = ValueLayout.ADDRESS;

    private static final MethodHandle _init            = _mh("ac_widgets_init",            FunctionDescriptor.ofVoid());
    private static final MethodHandle _setLazy         = _mh("ac_widgets_set_lazy",        FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _screenNew       = _mh("ac_widgets_screen_new",      FunctionDescriptor.of(W,A));
    private static final MethodHandle _screenMainloop  = _mh("ac_widgets_screen_mainloop", FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _screenUpdate    = _mh("ac_widgets_screen_update",   FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _screenDestroy   = _mh("ac_widgets_screen_destroy",  FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _screenDimensions = _mh("ac_widgets_screen_dimensions", FunctionDescriptor.ofVoid(W,I,I));
    private static final MethodHandle _displayNew      = _mh("ac_widgets_display_new",     FunctionDescriptor.of(W,W,A));
    private static final MethodHandle _displayPack     = _mh("ac_widgets_display_pack",    FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _displaySet      = _mh("ac_widgets_display_set",     FunctionDescriptor.ofVoid(W,A));
    private static final MethodHandle _displayGet      = _mh("ac_widgets_display_get",     FunctionDescriptor.of(A,W));
    private static final MethodHandle _askNew          = _mh("ac_widgets_ask_new",         FunctionDescriptor.of(W,W,I));
    private static final MethodHandle _askPack         = _mh("ac_widgets_ask_pack",        FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _askGet          = _mh("ac_widgets_ask_get",         FunctionDescriptor.of(A,W));
    private static final MethodHandle _askSet          = _mh("ac_widgets_ask_set",         FunctionDescriptor.ofVoid(W,A));
    private static final MethodHandle _btnNew          = _mh("ac_widgets_btn_new",         FunctionDescriptor.of(W,W,A));
    private static final MethodHandle _btnPack         = _mh("ac_widgets_btn_pack",        FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _btnOnClick      = _mh("ac_widgets_btn_on_click",    FunctionDescriptor.ofVoid(W,A,A));
    private static final MethodHandle _ckbtnNew        = _mh("ac_widgets_ckbtn_new",       FunctionDescriptor.of(W,W,A));
    private static final MethodHandle _ckbtnPack       = _mh("ac_widgets_ckbtn_pack",      FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _ckbtnGet        = _mh("ac_widgets_ckbtn_get",       FunctionDescriptor.of(I,W));
    private static final MethodHandle _ckbtnSet        = _mh("ac_widgets_ckbtn_set",       FunctionDescriptor.ofVoid(W,I));
    private static final MethodHandle _dropNew         = _mh("ac_widgets_dropdown_new",    FunctionDescriptor.of(W,W));
    private static final MethodHandle _dropPack        = _mh("ac_widgets_dropdown_pack",   FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _dropAdd         = _mh("ac_widgets_dropdown_add",    FunctionDescriptor.ofVoid(W,A));
    private static final MethodHandle _dropGet         = _mh("ac_widgets_dropdown_get",    FunctionDescriptor.of(A,W));
    private static final MethodHandle _dropSet         = _mh("ac_widgets_dropdown_set",    FunctionDescriptor.ofVoid(W,A));
    private static final MethodHandle _advNew          = _mh("ac_widgets_advance_new",     FunctionDescriptor.of(W,W,I));
    private static final MethodHandle _advPack         = _mh("ac_widgets_advance_pack",    FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _advSet          = _mh("ac_widgets_advance_set",     FunctionDescriptor.ofVoid(W,D));
    private static final MethodHandle _advGet          = _mh("ac_widgets_advance_get",     FunctionDescriptor.of(D,W));
    private static final MethodHandle _sliderNew       = _mh("ac_widgets_slider_new",      FunctionDescriptor.of(W,W,D,D,A));
    private static final MethodHandle _sliderPack      = _mh("ac_widgets_slider_pack",     FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _sliderGet       = _mh("ac_widgets_slider_get",      FunctionDescriptor.of(D,W));
    private static final MethodHandle _sliderSet       = _mh("ac_widgets_slider_set",      FunctionDescriptor.ofVoid(W,D));
    private static final MethodHandle _groupNew        = _mh("ac_widgets_group_new",       FunctionDescriptor.of(W,W,A));
    private static final MethodHandle _groupPack       = _mh("ac_widgets_group_pack",      FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _tabsNew         = _mh("ac_widgets_tabs_new",        FunctionDescriptor.of(W,W));
    private static final MethodHandle _tabsPack        = _mh("ac_widgets_tabs_pack",       FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _tabsAddTab      = _mh("ac_widgets_tabs_add_tab",    FunctionDescriptor.of(W,W,A));
    private static final MethodHandle _scrollNew       = _mh("ac_widgets_scroller_new",    FunctionDescriptor.of(W,W,A));
    private static final MethodHandle _scrollPack      = _mh("ac_widgets_scroller_pack",   FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _tblNew          = _mh("ac_widgets_table_new",       FunctionDescriptor.of(W,W,A,I));
    private static final MethodHandle _tblPack         = _mh("ac_widgets_table_pack",      FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _tblAdd          = _mh("ac_widgets_table_add",       FunctionDescriptor.ofVoid(W,A));
    private static final MethodHandle _lbNew           = _mh("ac_widgets_listbox_new",     FunctionDescriptor.of(W,W,I,I));
    private static final MethodHandle _lbPack          = _mh("ac_widgets_listbox_pack",    FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _lbAdd           = _mh("ac_widgets_listbox_add",     FunctionDescriptor.ofVoid(W,A));
    private static final MethodHandle _lbItem          = _mh("ac_widgets_listbox_item",    FunctionDescriptor.of(A,W,I));
    private static final MethodHandle _lbCount         = _mh("ac_widgets_listbox_count",   FunctionDescriptor.of(I,W));
    private static final MethodHandle _skNew           = _mh("ac_widgets_sketch_new",      FunctionDescriptor.of(W,W,I,I));
    private static final MethodHandle _skPack          = _mh("ac_widgets_sketch_pack",     FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _skClear         = _mh("ac_widgets_sketch_clear",    FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _skLine          = _mh("ac_widgets_sketch_line",     FunctionDescriptor.ofVoid(W,D,D,D,D,B,B,B));
    private static final MethodHandle _skRect          = _mh("ac_widgets_sketch_rect",     FunctionDescriptor.ofVoid(W,D,D,D,D,B,B,B));
    private static final MethodHandle _skCircle        = _mh("ac_widgets_sketch_circle",   FunctionDescriptor.ofVoid(W,D,D,D,B,B,B));
    private static final MethodHandle _skText          = _mh("ac_widgets_sketch_text",     FunctionDescriptor.ofVoid(W,D,D,A,B,B,B));
    private static final MethodHandle _tbNew           = _mh("ac_widgets_textbox_new",     FunctionDescriptor.of(W,W,A,A));
    private static final MethodHandle _tbPack          = _mh("ac_widgets_textbox_pack",    FunctionDescriptor.ofVoid(W));
    private static final MethodHandle _tbWrite         = _mh("ac_widgets_textbox_write",   FunctionDescriptor.ofVoid(W,A));
    private static final MethodHandle _tbGet           = _mh("ac_widgets_textbox_get",     FunctionDescriptor.of(A,W));
    private static final MethodHandle _tbFind          = _mh("ac_widgets_textbox_find",    FunctionDescriptor.of(A,W,A));
    private static final MethodHandle _tbFix           = _mh("ac_widgets_textbox_fix",     FunctionDescriptor.ofVoid(W,A));

    private static long  _l(MethodHandle h, Object... a) { try{return (long)h.invokeWithArguments(a);}catch(Throwable t){throw new RuntimeException(t);} }
    private static int   _i(MethodHandle h, Object... a) { try{return (int)h.invokeWithArguments(a);}catch(Throwable t){throw new RuntimeException(t);} }
    private static double _d(MethodHandle h, Object... a){ try{return (double)h.invokeWithArguments(a);}catch(Throwable t){throw new RuntimeException(t);} }
    private static MemorySegment _a(MethodHandle h, Object... a) { try{return (MemorySegment)h.invokeWithArguments(a);}catch(Throwable t){throw new RuntimeException(t);} }
    private static void  _v(MethodHandle h, Object... a) { try{h.invokeWithArguments(a);}catch(Throwable t){throw new RuntimeException(t);} }

    static { _v(_init); }

    public static void setLazy(long h) { _v(_setLazy, h); }

    private static String _str(MemorySegment p) { return p == null || p.equals(MemorySegment.NULL) ? "" : p.reinterpret(Long.MAX_VALUE).getUtf8String(0); }

    // title is mandatory — no geometry positional arg. Use screenDimensions(h, w, h) instead.
    public static long   screenNew(String title) {
        try (Arena a=Arena.ofConfined()){return _l(_screenNew,a.allocateUtf8String(title));}
    }
    public static void   screenMainloop(long h) { _v(_screenMainloop, h); }
    public static void   screenUpdate(long h)   { _v(_screenUpdate, h); }
    public static void   screenDimensions(long h, int w, int hh) { _v(_screenDimensions, h, w, hh); }
    public static void   screenDestroy(long h)  { _v(_screenDestroy, h); }

    public static long   displayNew(long m, String text)  { try(Arena a=Arena.ofConfined()){return _l(_displayNew,m,a.allocateUtf8String(text));} }
    public static void   displayPack(long h)              { _v(_displayPack, h); }
    public static void   displaySet(long h, String t)     { try(Arena a=Arena.ofConfined()){_v(_displaySet,h,a.allocateUtf8String(t));} }
    public static String displayGet(long h)               { return _str(_a(_displayGet, h)); }

    public static long   askNew(long m, int width)        { return _l(_askNew, m, width); }
    public static void   askPack(long h)                  { _v(_askPack, h); }
    public static String askGet(long h)                   { return _str(_a(_askGet, h)); }
    public static void   askSet(long h, String t)         { try(Arena a=Arena.ofConfined()){_v(_askSet,h,a.allocateUtf8String(t));} }

    public static long   btnNew(long m, String text)      { try(Arena a=Arena.ofConfined()){return _l(_btnNew,m,a.allocateUtf8String(text));} }
    public static void   btnPack(long h)                  { _v(_btnPack, h); }
    // `stub` is a native upcall trampoline for `void (*)(void*)` — see the AC-generated class's
    // own static block (per-callback bridge + MethodHandle + Linker.upcallStub) for how it's
    // built; userdata is unused (each stub is already bound to one specific AC function).
    public static void   btnOnClick(long h, MemorySegment stub) { _v(_btnOnClick, h, stub, MemorySegment.NULL); }

    public static long   ckbtnNew(long m, String text)    { try(Arena a=Arena.ofConfined()){return _l(_ckbtnNew,m,a.allocateUtf8String(text));} }
    public static void   ckbtnPack(long h)                { _v(_ckbtnPack, h); }
    public static boolean ckbtnGet(long h)                { return _i(_ckbtnGet, h) != 0; }
    public static void   ckbtnSet(long h, boolean v)      { _v(_ckbtnSet, h, v ? 1 : 0); }

    public static long   dropdownNew(long m)              { return _l(_dropNew, m); }
    public static void   dropdownPack(long h)             { _v(_dropPack, h); }
    public static void   dropdownAdd(long h, String item) { try(Arena a=Arena.ofConfined()){_v(_dropAdd,h,a.allocateUtf8String(item));} }
    public static String dropdownGet(long h)              { return _str(_a(_dropGet, h)); }
    public static void   dropdownSet(long h, String item) { try(Arena a=Arena.ofConfined()){_v(_dropSet,h,a.allocateUtf8String(item));} }

    public static long   advanceNew(long m, int length)   { return _l(_advNew, m, length); }
    public static void   advancePack(long h)              { _v(_advPack, h); }
    public static void   advanceSet(long h, double v)     { _v(_advSet, h, v); }
    public static double advanceGet(long h)               { return _d(_advGet, h); }

    public static long   sliderNew(long m, double from_val, double to_val, String orient) {
        try(Arena a=Arena.ofConfined()){return _l(_sliderNew,m,from_val,to_val,a.allocateUtf8String(orient));}
    }
    public static void   sliderPack(long h)               { _v(_sliderPack, h); }
    public static double sliderGet(long h)                { return _d(_sliderGet, h); }
    public static void   sliderSet(long h, double v)      { _v(_sliderSet, h, v); }

    public static long   groupNew(long m, String text)    { try(Arena a=Arena.ofConfined()){return _l(_groupNew,m,a.allocateUtf8String(text));} }
    public static void   groupPack(long h)                { _v(_groupPack, h); }

    public static long   tabsNew(long m)                  { return _l(_tabsNew, m); }
    public static void   tabsPack(long h)                 { _v(_tabsPack, h); }
    public static long   tabsAddTab(long h, String name)  { try(Arena a=Arena.ofConfined()){return _l(_tabsAddTab,h,a.allocateUtf8String(name));} }

    public static long   scrollerNew(long m, String orient) { try(Arena a=Arena.ofConfined()){return _l(_scrollNew,m,a.allocateUtf8String(orient));} }
    public static void   scrollerPack(long h)                { _v(_scrollPack, h); }

    public static long   tableNew(long m, String columnsCsv, int height) {
        try(Arena a=Arena.ofConfined()){return _l(_tblNew,m,a.allocateUtf8String(columnsCsv),height);}
    }
    public static void   tablePack(long h)                { _v(_tblPack, h); }
    public static void   tableAdd(long h, String valuesCsv){ try(Arena a=Arena.ofConfined()){_v(_tblAdd,h,a.allocateUtf8String(valuesCsv));} }

    public static long   listboxNew(long m, int w, int ht){ return _l(_lbNew, m, w, ht); }
    public static void   listboxPack(long h)              { _v(_lbPack, h); }
    public static void   listboxAdd(long h, String item)  { try(Arena a=Arena.ofConfined()){_v(_lbAdd,h,a.allocateUtf8String(item));} }
    public static String listboxItem(long h, int idx)     { return _str(_a(_lbItem, h, idx)); }
    public static int    listboxCount(long h)             { return _i(_lbCount, h); }

    public static long   sketchNew(long m, int w, int ht) { return _l(_skNew, m, w, ht); }
    public static void   sketchPack(long h)               { _v(_skPack, h); }
    public static void   sketchClear(long h)              { _v(_skClear, h); }
    public static void   sketchLine(long h, double x1, double y1, double x2, double y2, byte r, byte g, byte b) { _v(_skLine,h,x1,y1,x2,y2,r,g,b); }
    public static void   sketchRect(long h, double x1, double y1, double x2, double y2, byte r, byte g, byte b) { _v(_skRect,h,x1,y1,x2,y2,r,g,b); }
    public static void   sketchCircle(long h, double cx, double cy, double rad, byte r, byte g, byte b)         { _v(_skCircle,h,cx,cy,rad,r,g,b); }
    public static void   sketchText(long h, double x, double y, String t, byte r, byte g, byte b) {
        try(Arena a=Arena.ofConfined()){_v(_skText,h,x,y,a.allocateUtf8String(t),r,g,b);}
    }

    public static long   textboxNew(long m, String color, String font) {
        try (Arena a=Arena.ofConfined()){return _l(_tbNew,m,a.allocateUtf8String(color),a.allocateUtf8String(font));}
    }
    public static void   textboxPack(long h)  { _v(_tbPack, h); }
    public static void   textboxWrite(long h, String s) { try(Arena a=Arena.ofConfined()){_v(_tbWrite,h,a.allocateUtf8String(s));} }
    public static String textboxGet(long h)   { return _str(_a(_tbGet, h)); }
    public static String textboxFind(long h, String needle) {
        try(Arena a=Arena.ofConfined()){return _str(_a(_tbFind,h,a.allocateUtf8String(needle)));}
    }
    public static void   textboxFix(long h, String s) { try(Arena a=Arena.ofConfined()){_v(_tbFix,h,a.allocateUtf8String(s));} }
}

// ── Wrapper classes — AC-generated Java uses Screen(title=...), display(master=...) etc. ──

class Screen {
    long _h;
    // title is mandatory — no geometry positional arg. Use .dimensions(w, h) instead.
    Screen(String title) { this._h = AcWidgets.screenNew(title); }
    void mainloop() { AcWidgets.screenMainloop(this._h); }
    void update()   { AcWidgets.screenUpdate(this._h); }
    void dimensions(int w, int h) { AcWidgets.screenDimensions(this._h, w, h); }
    void destroy()  { AcWidgets.screenDestroy(this._h); }
}
class AcDisplay {
    long _h;
    AcDisplay(Screen m, String text) { this._h = AcWidgets.displayNew(m._h, text); }
    // A "master" isn't always a Screen — `tabber.add_tab(...)` returns a plain handle usable as
    // any other widget's master too (AC's own model is fully untyped: every widget is just a
    // scalar). javac picks this overload automatically for a `long`-typed master argument.
    AcDisplay(long m, String text) { this._h = AcWidgets.displayNew(m, text); }
    void pack()           { AcWidgets.displayPack(this._h); }
    void set(String v)    { AcWidgets.displaySet(this._h, v); }
    String get()          { return AcWidgets.displayGet(this._h); }
    void config(String v) { set(v); }
}
class AcAsk {
    long _h;
    AcAsk(Screen m, int width) { this._h = AcWidgets.askNew(m._h, width); }
    void pack()        { AcWidgets.askPack(this._h); }
    String get()       { return AcWidgets.askGet(this._h); }
    void set(String v) { AcWidgets.askSet(this._h, v); }
}
class AcBtn {
    long _h;
    AcBtn(Screen m, String text) { this._h = AcWidgets.btnNew(m._h, text); }
    void pack() { AcWidgets.btnPack(this._h); }
    void onClick(java.lang.foreign.MemorySegment stub) { AcWidgets.btnOnClick(this._h, stub); }
}
class AcCkbtn {
    long _h;
    AcCkbtn(Screen m, String text) { this._h = AcWidgets.ckbtnNew(m._h, text); }
    void pack()        { AcWidgets.ckbtnPack(this._h); }
    boolean get()      { return AcWidgets.ckbtnGet(this._h); }
    void set(boolean v){ AcWidgets.ckbtnSet(this._h, v); }
}
class AcRadbtn {
    long _h;
    AcRadbtn(Screen m, String text) { this._h = AcWidgets.ckbtnNew(m._h, text); }
    void pack()    { AcWidgets.ckbtnPack(this._h); }
    boolean get()  { return AcWidgets.ckbtnGet(this._h); }
}
class AcDropdown {
    long _h;
    // AC's `dropdown(root)` constructs empty, then adds items one at a time via separate
    // `.add(...)` calls — the bulk comma-split constructor below predates that call shape and
    // is kept only for any caller still using it directly.
    AcDropdown(Screen m) { this._h = AcWidgets.dropdownNew(m._h); }
    AcDropdown(Screen m, String values) {
        this._h = AcWidgets.dropdownNew(m._h);
        for (String v : values.split(",")) AcWidgets.dropdownAdd(this._h, v.trim());
    }
    void pack()          { AcWidgets.dropdownPack(this._h); }
    void add(String item){ AcWidgets.dropdownAdd(this._h, item); }
    String get()         { return AcWidgets.dropdownGet(this._h); }
    void set(String v)   { AcWidgets.dropdownSet(this._h, v); }
}
class AcAdvance {
    long _h;
    AcAdvance(Screen m, int length) { this._h = AcWidgets.advanceNew(m._h, length); }
    void pack()       { AcWidgets.advancePack(this._h); }
    void set(double v){ AcWidgets.advanceSet(this._h, v); }
    double get()      { return AcWidgets.advanceGet(this._h); }
}
class AcSlider {
    long _h;
    AcSlider(Screen m, double from_val, double to_val, String orient) { this._h = AcWidgets.sliderNew(m._h, from_val, to_val, orient); }
    void pack()       { AcWidgets.sliderPack(this._h); }
    double get()      { return AcWidgets.sliderGet(this._h); }
    void set(double v){ AcWidgets.sliderSet(this._h, v); }
}
class AcGroup {
    long _h;
    AcGroup(Screen m, String text) { this._h = AcWidgets.groupNew(m._h, text); }
    void pack() { AcWidgets.groupPack(this._h); }
}
class AcTabs {
    long _h;
    AcTabs(Screen m) { this._h = AcWidgets.tabsNew(m._h); }
    void pack()          { AcWidgets.tabsPack(this._h); }
    // Returns the new page as a plain handle (usable as any other widget's "master"), matching
    // the C header's own doc comment on ac_widgets_tabs_add_tab — NOT `this` (the tabs widget
    // itself), which the previous stub incorrectly returned while doing nothing else at all.
    long add_tab(String n) { return AcWidgets.tabsAddTab(this._h, n); }
}
class AcScroller {
    long _h;
    AcScroller(Screen m, String orient) { this._h = AcWidgets.scrollerNew(m._h, orient); }
    void pack() { AcWidgets.scrollerPack(this._h); }
}
class AcTable {
    long _h;
    AcTable(Screen m, String columnsCsv, int height) { this._h = AcWidgets.tableNew(m._h, columnsCsv, height); }
    void pack()           { AcWidgets.tablePack(this._h); }
    void add(String valuesCsv) { AcWidgets.tableAdd(this._h, valuesCsv); }
}
class AcListbox {
    long _h;
    AcListbox(Screen m, int width, int height) { this._h = AcWidgets.listboxNew(m._h, width, height); }
    void pack()          { AcWidgets.listboxPack(this._h); }
    void add(String item){ AcWidgets.listboxAdd(this._h, item); }
    java.util.List<String> get() {
        int n = AcWidgets.listboxCount(this._h);
        java.util.List<String> out = new ArrayList<>();
        for (int i=0;i<n;i++) out.add(AcWidgets.listboxItem(this._h, i));
        return out;
    }
}
class AcSketch {
    long _h;
    AcSketch(Screen m, int width, int height) { this._h = AcWidgets.sketchNew(m._h, width, height); }
    void pack()  { AcWidgets.sketchPack(this._h); }
    void clear() { AcWidgets.sketchClear(this._h); }
    void line(double x1, double y1, double x2, double y2, byte r, byte g, byte b) { AcWidgets.sketchLine(this._h,x1,y1,x2,y2,r,g,b); }
    void rect(double x1, double y1, double x2, double y2, byte r, byte g, byte b) { AcWidgets.sketchRect(this._h,x1,y1,x2,y2,r,g,b); }
    void circle(double cx, double cy, double rad, byte r, byte g, byte b)          { AcWidgets.sketchCircle(this._h,cx,cy,rad,r,g,b); }
    void text_at(double x, double y, String t, byte r, byte g, byte b)             { AcWidgets.sketchText(this._h,x,y,t,r,g,b); }
}
class AcTextbox {
    long _h;
    AcTextbox(Screen m, String color, String font) { this._h = AcWidgets.textboxNew(m._h, color, font); }
    void pack()             { AcWidgets.textboxPack(this._h); }
    void write(String s)    { AcWidgets.textboxWrite(this._h, s); }
    String get()            { return AcWidgets.textboxGet(this._h); }
    String find(String needle) { return AcWidgets.textboxFind(this._h, needle); }
    void fix(String s)      { AcWidgets.textboxFix(this._h, s); }
}
