// AC ilib: gl — Java Panama FFI (libacgl.so / acgl.dll)
// Requires: Java 21+, --enable-preview, java.lang.foreign module
import java.lang.foreign.*;
import java.lang.invoke.*;
import java.nio.file.*;

final class AcGl {
    private static final Linker _L = Linker.nativeLinker();
    private static final SymbolLookup _SYM;
    static {
        String _os = System.getProperty("os.name").toLowerCase();
        String _lib = _os.contains("win") ? "acgl.dll" : "libacgl.so";
        Path _p = Path.of(System.getProperty("user.dir"))
            .resolve("library/gl/" + _lib).toAbsolutePath();
        _SYM = SymbolLookup.libraryLookup(_p, Arena.global());
    }
    private static MethodHandle _mh(String n, FunctionDescriptor fd) {
        return _L.downcallHandle(_SYM.find(n).orElseThrow(), fd);
    }

    private static final ValueLayout.OfInt   I  = ValueLayout.JAVA_INT;
    private static final ValueLayout.OfFloat F  = ValueLayout.JAVA_FLOAT;
    private static final ValueLayout.OfByte  B  = ValueLayout.JAVA_BYTE;
    private static final AddressLayout       A  = ValueLayout.ADDRESS;

    private static final MethodHandle _init              = _mh("ac_gl_init",                     FunctionDescriptor.of(I));
    private static final MethodHandle _quit              = _mh("ac_gl_quit",                     FunctionDescriptor.ofVoid());
    private static final MethodHandle _screenCreate      = _mh("ac_gl_screen_create",            FunctionDescriptor.of(I,I,I,A));
    private static final MethodHandle _screenSetBg       = _mh("ac_gl_screen_set_bg",            FunctionDescriptor.ofVoid(B,B,B));
    private static final MethodHandle _screenSetFps      = _mh("ac_gl_screen_set_fps",           FunctionDescriptor.ofVoid(I));
    private static final MethodHandle _screenW           = _mh("ac_gl_screen_w",                 FunctionDescriptor.of(I));
    private static final MethodHandle _screenH           = _mh("ac_gl_screen_h",                 FunctionDescriptor.of(I));
    private static final MethodHandle _objCreate         = _mh("ac_gl_obj_create",               FunctionDescriptor.ofVoid(A));
    private static final MethodHandle _objGeometry       = _mh("ac_gl_obj_geometry",             FunctionDescriptor.ofVoid(A,I,I));
    private static final MethodHandle _objSquare         = _mh("ac_gl_obj_square",               FunctionDescriptor.ofVoid(A,I));
    private static final MethodHandle _objPos            = _mh("ac_gl_obj_pos",                  FunctionDescriptor.ofVoid(A,I,I));
    private static final MethodHandle _objColor          = _mh("ac_gl_obj_color",                FunctionDescriptor.ofVoid(A,B,B,B));
    private static final MethodHandle _objVelocity       = _mh("ac_gl_obj_velocity",             FunctionDescriptor.ofVoid(A,F,F));
    private static final MethodHandle _objSetSpeed       = _mh("ac_gl_obj_set_speed",            FunctionDescriptor.ofVoid(A,F));
    private static final MethodHandle _objSetDirection   = _mh("ac_gl_obj_set_direction",        FunctionDescriptor.ofVoid(A,F));
    private static final MethodHandle _objSpeedMult      = _mh("ac_gl_obj_speed_mult",           FunctionDescriptor.ofVoid(A,F));
    private static final MethodHandle _objMoveX          = _mh("ac_gl_obj_move_x",               FunctionDescriptor.ofVoid(A,I));
    private static final MethodHandle _objMoveY          = _mh("ac_gl_obj_move_y",               FunctionDescriptor.ofVoid(A,I));
    private static final MethodHandle _objX              = _mh("ac_gl_obj_x",                    FunctionDescriptor.of(I,A));
    private static final MethodHandle _objY              = _mh("ac_gl_obj_y",                    FunctionDescriptor.of(I,A));
    private static final MethodHandle _objW              = _mh("ac_gl_obj_w",                    FunctionDescriptor.of(I,A));
    private static final MethodHandle _objH              = _mh("ac_gl_obj_h",                    FunctionDescriptor.of(I,A));
    private static final MethodHandle _objCurveshape     = _mh("ac_gl_obj_curveshape",           FunctionDescriptor.ofVoid(A,A));
    private static final MethodHandle _objVertex         = _mh("ac_gl_obj_vertex",               FunctionDescriptor.ofVoid(A,F,F));
    private static final MethodHandle _objToDraw         = _mh("ac_gl_obj_to_draw",              FunctionDescriptor.ofVoid(A));
    private static final MethodHandle _objCircleFall     = _mh("ac_gl_obj_circle_fall",          FunctionDescriptor.ofVoid(A,F,A));
    private static final MethodHandle _objCircleFell     = _mh("ac_gl_obj_circle_fell",          FunctionDescriptor.of(I,A,F,A));
    private static final MethodHandle _objSetSpawn       = _mh("ac_gl_obj_set_spawn",            FunctionDescriptor.ofVoid(A));
    private static final MethodHandle _objRegen          = _mh("ac_gl_obj_regen",                FunctionDescriptor.ofVoid(A));
    private static final MethodHandle _objAnimate        = _mh("ac_gl_obj_animate",              FunctionDescriptor.ofVoid(A,A,F));
    private static final MethodHandle _hitboxMany        = _mh("ac_gl_hitbox_many_overlap",      FunctionDescriptor.of(I));
    private static final MethodHandle _drawCreate        = _mh("ac_gl_draw_create",              FunctionDescriptor.ofVoid(A));
    private static final MethodHandle _drawCurveshape    = _mh("ac_gl_draw_curveshape",          FunctionDescriptor.ofVoid(A,A));
    private static final MethodHandle _drawVertex        = _mh("ac_gl_draw_vertex",              FunctionDescriptor.ofVoid(A,F,F));
    private static final MethodHandle _drawLine          = _mh("ac_gl_draw_line",                FunctionDescriptor.ofVoid(A,F,F,F,F,B,B,B));
    private static final MethodHandle _drawCircle        = _mh("ac_gl_draw_circle",              FunctionDescriptor.ofVoid(A,F,F,F,B,B,B));
    private static final MethodHandle _drawClear         = _mh("ac_gl_draw_clear",               FunctionDescriptor.ofVoid(A));
    private static final MethodHandle _drawToObj         = _mh("ac_gl_draw_to_obj",              FunctionDescriptor.ofVoid(A));
    private static final MethodHandle _isDraw            = _mh("ac_gl_is_draw",                  FunctionDescriptor.of(I,A));
    private static final MethodHandle _isObj             = _mh("ac_gl_is_obj",                   FunctionDescriptor.of(I,A));
    private static final MethodHandle _hitboxOverlap     = _mh("ac_gl_hitbox_overlap",           FunctionDescriptor.of(I,A,A));
    private static final MethodHandle _hitboxBoundary    = _mh("ac_gl_hitbox_overlap_boundary",  FunctionDescriptor.of(I,A));
    private static final MethodHandle _hitboxPattern     = _mh("ac_gl_hitbox_overlap_pattern",   FunctionDescriptor.of(I,A,A));
    private static final MethodHandle _keyPressed        = _mh("ac_gl_key_pressed",              FunctionDescriptor.of(I,A));
    private static final MethodHandle _keyJustPressed    = _mh("ac_gl_key_just_pressed",         FunctionDescriptor.of(I,A));
    private static final MethodHandle _frameBegin        = _mh("ac_gl_frame_begin",              FunctionDescriptor.of(I));
    private static final MethodHandle _frameUpdate       = _mh("ac_gl_frame_update",             FunctionDescriptor.ofVoid(F));
    private static final MethodHandle _frameRender       = _mh("ac_gl_frame_render",             FunctionDescriptor.ofVoid());
    private static final MethodHandle _frameEnd          = _mh("ac_gl_frame_end",                FunctionDescriptor.ofVoid());
    private static final MethodHandle _deltaTime         = _mh("ac_gl_delta_time",               FunctionDescriptor.of(F));

    private static int   _i(MethodHandle h, Object... a) { try{return (int)h.invokeWithArguments(a);}catch(Throwable t){throw new RuntimeException(t);} }
    private static float _f(MethodHandle h, Object... a) { try{return (float)h.invokeWithArguments(a);}catch(Throwable t){throw new RuntimeException(t);} }
    private static void  _v(MethodHandle h, Object... a) { try{h.invokeWithArguments(a);}catch(Throwable t){throw new RuntimeException(t);} }

    public static boolean init()                           { return _i(_init) != 0; }
    public static void    quit()                           { _v(_quit); }
    public static boolean screenCreate(int w, int h, String title) {
        try (Arena a = Arena.ofConfined()) { return _i(_screenCreate, w, h, a.allocateUtf8String(title)) != 0; }
    }
    public static void screenSetBg(byte r, byte g, byte b) { _v(_screenSetBg, r, g, b); }
    public static void screenSetFps(int fps)                { _v(_screenSetFps, fps); }
    public static int  screenW()                            { return _i(_screenW); }
    public static int  screenH()                            { return _i(_screenH); }

    public static void objCreate(String name) {
        try (Arena a = Arena.ofConfined()) { _v(_objCreate, a.allocateUtf8String(name)); }
    }
    public static void objGeometry(String name, int w, int h) {
        try (Arena a = Arena.ofConfined()) { _v(_objGeometry, a.allocateUtf8String(name), w, h); }
    }
    public static void objSquare(String name, int size) {
        try (Arena a = Arena.ofConfined()) { _v(_objSquare, a.allocateUtf8String(name), size); }
    }
    public static void objPos(String name, int x, int y) {
        try (Arena a = Arena.ofConfined()) { _v(_objPos, a.allocateUtf8String(name), x, y); }
    }
    public static void objColor(String name, byte r, byte g, byte b) {
        try (Arena a = Arena.ofConfined()) { _v(_objColor, a.allocateUtf8String(name), r, g, b); }
    }
    public static void objVelocity(String name, float vx, float vy) {
        try (Arena a = Arena.ofConfined()) { _v(_objVelocity, a.allocateUtf8String(name), vx, vy); }
    }
    public static void objSetSpeed(String name, float s) {
        try (Arena a = Arena.ofConfined()) { _v(_objSetSpeed, a.allocateUtf8String(name), s); }
    }
    public static void objSetDirection(String name, float deg) {
        try (Arena a = Arena.ofConfined()) { _v(_objSetDirection, a.allocateUtf8String(name), deg); }
    }
    public static void objSpeedMult(String name, float m) {
        try (Arena a = Arena.ofConfined()) { _v(_objSpeedMult, a.allocateUtf8String(name), m); }
    }
    public static void objMoveX(String name, int dx) {
        try (Arena a = Arena.ofConfined()) { _v(_objMoveX, a.allocateUtf8String(name), dx); }
    }
    public static void objMoveY(String name, int dy) {
        try (Arena a = Arena.ofConfined()) { _v(_objMoveY, a.allocateUtf8String(name), dy); }
    }
    public static int objX(String name) { try (Arena a=Arena.ofConfined()){return _i(_objX,a.allocateUtf8String(name));} }
    public static int objY(String name) { try (Arena a=Arena.ofConfined()){return _i(_objY,a.allocateUtf8String(name));} }
    public static int objW(String name) { try (Arena a=Arena.ofConfined()){return _i(_objW,a.allocateUtf8String(name));} }
    public static int objH(String name) { try (Arena a=Arena.ofConfined()){return _i(_objH,a.allocateUtf8String(name));} }
    public static void objCurveshape(String name, String expr) {
        try (Arena a=Arena.ofConfined()){_v(_objCurveshape,a.allocateUtf8String(name),a.allocateUtf8String(expr));}
    }
    public static void objVertex(String name, float vx, float vy) {
        try (Arena a=Arena.ofConfined()){_v(_objVertex,a.allocateUtf8String(name),vx,vy);}
    }
    public static void objToDraw(String name) {
        try (Arena a=Arena.ofConfined()){_v(_objToDraw,a.allocateUtf8String(name));}
    }
    public static void objCircleFall(String name, float frac, String dir) {
        try (Arena a=Arena.ofConfined()){_v(_objCircleFall,a.allocateUtf8String(name),frac,a.allocateUtf8String(dir));}
    }
    public static boolean objCircleFell(String name, float frac, String dir) {
        try (Arena a=Arena.ofConfined()){return _i(_objCircleFell,a.allocateUtf8String(name),frac,a.allocateUtf8String(dir))!=0;}
    }
    public static void objSetSpawn(String name) {
        try (Arena a=Arena.ofConfined()){_v(_objSetSpawn,a.allocateUtf8String(name));}
    }
    public static void objRegen(String name) {
        try (Arena a=Arena.ofConfined()){_v(_objRegen,a.allocateUtf8String(name));}
    }
    public static void objAnimate(String name, String dir, float speed) {
        try (Arena a=Arena.ofConfined()){_v(_objAnimate,a.allocateUtf8String(name),a.allocateUtf8String(dir),speed);}
    }
    public static boolean hitboxManyOverlap() { return _i(_hitboxMany) != 0; }

    public static void drawCreate(String name) {
        try (Arena a=Arena.ofConfined()){_v(_drawCreate,a.allocateUtf8String(name));}
    }
    public static void drawCurveshape(String name, String expr) {
        try (Arena a=Arena.ofConfined()){_v(_drawCurveshape,a.allocateUtf8String(name),a.allocateUtf8String(expr));}
    }
    public static void drawVertex(String name, float vx, float vy) {
        try (Arena a=Arena.ofConfined()){_v(_drawVertex,a.allocateUtf8String(name),vx,vy);}
    }
    public static void drawLine(String name, float x1, float y1, float x2, float y2, byte r, byte g, byte b) {
        try (Arena a=Arena.ofConfined()){_v(_drawLine,a.allocateUtf8String(name),x1,y1,x2,y2,r,g,b);}
    }
    public static void drawCircle(String name, float cx, float cy, float radius, byte r, byte g, byte b) {
        try (Arena a=Arena.ofConfined()){_v(_drawCircle,a.allocateUtf8String(name),cx,cy,radius,r,g,b);}
    }
    public static void drawClear(String name) {
        try (Arena a=Arena.ofConfined()){_v(_drawClear,a.allocateUtf8String(name));}
    }
    public static void drawToObj(String name) {
        try (Arena a=Arena.ofConfined()){_v(_drawToObj,a.allocateUtf8String(name));}
    }
    public static boolean isDraw(String name) {
        try (Arena a=Arena.ofConfined()){return _i(_isDraw,a.allocateUtf8String(name))!=0;}
    }
    public static boolean isObj(String name) {
        try (Arena a=Arena.ofConfined()){return _i(_isObj,a.allocateUtf8String(name))!=0;}
    }

    public static boolean hitboxOverlap(String a, String b) {
        try (Arena ar=Arena.ofConfined()){return _i(_hitboxOverlap,ar.allocateUtf8String(a),ar.allocateUtf8String(b))!=0;}
    }
    public static boolean hitboxBoundary(String name) {
        try (Arena a=Arena.ofConfined()){return _i(_hitboxBoundary,a.allocateUtf8String(name))!=0;}
    }
    public static boolean hitboxOverlapPattern(String name, String pat) {
        try (Arena a=Arena.ofConfined()){return _i(_hitboxPattern,a.allocateUtf8String(name),a.allocateUtf8String(pat))!=0;}
    }

    public static boolean keyPressed(String k) {
        try (Arena a=Arena.ofConfined()){return _i(_keyPressed,a.allocateUtf8String(k))!=0;}
    }
    public static boolean keyJustPressed(String k) {
        try (Arena a=Arena.ofConfined()){return _i(_keyJustPressed,a.allocateUtf8String(k))!=0;}
    }

    public static boolean frameBegin()     { return _i(_frameBegin) != 0; }
    public static void    frameUpdate(float dt) { _v(_frameUpdate, dt); }
    public static void    frameRender()    { _v(_frameRender); }
    public static void    frameEnd()       { _v(_frameEnd); }
    public static float   deltaTime()      { return _f(_deltaTime); }

    // ── High-level helpers ────────────────────────────────────────────────────
    private static final MethodHandle _screenInit        = _mh("ac_gl_screen_init",             FunctionDescriptor.ofVoid(I,I,A));
    private static final MethodHandle _screenAnimate     = _mh("ac_gl_screen_animate",          FunctionDescriptor.ofVoid(A));
    private static final MethodHandle _screenSetBgByName = _mh("ac_gl_screen_set_bg_by_name",   FunctionDescriptor.ofVoid(A));
    private static final MethodHandle _objColorByName    = _mh("ac_gl_obj_color_by_name",       FunctionDescriptor.ofVoid(A,A));
    private static final MethodHandle _objPosFromSpec    = _mh("ac_gl_obj_pos_from_spec",       FunctionDescriptor.ofVoid(A,A,A));
    private static final MethodHandle _objConfigItem     = _mh("ac_gl_obj_config_item",         FunctionDescriptor.ofVoid(A,A));
    private static final MethodHandle _objSaveSpawn      = _mh("ac_gl_obj_save_spawn",          FunctionDescriptor.ofVoid(A));

    public static void screenInit(int w, int h, String title) {
        try (Arena a=Arena.ofConfined()){_v(_screenInit,w,h,a.allocateUtf8String(title));}
    }
    public static void screenAnimate(String spec) {
        try (Arena a=Arena.ofConfined()){_v(_screenAnimate,a.allocateUtf8String(spec));}
    }
    public static void screenSetBgByName(String color) {
        try (Arena a=Arena.ofConfined()){_v(_screenSetBgByName,a.allocateUtf8String(color));}
    }
    public static void objColorByName(String name, String color) {
        try (Arena a=Arena.ofConfined()){_v(_objColorByName,a.allocateUtf8String(name),a.allocateUtf8String(color));}
    }
    public static void objPosFromSpec(String name, String xs, String ys) {
        try (Arena a=Arena.ofConfined()){_v(_objPosFromSpec,a.allocateUtf8String(name),a.allocateUtf8String(xs),a.allocateUtf8String(ys));}
    }
    public static void objConfigItem(String name, String spec) {
        try (Arena a=Arena.ofConfined()){_v(_objConfigItem,a.allocateUtf8String(name),a.allocateUtf8String(spec));}
    }
    public static void objSaveSpawn(String name) {
        try (Arena a=Arena.ofConfined()){_v(_objSaveSpawn,a.allocateUtf8String(name));}
    }
}

// ── Namespace classes — AC-generated Java uses gl.screen.create(...) etc. ──

class gl {
    static final class screen {
        public static boolean create(int w, int h, String title) { return AcGl.screenCreate(w, h, title); }
        public static void set_bg(byte r, byte g, byte b)        { AcGl.screenSetBg(r, g, b); }
        public static void set_fps(int fps)                       { AcGl.screenSetFps(fps); }
        public static int  w()                                    { return AcGl.screenW(); }
        public static int  h()                                    { return AcGl.screenH(); }
    }
    static final class obj {
        public static void create(String name)                    { AcGl.objCreate(name); }
        public static void geometry(String name, int w, int h)    { AcGl.objGeometry(name, w, h); }
        public static void square(String name, int size)          { AcGl.objSquare(name, size); }
        public static void pos(String name, int x, int y)         { AcGl.objPos(name, x, y); }
        public static void color(String name, byte r, byte g, byte b) { AcGl.objColor(name, r, g, b); }
        public static void velocity(String name, float vx, float vy)  { AcGl.objVelocity(name, vx, vy); }
        public static void set_speed(String name, float s)         { AcGl.objSetSpeed(name, s); }
        public static void set_direction(String name, float deg)   { AcGl.objSetDirection(name, deg); }
        public static void speed_mult(String name, float m)        { AcGl.objSpeedMult(name, m); }
        public static void move_x(String name, int dx)            { AcGl.objMoveX(name, dx); }
        public static void move_y(String name, int dy)            { AcGl.objMoveY(name, dy); }
        public static int  x(String name)                         { return AcGl.objX(name); }
        public static int  y(String name)                         { return AcGl.objY(name); }
        public static int  w(String name)                         { return AcGl.objW(name); }
        public static int  h(String name)                         { return AcGl.objH(name); }
        public static void curveshape(String name, String expr)   { AcGl.objCurveshape(name, expr); }
        public static void vertex(String name, float vx, float vy){ AcGl.objVertex(name, vx, vy); }
        public static void to_draw(String name)                             { AcGl.objToDraw(name); }
        public static void circle_fall(String name, float f, String dir)   { AcGl.objCircleFall(name, f, dir); }
        public static boolean circle_fell(String name, float f, String dir)  { return AcGl.objCircleFell(name, f, dir); }
        public static void set_spawn(String name)                           { AcGl.objSetSpawn(name); }
        public static void regen(String name)                               { AcGl.objRegen(name); }
        public static void animate(String name, String dir, float speed)    { AcGl.objAnimate(name, dir, speed); }
    }
    static final class draw {
        public static void create(String name)                                             { AcGl.drawCreate(name); }
        public static void curveshape(String name, String expr)                            { AcGl.drawCurveshape(name, expr); }
        public static void vertex(String name, float vx, float vy)                        { AcGl.drawVertex(name, vx, vy); }
        public static void line(String name, float x1, float y1, float x2, float y2, byte r, byte g, byte b) { AcGl.drawLine(name,x1,y1,x2,y2,r,g,b); }
        public static void circle(String name, float cx, float cy, float rad, byte r, byte g, byte b)        { AcGl.drawCircle(name,cx,cy,rad,r,g,b); }
        public static void clear(String name)                                              { AcGl.drawClear(name); }
        public static void to_obj(String name)                                             { AcGl.drawToObj(name); }
    }
    static final class hitbox {
        public static boolean overlap(String a, String b)          { return AcGl.hitboxOverlap(a, b); }
        public static boolean boundary(String name)                 { return AcGl.hitboxBoundary(name); }
        public static boolean overlap_pattern(String n, String pat) { return AcGl.hitboxOverlapPattern(n, pat); }
        public static boolean many_overlap()                        { return AcGl.hitboxManyOverlap(); }
    }
    static final class key {
        public static boolean pressed(String k)      { return AcGl.keyPressed(k); }
        public static boolean just_pressed(String k) { return AcGl.keyJustPressed(k); }
    }
    static final class frame {
        public static boolean begin()      { return AcGl.frameBegin(); }
        public static void update(float dt){ AcGl.frameUpdate(dt); }
        public static void render()        { AcGl.frameRender(); }
        public static void end()           { AcGl.frameEnd(); }
        public static float delta()        { return AcGl.deltaTime(); }
    }

    public static boolean init()             { return AcGl.init(); }
    public static void    quit()             { AcGl.quit(); }
    public static boolean is_draw(String n)  { return AcGl.isDraw(n); }
    public static boolean is_obj(String n)   { return AcGl.isObj(n); }
}
