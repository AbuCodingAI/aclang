// AC ilib: camera — Java Panama FFI (libaccamera.so / libaccamera.dll)
import java.lang.foreign.*;
import java.lang.invoke.*;
import java.nio.file.*;

final class AcCamera {
    private static final Linker _L = Linker.nativeLinker();
    private static final SymbolLookup _SYM;
    static {
        String _os = System.getProperty("os.name").toLowerCase();
        String _libFile = _os.contains("win") ? "libaccamera.dll" : "libaccamera.so";
        Path _libPath = Path.of(System.getProperty("user.dir"))
            .resolve("library/camera/" + _libFile).toAbsolutePath();
        _SYM = SymbolLookup.libraryLookup(_libPath, Arena.global());
    }
    private static MethodHandle _mh(String name, FunctionDescriptor fd) {
        return _L.downcallHandle(_SYM.find(name).orElseThrow(), fd);
    }
    private static final ValueLayout.OfInt    I = ValueLayout.JAVA_INT;
    private static final AddressLayout        P = ValueLayout.ADDRESS;

    private static final MethodHandle _camera_init             = _mh("ac_camera_init",             FunctionDescriptor.of(I));
    private static final MethodHandle _camera_capture          = _mh("ac_camera_capture",          FunctionDescriptor.of(I,P));
    private static final MethodHandle _camera_capture_latest   = _mh("ac_camera_capture_latest",   FunctionDescriptor.of(I,P));
    private static final MethodHandle _camera_capture_first    = _mh("ac_camera_capture_first",    FunctionDescriptor.of(I,P));
    private static final MethodHandle _camera_release          = _mh("ac_camera_release",          FunctionDescriptor.ofVoid());
    private static final MethodHandle _sidebar_config          = _mh("ac_sidebar_config",          FunctionDescriptor.ofVoid(P,P));
    private static final MethodHandle _sidebar_setregion       = _mh("ac_sidebar_setregion",       FunctionDescriptor.ofVoid(P));
    private static final MethodHandle _sidebar_setinteractive  = _mh("ac_sidebar_setinteractive",  FunctionDescriptor.ofVoid(I));
    private static final MethodHandle _sidebar_display         = _mh("ac_sidebar_display",         FunctionDescriptor.ofVoid(P));
    private static final MethodHandle _sidebar_ask             = _mh("ac_sidebar_ask",             FunctionDescriptor.of(P,P));
    private static final MethodHandle _sidebar_getinput        = _mh("ac_sidebar_getinput",        FunctionDescriptor.of(P));
    private static final MethodHandle _screen_setmode          = _mh("ac_screen_setmode",          FunctionDescriptor.ofVoid(P));
    private static final MethodHandle _screen_update           = _mh("ac_screen_update",           FunctionDescriptor.ofVoid());

    private static final Arena _arena = Arena.ofAuto();
    @SuppressWarnings("preview")
    private static MemorySegment _cs(String s) { return _arena.allocateUtf8String(s); }
    @SuppressWarnings("preview")
    private static String _gs(MemorySegment p) { return p == null || p.equals(MemorySegment.NULL) ? "" : p.reinterpret(Long.MAX_VALUE).getUtf8String(0); }

    static int cameraInit()                     { try { return (int)_camera_init.invoke(); } catch(Throwable t){ throw new RuntimeException(t); } }
    static int cameraCapture(String f)          { try { return (int)_camera_capture.invoke(_cs(f)); } catch(Throwable t){ throw new RuntimeException(t); } }
    static int cameraCaptureLatest(String f)    { try { return (int)_camera_capture_latest.invoke(_cs(f)); } catch(Throwable t){ throw new RuntimeException(t); } }
    static int cameraCaptureFirst(String f)     { try { return (int)_camera_capture_first.invoke(_cs(f)); } catch(Throwable t){ throw new RuntimeException(t); } }
    static void cameraRelease()                 { try { _camera_release.invoke(); } catch(Throwable t){ throw new RuntimeException(t); } }
    static void sidebarConfig(String k,String v){ try { _sidebar_config.invoke(_cs(k),_cs(v)); } catch(Throwable t){ throw new RuntimeException(t); } }
    static void sidebarSetregion(String r)      { try { _sidebar_setregion.invoke(_cs(r)); } catch(Throwable t){ throw new RuntimeException(t); } }
    static void sidebarSetinteractive(int v)    { try { _sidebar_setinteractive.invoke(v); } catch(Throwable t){ throw new RuntimeException(t); } }
    static void sidebarDisplay(String m)        { try { _sidebar_display.invoke(_cs(m)); } catch(Throwable t){ throw new RuntimeException(t); } }
    static String sidebarAsk(String p)          { try { return _gs((MemorySegment)_sidebar_ask.invoke(_cs(p))); } catch(Throwable t){ throw new RuntimeException(t); } }
    static String sidebarGetinput()             { try { return _gs((MemorySegment)_sidebar_getinput.invoke()); } catch(Throwable t){ throw new RuntimeException(t); } }
    static void screenSetmode(String m)         { try { _screen_setmode.invoke(_cs(m)); } catch(Throwable t){ throw new RuntimeException(t); } }
    static void screenUpdate()                  { try { _screen_update.invoke(); } catch(Throwable t){ throw new RuntimeException(t); } }
}

// Namespace classes — AC-generated Java uses camera.init(), sidebar.display(), screen.update()
class camera {
    public static int init()                        { return AcCamera.cameraInit(); }
    public static int capture(String f)             { return AcCamera.cameraCapture(f); }
    public static int capture_latest(String f)      { return AcCamera.cameraCaptureLatest(f); }
    public static int capture_first(String f)       { return AcCamera.cameraCaptureFirst(f); }
    public static void release()                    { AcCamera.cameraRelease(); }
}
class sidebar {
    public static void config(String k, String v)   { AcCamera.sidebarConfig(k, v); }
    public static void setregion(String r)          { AcCamera.sidebarSetregion(r); }
    public static void setinteractive(int v)        { AcCamera.sidebarSetinteractive(v); }
    public static void display(String m)            { AcCamera.sidebarDisplay(m); }
    public static String ask(String p)              { return AcCamera.sidebarAsk(p); }
    public static String getinput()                 { return AcCamera.sidebarGetinput(); }
}
class screen {
    public static void setmode(String m)            { AcCamera.screenSetmode(m); }
    public static void update()                     { AcCamera.screenUpdate(); }
}
