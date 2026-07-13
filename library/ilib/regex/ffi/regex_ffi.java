// AC ilib: regex — Java Panama FFI (libacregex.so / acregex.dll)
// Requires: Java 22+, --enable-preview, java.lang.foreign module
import java.lang.foreign.*;
import java.lang.invoke.*;
import java.nio.file.*;
import java.util.*;

final class AcRegex {
    private static final Linker _L = Linker.nativeLinker();
    private static final SymbolLookup _SYM;
    static {
        String _os = System.getProperty("os.name").toLowerCase();
        String _lib = _os.contains("win") ? "acregex.dll" : "libacregex.so";
        Path _p = Path.of(System.getProperty("user.dir"))
            .resolve("library/regex/" + _lib).toAbsolutePath();
        _SYM = SymbolLookup.libraryLookup(_p, Arena.global());
    }
    private static MethodHandle _mh(String n, FunctionDescriptor fd) {
        return _L.downcallHandle(_SYM.find(n).orElseThrow(), fd);
    }
    private static final ValueLayout.OfInt I = ValueLayout.JAVA_INT;
    private static final AddressLayout    A = ValueLayout.ADDRESS;

    private static final MethodHandle _match       = _mh("ac_regex_match",       FunctionDescriptor.of(I,A,A));
    private static final MethodHandle _test        = _mh("ac_regex_test",        FunctionDescriptor.of(I,A,A));
    private static final MethodHandle _search      = _mh("ac_regex_search",      FunctionDescriptor.of(A,A,A));
    private static final MethodHandle _replace     = _mh("ac_regex_replace",     FunctionDescriptor.of(A,A,A,A));
    private static final MethodHandle _replaceAll  = _mh("ac_regex_replace_all", FunctionDescriptor.of(A,A,A,A));
    private static final MethodHandle _count       = _mh("ac_regex_count",       FunctionDescriptor.of(I,A,A));
    private static final MethodHandle _escape      = _mh("ac_regex_escape",      FunctionDescriptor.of(A,A));
    private static final MethodHandle _findAll     = _mh("ac_regex_find_all",    FunctionDescriptor.of(A,A,A,A));
    private static final MethodHandle _split       = _mh("ac_regex_split",       FunctionDescriptor.of(A,A,A,A));
    private static final MethodHandle _groups      = _mh("ac_regex_groups",      FunctionDescriptor.of(A,A,A,A));
    private static final MethodHandle _freeList    = _mh("ac_regex_free_list",   FunctionDescriptor.ofVoid(A,I));

    private static int    _i(MethodHandle h, Object... a) {
        try { return (int)h.invokeWithArguments(a); } catch(Throwable t){throw new RuntimeException(t);}
    }
    private static MemorySegment _a(MethodHandle h, Object... a) {
        try { return (MemorySegment)h.invokeWithArguments(a); } catch(Throwable t){throw new RuntimeException(t);}
    }
    private static void _v(MethodHandle h, Object... a) {
        try { h.invokeWithArguments(a); } catch(Throwable t){throw new RuntimeException(t);}
    }

    private static String _str(MemorySegment seg) {
        if (seg == null || seg.equals(MemorySegment.NULL)) return "";
        return seg.reinterpret(Long.MAX_VALUE).getString(0);
    }

    private static List<String> _list(Arena ar, MemorySegment arr, int n) {
        List<String> r = new ArrayList<>(n);
        for (int i = 0; i < n; i++) {
            MemorySegment p = arr.reinterpret(Long.MAX_VALUE).getAtIndex(A, i);
            r.add(_str(p));
        }
        _v(_freeList, arr, n);
        return r;
    }

    public static boolean match(String s, String p) {
        try (Arena a = Arena.ofConfined()) {
            return _i(_match, a.allocateFrom(s), a.allocateFrom(p)) != 0;
        }
    }
    public static boolean test(String s, String p) {
        try (Arena a = Arena.ofConfined()) {
            return _i(_test, a.allocateFrom(s), a.allocateFrom(p)) != 0;
        }
    }
    public static String search(String s, String p) {
        try (Arena a = Arena.ofConfined()) {
            return _str(_a(_search, a.allocateFrom(s), a.allocateFrom(p)));
        }
    }
    public static String replace(String s, String p, String r) {
        try (Arena a = Arena.ofConfined()) {
            return _str(_a(_replace, a.allocateFrom(s), a.allocateFrom(p), a.allocateFrom(r)));
        }
    }
    public static String replaceAll(String s, String p, String r) {
        try (Arena a = Arena.ofConfined()) {
            return _str(_a(_replaceAll, a.allocateFrom(s), a.allocateFrom(p), a.allocateFrom(r)));
        }
    }
    public static int count(String s, String p) {
        try (Arena a = Arena.ofConfined()) {
            return _i(_count, a.allocateFrom(s), a.allocateFrom(p));
        }
    }
    public static String escape(String s) {
        try (Arena a = Arena.ofConfined()) {
            return _str(_a(_escape, a.allocateFrom(s)));
        }
    }
    public static List<String> findAll(String s, String p) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment nSeg = a.allocate(I);
            MemorySegment arr  = _a(_findAll, a.allocateFrom(s), a.allocateFrom(p), nSeg);
            return _list(a, arr, nSeg.get(I, 0));
        }
    }
    public static List<String> split(String s, String p) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment nSeg = a.allocate(I);
            MemorySegment arr  = _a(_split, a.allocateFrom(s), a.allocateFrom(p), nSeg);
            return _list(a, arr, nSeg.get(I, 0));
        }
    }
    public static List<String> groups(String s, String p) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment nSeg = a.allocate(I);
            MemorySegment arr  = _a(_groups, a.allocateFrom(s), a.allocateFrom(p), nSeg);
            return _list(a, arr, nSeg.get(I, 0));
        }
    }

    // AC-name aliases
    public static boolean    regex_match(String s, String p)             { return match(s, p);       }
    public static boolean    regex_test(String s, String p)              { return test(s, p);        }
    public static String     regex_search(String s, String p)            { return search(s, p);      }
    public static String     regex_replace(String s, String p, String r) { return replace(s, p, r);  }
    public static String     regex_replace_all(String s,String p,String r){ return replaceAll(s,p,r);}
    public static int        regex_count(String s, String p)             { return count(s, p);       }
    public static String     regex_escape(String s)                      { return escape(s);         }
    public static List<String> regex_find_all(String s, String p)        { return findAll(s, p);     }
    public static List<String> regex_split(String s, String p)           { return split(s, p);       }
    public static List<String> regex_groups(String s, String p)          { return groups(s, p);      }
}

// regex namespace class — AC-generated Java uses regex.match(s, p), etc.
class regex {
    public static boolean    match(String s, String p)              { return AcRegex.match(s, p);       }
    public static boolean    test(String s, String p)               { return AcRegex.test(s, p);        }
    public static String     search(String s, String p)             { return AcRegex.search(s, p);      }
    public static String     replace(String s, String p, String r)  { return AcRegex.replace(s, p, r);  }
    public static String     replace_all(String s, String p, String r){ return AcRegex.replaceAll(s,p,r);}
    public static int        count(String s, String p)              { return AcRegex.count(s, p);       }
    public static String     escape(String s)                       { return AcRegex.escape(s);         }
    public static List<String> find_all(String s, String p)         { return AcRegex.findAll(s, p);     }
    public static List<String> split(String s, String p)            { return AcRegex.split(s, p);       }
    public static List<String> groups(String s, String p)           { return AcRegex.groups(s, p);      }
}
