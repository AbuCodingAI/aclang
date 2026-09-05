// AC ilib: aczip — Java Panama FFI (libaczip.so / libaczip.dll)
// AC has no raw byte-buffer type, so this binding calls the file-to-file convenience
// functions in aczip_c.h/.cpp (shared with every other backend's FFI — see that file's
// own comment) instead of marshaling ACZipByteArray by hand.
import java.lang.foreign.*;
import java.lang.invoke.*;
import java.nio.file.*;

final class AcAczip {
    private static final Linker _L = Linker.nativeLinker();
    private static final SymbolLookup _SYM;
    // AC_PATH-aware resolution — see camera_ffi.java's _resolveLib for why the plain
    // user.dir-relative path broke whenever `ac` ran from anywhere but the project root.
    private static Path _resolveLib(String rel) {
        Path relP = Path.of("library", "ilib", "aczip", rel);
        String acp = System.getenv("AC_PATH");
        if (acp != null) {
            Path cand = Path.of(acp).resolve(relP);
            if (Files.exists(cand)) return cand.toAbsolutePath();
        }
        Path cwdCand = Path.of(".").resolve(relP);
        if (Files.exists(cwdCand)) return cwdCand.toAbsolutePath();
        try {
            Path self = Path.of(AcAczip.class.getProtectionDomain().getCodeSource().getLocation().toURI());
            Path base = Files.isDirectory(self) ? self : self.getParent();
            Path fromClass = base.resolve("..").resolve(relP).normalize();
            if (Files.exists(fromClass)) return fromClass.toAbsolutePath();
        } catch (Exception ignored) {}
        return cwdCand.toAbsolutePath();
    }
    static {
        String _os = System.getProperty("os.name").toLowerCase();
        String _libFile = _os.contains("win") ? "aczip.dll" : "libaczip.so";
        _SYM = SymbolLookup.libraryLookup(_resolveLib(_libFile), Arena.global());
    }
    private static MethodHandle _mh(String name, FunctionDescriptor fd) {
        return _L.downcallHandle(_SYM.find(name).orElseThrow(), fd);
    }
    private static final ValueLayout.OfLong   L8 = ValueLayout.JAVA_LONG;
    private static final ValueLayout.OfInt    I  = ValueLayout.JAVA_INT;
    private static final ValueLayout.OfDouble D  = ValueLayout.JAVA_DOUBLE;
    private static final AddressLayout        P  = ValueLayout.ADDRESS;

    private static final MethodHandle _compress   = _mh("ac_zip_compress_to_file",     FunctionDescriptor.of(L8, P, I, P));
    private static final MethodHandle _decompress = _mh("ac_zip_decompress_from_file", FunctionDescriptor.of(I, P, P));
    private static final MethodHandle _ratio      = _mh("ac_get_compression_ratio",    FunctionDescriptor.of(D, L8, L8));

    private static final Arena _arena = Arena.ofAuto();
    private static MemorySegment _cs(String s) { return _arena.allocateUtf8String(s); }

    static long compress(String path, long parallel, String outputPath) {
        try { return (long)_compress.invoke(_cs(path), (int)(parallel != 0 ? 1 : 0), _cs(outputPath)); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }
    static long decompress(String archivePath, String outputPath) {
        try { return (long)(int)_decompress.invoke(_cs(archivePath), _cs(outputPath)); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }
    static double get_ratio(long original, long compressed) {
        try { return (double)_ratio.invoke(original, compressed); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }
}

// Namespace class — AC-generated Java uses aczip.compress(...), aczip.decompress(...)
class aczip {
    public static long compress(String path, long parallel, String outputPath) { return AcAczip.compress(path, parallel, outputPath); }
    public static long decompress(String archivePath, String outputPath)       { return AcAczip.decompress(archivePath, outputPath); }
    public static double get_ratio(long original, long compressed)             { return AcAczip.get_ratio(original, compressed); }
}
