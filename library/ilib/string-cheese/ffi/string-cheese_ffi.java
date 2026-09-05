// AC ilib: string-cheese — Java implementation
import java.util.*;
import java.util.regex.*;

class StringCheese {

    private static final String _WS = " \t\n\r";

    private static boolean isWsSentinel(String s) {
        return s.equals(_WS);
    }

    /**
     * stringm_lower - Convert to lowercase
     */
    public static String lower(String s) {
        return s == null ? "" : s.toLowerCase();
    }

    /**
     * stringm_upper - Convert to uppercase
     */
    public static String upper(String s) {
        return s == null ? "" : s.toUpperCase();
    }

    /**
     * stringm_trim - Trim whitespace
     */
    public static String trim(String s) {
        return s == null ? "" : s.trim();
    }

    /**
     * stringm_strip - Strip characters from ends
     */
    public static String strip(String s, String chars) {
        if (s == null) return "";
        if (chars == null || isWsSentinel(chars)) {
            return s.trim();
        }

        int start = 0, end = s.length();
        while (start < end && chars.indexOf(s.charAt(start)) >= 0) start++;
        while (end > start && chars.indexOf(s.charAt(end - 1)) >= 0) end--;

        return s.substring(start, end);
    }

    /**
     * stringm_find - Find pattern position
     */
    public static long find(String s, String pattern) {
        if (s == null || pattern == null) return -1;

        if (isWsSentinel(pattern)) {
            for (int i = 0; i < s.length(); i++) {
                if (Character.isWhitespace(s.charAt(i))) {
                    return i;
                }
            }
            return -1;
        }

        int idx = s.indexOf(pattern);
        return idx >= 0 ? idx : -1;
    }

    /**
     * stringm_replace - Replace pattern
     */
    public static String replace(String s, String old, String newStr) {
        if (s == null || old == null) return s == null ? "" : s;

        if (isWsSentinel(old)) {
            String[] parts = s.trim().split("\\s+");
            return String.join(newStr == null ? " " : newStr, parts);
        }

        return s.replace(old, newStr == null ? "" : newStr);
    }

    /**
     * stringm_split - Split string
     */
    public static String[] split(String s, String sep) {
        if (s == null) return new String[]{};

        if (sep == null || isWsSentinel(sep)) {
            return s.trim().split("\\s+");
        }

        return s.split(Pattern.quote(sep), -1);
    }

    /**
     * stringm_split_nth - Get nth split part
     */
    public static String splitNth(String s, String sep, int n) {
        String[] parts = split(s, sep);
        if (n >= 0 && n < parts.length) {
            return parts[n];
        }
        return "";
    }

    /**
     * stringm_len - String length
     */
    public static long len(String s) {
        return s == null ? 0 : s.length();
    }

    /**
     * stringm_startswith - Check prefix
     */
    public static boolean startsWith(String s, String prefix) {
        return s != null && prefix != null && s.startsWith(prefix);
    }

    /**
     * stringm_endswith - Check suffix
     */
    public static boolean endsWith(String s, String suffix) {
        return s != null && suffix != null && s.endsWith(suffix);
    }

    /**
     * stringm_count - Count occurrences
     */
    public static long count(String s, String sub) {
        if (s == null || sub == null) return 0;

        if (isWsSentinel(sub)) {
            int cnt = 0;
            for (char c : s.toCharArray()) {
                if (Character.isWhitespace(c)) cnt++;
            }
            return cnt;
        }

        int count = 0;
        int idx = 0;
        while ((idx = s.indexOf(sub, idx)) >= 0) {
            count++;
            idx += sub.length();
        }
        return count;
    }

    /**
     * stringm_join - Join strings
     */
    public static String join(String sep, String... parts) {
        if (sep == null) sep = "";
        return String.join(sep, parts);
    }

    /**
     * stringm_format - Format string (passthrough)
     */
    public static String format(String t) {
        return t == null ? "" : t;
    }

    /**
     * stringm_ischar - Check if all alphabetic
     */
    public static boolean isChar(String s) {
        if (s == null || s.isEmpty()) return false;
        for (char c : s.toCharArray()) {
            if (!Character.isLetter(c)) return false;
        }
        return true;
    }

    /**
     * stringm_isws - Check if all whitespace
     */
    public static boolean isWS(String s) {
        if (s == null) return false;
        if (s.isEmpty()) return true;
        for (char c : s.toCharArray()) {
            if (!Character.isWhitespace(c)) return false;
        }
        return true;
    }

    /**
     * stringm_getline - Read line from stdin (stub)
     */
    public static String getline() {
        try {
            Scanner sc = new Scanner(System.in);
            return sc.hasNextLine() ? sc.nextLine() : "";
        } catch (Exception e) {
            return "";
        }
    }

    /**
     * stringm_scan - Scan stdin for pattern (stub)
     */
    public static boolean scan(String needle) {
        try {
            Scanner sc = new Scanner(System.in);
            while (sc.hasNextLine()) {
                if (sc.nextLine().contains(needle)) {
                    return true;
                }
            }
        } catch (Exception e) {
        }
        return false;
    }
}

// stringm namespace class — AC-generated Java uses stringm.upper(s), etc. Missing entirely
// before (verified: examples/stringm_demo.ac, "cannot find symbol: variable stringm" — every
// other backend resolves `stringm.*` through a native object/namespace of some kind; Java had
// the real StringCheese implementation but no dispatcher exposing it under the AC-level name).
// Names/arities mirror string-cheese.acl's stringm:* -> stringm.* mapping.
class stringm {
    public static String b(String s)                       { return StringCheese.trim(s); }
    public static String upper(String s)                   { return StringCheese.upper(s); }
    public static String lower(String s)                   { return StringCheese.lower(s); }
    public static long   find(String s, String pattern)    { return StringCheese.find(s, pattern); }
    public static String strip(String s, String chars)     { return StringCheese.strip(s, chars); }
    public static String strip(String s)                   { return StringCheese.strip(s, " \t\n\r"); }
    public static String replace(String s, String o, String n) { return StringCheese.replace(s, o, n); }
    public static String[] split(String s, String sep)     { return StringCheese.split(s, sep); }
    public static String splitNth(String s, String sep, int n) { return StringCheese.splitNth(s, sep, n); }
    public static String join(String sep, String... parts) { return StringCheese.join(sep, parts); }
    public static long   length(String s)                  { return StringCheese.len(s); }
    public static boolean startswith(String s, String p)   { return StringCheese.startsWith(s, p); }
    public static boolean endswith(String s, String p)     { return StringCheese.endsWith(s, p); }
    public static long   count(String s, String sub)       { return StringCheese.count(s, sub); }
    public static String format(String t)                  { return StringCheese.format(t); }
    public static String getline()                         { return StringCheese.getline(); }
    public static boolean scan(String needle)               { return StringCheese.scan(needle); }
}
