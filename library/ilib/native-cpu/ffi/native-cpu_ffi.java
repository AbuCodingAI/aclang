// AC ilib: native-cpu (ncpu) — Java Panama FFI (libacncpu.so / libacncpu.dll)
//
// Only the dotted `ncpu.dha(...)`/`ncpu.abort()`/etc surface is wired here. The
// carried-over bare `ptr_new`/`ptr_deref`/... calls have a pre-existing architectural
// gap on this backend: ilib FFI content is injected as a SEPARATE top-level class
// (Java has no free functions), so an unqualified `ptr_new(x)` inside the generated
// program's main class can't resolve to another class's static method without a
// compiler-side rewrite to `ncpu.ptr_new(x)` — a Java-codegen change, not an FFI-file
// fix, and out of scope here (the same gap pre-dates this rename; not made worse).
import java.lang.foreign.*;
import java.lang.invoke.*;
import java.nio.file.*;

final class AcNcpu {
    private static final Linker _L = Linker.nativeLinker();
    private static final SymbolLookup _SYM;
    // AC_PATH-aware resolution — see camera_ffi.java's _resolveLib for why the plain
    // user.dir-relative path broke whenever `ac` ran from anywhere but the project root.
    private static Path _resolveLib(String rel) {
        Path relP = Path.of("library", "ilib", "native-cpu", rel);
        String acp = System.getenv("AC_PATH");
        if (acp != null) {
            Path cand = Path.of(acp).resolve(relP);
            if (Files.exists(cand)) return cand.toAbsolutePath();
        }
        Path cwdCand = Path.of(".").resolve(relP);
        if (Files.exists(cwdCand)) return cwdCand.toAbsolutePath();
        try {
            Path self = Path.of(AcNcpu.class.getProtectionDomain().getCodeSource().getLocation().toURI());
            Path base = Files.isDirectory(self) ? self : self.getParent();
            Path fromClass = base.resolve("..").resolve(relP).normalize();
            if (Files.exists(fromClass)) return fromClass.toAbsolutePath();
        } catch (Exception ignored) {}
        return cwdCand.toAbsolutePath();
    }
    static {
        String _os = System.getProperty("os.name").toLowerCase();
        String _libFile = _os.contains("win") ? "libacncpu.dll" : "libacncpu.so";
        Path _libPath = _resolveLib(_libFile);
        _SYM = SymbolLookup.libraryLookup(_libPath, Arena.global());
    }
    private static MethodHandle _mh(String name, FunctionDescriptor fd) {
        return _L.downcallHandle(_SYM.find(name).orElseThrow(), fd);
    }
    private static final ValueLayout.OfInt  I  = ValueLayout.JAVA_INT;
    private static final ValueLayout.OfLong L8 = ValueLayout.JAVA_LONG;
    private static final AddressLayout      P  = ValueLayout.ADDRESS;

    private static final MethodHandle _ptr_new      = _mh("ac_ncpu_ptr_new",       FunctionDescriptor.of(L8,P,L8,L8));
    private static final MethodHandle _ptr_deref    = _mh("ac_ncpu_ptr_deref",     FunctionDescriptor.of(L8,L8,P,L8));
    private static final MethodHandle _ptr_is_null  = _mh("ac_ncpu_ptr_is_null",   FunctionDescriptor.of(I,L8));
    private static final MethodHandle _ptr_null     = _mh("ac_ncpu_ptr_null",      FunctionDescriptor.of(L8));
    private static final MethodHandle _ptr_eq       = _mh("ac_ncpu_ptr_eq",        FunctionDescriptor.of(I,L8,L8));
    private static final MethodHandle _ptr_copy     = _mh("ac_ncpu_ptr_copy",      FunctionDescriptor.of(L8,L8));
    private static final MethodHandle _ptr_update   = _mh("ac_ncpu_ptr_update",    FunctionDescriptor.of(I,L8,P,L8));
    private static final MethodHandle _ptr_free     = _mh("ac_ncpu_ptr_free",      FunctionDescriptor.of(I,L8));

    private static final MethodHandle _dha          = _mh("ac_ncpu_dha",           FunctionDescriptor.of(L8,L8,L8));
    private static final MethodHandle _zdha         = _mh("ac_ncpu_zdha",          FunctionDescriptor.of(L8,L8,L8));
    private static final MethodHandle _realloc      = _mh("ac_ncpu_realloc",       FunctionDescriptor.of(L8,L8,L8));
    private static final MethodHandle _bmdha        = _mh("ac_ncpu_bmdha",         FunctionDescriptor.of(L8,L8,L8));
    private static final MethodHandle _bmzdha       = _mh("ac_ncpu_bmzdha",        FunctionDescriptor.of(L8,L8,L8));
    private static final MethodHandle _bmrealloc    = _mh("ac_ncpu_bmrealloc",     FunctionDescriptor.of(L8,L8,L8));

    private static final MethodHandle _arena_create  = _mh("ac_ncpu_arena_create", FunctionDescriptor.of(L8,L8));
    private static final MethodHandle _arena_alloc   = _mh("ac_ncpu_arena_alloc",  FunctionDescriptor.of(L8,L8,L8));
    private static final MethodHandle _arena_dealloc = _mh("ac_ncpu_arena_dealloc",FunctionDescriptor.of(I,L8,L8));
    private static final MethodHandle _arena_destroy = _mh("ac_ncpu_arena_destroy",FunctionDescriptor.of(I,L8));
    private static final MethodHandle _arena_abort   = _mh("ac_ncpu_arena_abort",  FunctionDescriptor.of(I,L8));

    private static final MethodHandle _abort     = _mh("ac_ncpu_abort",     FunctionDescriptor.ofVoid());
    private static final MethodHandle _broadcast = _mh("ac_ncpu_broadcast", FunctionDescriptor.ofVoid(P));
    private static final MethodHandle _recieve   = _mh("ac_ncpu_recieve",   FunctionDescriptor.of(I,P));

    private static final Arena _arena = Arena.ofAuto();
    @SuppressWarnings("preview")
    private static MemorySegment _cs(String s) { return _arena.allocateUtf8String(s); }

    // AC source passes ptr_new/ptr_update a plain value expression (usually a string literal) —
    // not a manually-built MemorySegment — so these take String and convert internally. The raw
    // MemorySegment overloads stay available for direct Java callers that already have one.
    static long ptrNew(String value, long size, long typeId) { return ptrNew(_cs(value), size, typeId); }
    static long ptrNew(MemorySegment value, long size, long typeId) { try { return (long)_ptr_new.invoke(value, size, typeId); } catch (Throwable t) { throw new RuntimeException(t); } }
    static long ptrDeref(long ptrId, MemorySegment out, long outSize) { try { return (long)_ptr_deref.invoke(ptrId, out, outSize); } catch (Throwable t) { throw new RuntimeException(t); } }
    static int  ptrIsNull(long ptrId)  { try { return (int)_ptr_is_null.invoke(ptrId); } catch (Throwable t) { throw new RuntimeException(t); } }
    static long ptrNull()              { try { return (long)_ptr_null.invoke(); } catch (Throwable t) { throw new RuntimeException(t); } }
    static int  ptrEq(long a, long b)  { try { return (int)_ptr_eq.invoke(a, b); } catch (Throwable t) { throw new RuntimeException(t); } }
    static long ptrCopy(long ptrId)    { try { return (long)_ptr_copy.invoke(ptrId); } catch (Throwable t) { throw new RuntimeException(t); } }
    static int  ptrUpdate(long ptrId, String value, long size) { return ptrUpdate(ptrId, _cs(value), size); }
    static int  ptrUpdate(long ptrId, MemorySegment value, long size) { try { return (int)_ptr_update.invoke(ptrId, value, size); } catch (Throwable t) { throw new RuntimeException(t); } }
    static int  ptrFree(long ptrId)    { try { return (int)_ptr_free.invoke(ptrId); } catch (Throwable t) { throw new RuntimeException(t); } }

    static long dha(long size, long typeId)         { try { return (long)_dha.invoke(size, typeId); } catch (Throwable t) { throw new RuntimeException(t); } }
    static long zdha(long size, long typeId)        { try { return (long)_zdha.invoke(size, typeId); } catch (Throwable t) { throw new RuntimeException(t); } }
    static long ncpuRealloc(long ptrId, long size)  { try { return (long)_realloc.invoke(ptrId, size); } catch (Throwable t) { throw new RuntimeException(t); } }
    static long bmdha(long size, long typeId)       { try { return (long)_bmdha.invoke(size, typeId); } catch (Throwable t) { throw new RuntimeException(t); } }
    static long bmzdha(long size, long typeId)      { try { return (long)_bmzdha.invoke(size, typeId); } catch (Throwable t) { throw new RuntimeException(t); } }
    static long bmrealloc(long ptrId, long size)    { try { return (long)_bmrealloc.invoke(ptrId, size); } catch (Throwable t) { throw new RuntimeException(t); } }

    static long arenaCreate(long size)                 { try { return (long)_arena_create.invoke(size); } catch (Throwable t) { throw new RuntimeException(t); } }
    static long arenaAlloc(long arenaId, long size)    { try { return (long)_arena_alloc.invoke(arenaId, size); } catch (Throwable t) { throw new RuntimeException(t); } }
    static int  arenaDealloc(long arenaId, long ptrId) { try { return (int)_arena_dealloc.invoke(arenaId, ptrId); } catch (Throwable t) { throw new RuntimeException(t); } }
    static int  arenaDestroy(long arenaId)             { try { return (int)_arena_destroy.invoke(arenaId); } catch (Throwable t) { throw new RuntimeException(t); } }
    static int  arenaAbort(long arenaId)               { try { return (int)_arena_abort.invoke(arenaId); } catch (Throwable t) { throw new RuntimeException(t); } }

    static void ncpuAbort()         { try { _abort.invoke(); } catch (Throwable t) { throw new RuntimeException(t); } }
    static void broadcast(String m) { try { _broadcast.invoke(_cs(m)); } catch (Throwable t) { throw new RuntimeException(t); } }
    static int  recieve(String m)   { try { return (int)_recieve.invoke(_cs(m)); } catch (Throwable t) { throw new RuntimeException(t); } }
}

// AC-facing dotted namespace: `ncpu.dha(...)`, `ncpu.abort()`, etc. (Simple value-typed
// signatures only — buffer-taking overloads for ptr_new/ptr_deref/ptr_update aren't part
// of the dotted surface since AC's scalar call convention doesn't expose raw addresses
// at this layer; those stay on the internal AcNcpu handles above.)
class ncpu {
    static long dha(long size, long typeId)         { return AcNcpu.dha(size, typeId); }
    static long zdha(long size, long typeId)        { return AcNcpu.zdha(size, typeId); }
    static long realloc(long ptrId, long size)      { return AcNcpu.ncpuRealloc(ptrId, size); }
    static long bmdha(long size, long typeId)       { return AcNcpu.bmdha(size, typeId); }
    static long bmzdha(long size, long typeId)      { return AcNcpu.bmzdha(size, typeId); }
    static long bmrealloc(long ptrId, long size)    { return AcNcpu.bmrealloc(ptrId, size); }
    static long arena_create(long size)                 { return AcNcpu.arenaCreate(size); }
    static long arena_alloc(long arenaId, long size)    { return AcNcpu.arenaAlloc(arenaId, size); }
    static int  arena_dealloc(long arenaId, long ptrId) { return AcNcpu.arenaDealloc(arenaId, ptrId); }
    static int  arena_destroy(long arenaId)             { return AcNcpu.arenaDestroy(arenaId); }
    static int  arena_abort(long arenaId)               { return AcNcpu.arenaAbort(arenaId); }
    static void abort()               { AcNcpu.ncpuAbort(); }
    static void broadcast(String m)   { AcNcpu.broadcast(m); }
    static int  recieve(String m)     { return AcNcpu.recieve(m); }
}
