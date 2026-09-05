// AC ilib: os — Java FFI
// Shell command execution with safety checks

import java.io.*;
import java.nio.file.*;
import java.util.regex.*;

class OSLib {

    private static final String[] SBASH_FORBIDDEN = {
        "\\bsudo\\b", "\\bsu\\s", "function\\s+\\w+\\s*\\(",
        "\\(\\s*\\)\\s*\\{", "&\\s*$", "&\\s+\\w", "\\bnohup\\b",
        "\\bscreen\\b", "\\btmux\\b", "\\bdisown\\b"
    };

    /**
     * os_bash - Execute shell command (unrestricted)
     * @param cmd Command to execute
     * @return Exit code (0 = success)
     */
    public static int osBash(String cmd) {
        if (cmd == null || cmd.isEmpty()) return -1;

        try {
            ProcessBuilder pb = new ProcessBuilder("/bin/sh", "-c", cmd);
            pb.inheritIO();
            Process p = pb.start();
            return p.waitFor();
        } catch (Exception e) {
            System.err.println("[os.bash] " + e.getMessage());
            return -1;
        }
    }

    /**
     * os_sbash - Execute shell command (restricted/safe)
     * Blocks: sudo, su, function definitions, nohup, tmux, screen, disown, background execution
     * @param cmd Command to execute
     * @return Exit code (0 = success, -1 = blocked)
     */
    public static int osSbash(String cmd) {
        if (cmd == null || cmd.isEmpty()) return -1;

        // Check forbidden patterns
        for (String pattern : SBASH_FORBIDDEN) {
            if (Pattern.compile(pattern).matcher(cmd).find()) {
                System.err.println("[os.sbash] The p in bash stands for protection (blocked: " + pattern + ")");
                return -1;
            }
        }

        try {
            ProcessBuilder pb = new ProcessBuilder("/bin/sh", "-c", cmd);
            pb.inheritIO();
            Process p = pb.start();
            return p.waitFor();
        } catch (Exception e) {
            System.err.println("[os.sbash] " + e.getMessage());
            return -1;
        }
    }

    /**
     * os_app_open - Open application or file
     * @param app Application or file path
     * @return 0 on success
     */
    public static int osAppOpen(String app) {
        if (app == null || app.isEmpty()) return -1;

        String[] launchers = {"xdg-open", "open", "start"};

        for (String launcher : launchers) {
            try {
                ProcessBuilder pb = new ProcessBuilder(launcher, app);
                pb.start();
                return 0;
            } catch (Exception e) {
                // Try next launcher
            }
        }

        // Fallback: try to execute directly
        try {
            Runtime.getRuntime().exec(app);
            return 0;
        } catch (Exception e) {
            System.err.println("[os.app_open] " + e.getMessage());
            return -1;
        }
    }

    /**
     * os_mkfile - Create an empty file
     * @param path File path
     * @return 0 on success, -1 on error
     */
    public static int osMkfile(String path) {
        if (path == null || path.isEmpty()) return -1;

        try {
            Path p = Paths.get(path);
            if (!Files.exists(p)) {
                Files.createFile(p);
            }
            return 0;
        } catch (IOException e) {
            System.err.println("[os.mkfile] " + e.getMessage());
            return -1;
        }
    }

    /**
     * os_rmfile - Delete a file
     * @param path File path
     * @return 0 on success, -1 on error
     */
    public static int osRmfile(String path) {
        if (path == null || path.isEmpty()) return -1;

        try {
            Files.deleteIfExists(Paths.get(path));
            return 0;
        } catch (IOException e) {
            System.err.println("[os.rmfile] " + e.getMessage());
            return -1;
        }
    }

    /**
     * os_mkdir - Create a directory
     * @param path Directory path
     * @return 0 on success, -1 on error
     */
    public static int osMkdir(String path) {
        if (path == null || path.isEmpty()) return -1;

        try {
            Files.createDirectories(Paths.get(path));
            return 0;
        } catch (IOException e) {
            System.err.println("[os.mkdir] " + e.getMessage());
            return -1;
        }
    }

    /**
     * os_rmdir - Remove a directory (recursively)
     * @param path Directory path
     * @return 0 on success, -1 on error
     */
    public static int osRmdir(String path) {
        if (path == null || path.isEmpty()) return -1;

        try {
            Files.walk(Paths.get(path))
                .sorted(java.util.Comparator.reverseOrder())
                .forEach(p -> {
                    try {
                        Files.delete(p);
                    } catch (IOException e) {
                        // Continue on error
                    }
                });
            return 0;
        } catch (IOException e) {
            System.err.println("[os.rmdir] " + e.getMessage());
            return -1;
        }
    }

    /**
     * os_copy - Copy file
     * @param src Source path
     * @param dst Destination path
     * @return 0 on success, -1 on error
     */
    public static int osCopy(String src, String dst) {
        if (src == null || dst == null || src.isEmpty() || dst.isEmpty()) return -1;

        try {
            Files.copy(Paths.get(src), Paths.get(dst),
                java.nio.file.StandardCopyOption.REPLACE_EXISTING);
            return 0;
        } catch (IOException e) {
            System.err.println("[os.copy] " + e.getMessage());
            return -1;
        }
    }

    /**
     * os_move - Move/rename file
     * @param src Source path
     * @param dst Destination path
     * @return 0 on success, -1 on error
     */
    public static int osMove(String src, String dst) {
        if (src == null || dst == null || src.isEmpty() || dst.isEmpty()) return -1;

        try {
            Files.move(Paths.get(src), Paths.get(dst),
                java.nio.file.StandardCopyOption.REPLACE_EXISTING);
            return 0;
        } catch (IOException e) {
            System.err.println("[os.move] " + e.getMessage());
            return -1;
        }
    }

    /**
     * os_exists - Check if file/directory exists
     * @param path Path to check
     * @return 1 if exists, 0 otherwise
     */
    public static int osExists(String path) {
        if (path == null || path.isEmpty()) return 0;
        return Files.exists(Paths.get(path)) ? 1 : 0;
    }

    /**
     * os_isfile - Check if path is a file
     * @param path Path to check
     * @return 1 if is file, 0 otherwise
     */
    public static int osIsfile(String path) {
        if (path == null || path.isEmpty()) return 0;
        return Files.isRegularFile(Paths.get(path)) ? 1 : 0;
    }

    /**
     * os_isdir - Check if path is a directory
     * @param path Path to check
     * @return 1 if is directory, 0 otherwise
     */
    public static int osIsdir(String path) {
        if (path == null || path.isEmpty()) return 0;
        return Files.isDirectory(Paths.get(path)) ? 1 : 0;
    }

    /**
     * os_getwd - Get current working directory
     * @return Current working directory path
     */
    public static String osGetwd() {
        return System.getProperty("user.dir");
    }

    /**
     * os_chdir - Change current working directory
     * @param path New directory path
     * @return 0 on success (simulated), -1 on error
     */
    public static int osChdir(String path) {
        if (path == null || path.isEmpty()) return -1;

        try {
            String os_name = System.getProperty("os.name").toLowerCase();
            if (os_name.contains("win")) {
                // Windows: use cmd /c cd
                ProcessBuilder pb = new ProcessBuilder("cmd", "/c", "cd", path);
                return pb.start().waitFor();
            } else {
                // Unix: use sh -c cd
                ProcessBuilder pb = new ProcessBuilder("sh", "-c", "cd " + path);
                return pb.start().waitFor();
            }
        } catch (Exception e) {
            System.err.println("[os.chdir] " + e.getMessage());
            return -1;
        }
    }

    // os_env / os_write_to / os_append_to / os_read: declared in os.acl but never implemented
    // here at all (Java's OSLib had 14 methods, os.acl's surface needs 4 more) — matches
    // Python's os_ffi.py semantics (see its os_env/os_write_to/os_append_to/os_read).
    public static String osEnv(String key) {
        String v = System.getenv(key);
        return v == null ? "" : v;
    }

    public static int osWriteTo(String path, String content) {
        try (java.io.FileWriter w = new java.io.FileWriter(path)) {
            w.write(content);
            return 0;
        } catch (java.io.IOException e) {
            System.err.println("[os.write_to] " + e.getMessage());
            return -1;
        }
    }

    public static int osAppendTo(String path, String content) {
        try (java.io.FileWriter w = new java.io.FileWriter(path, true)) {
            w.write(content.endsWith("\n") ? content : content + "\n");
            return 0;
        } catch (java.io.IOException e) {
            System.err.println("[os.append_to] " + e.getMessage());
            return -1;
        }
    }

    public static String osRead(String path) {
        try {
            return new String(java.nio.file.Files.readAllBytes(java.nio.file.Paths.get(path)));
        } catch (java.io.IOException e) {
            System.err.println("[os.read] " + e.getMessage());
            return "";
        }
    }
}

// os namespace class — AC-generated Java uses os.exists(path), etc. Missing entirely before
// (verified: examples/os_demo.ac, "cannot find symbol: variable os" — mirrors regex's own
// `class regex {...}` dispatcher, which OSLib never had at all). Names match os.acl's
// os:* -> os.* mapping.
class os {
    public static int    bash(String cmd)                 { return OSLib.osBash(cmd); }
    public static int    sbash(String cmd)                { return OSLib.osSbash(cmd); }
    public static int    app_open(String app)              { return OSLib.osAppOpen(app); }
    public static int    mkfile(String path)               { return OSLib.osMkfile(path); }
    public static int    rmfile(String path)               { return OSLib.osRmfile(path); }
    public static int    mkdir(String path)                { return OSLib.osMkdir(path); }
    public static int    rmdir(String path)                { return OSLib.osRmdir(path); }
    public static int    exists(String path)                { return OSLib.osExists(path); }
    public static String cwd()                              { return OSLib.osGetwd(); }
    public static String env(String key)                    { return OSLib.osEnv(key); }
    public static int    write_to(String path, String c)    { return OSLib.osWriteTo(path, c); }
    public static int    append_to(String path, String c)   { return OSLib.osAppendTo(path, c); }
    public static String read(String path)                  { return OSLib.osRead(path); }
}
