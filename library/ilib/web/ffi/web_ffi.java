import java.awt.Desktop;
import java.net.URI;

class WebLib {
    public static void open(String link) {
        openURL("https://" + link);
    }

    public static void fileOpen(String file) {
        openURL("file://" + file);
    }

    public static void popen(String rawLink) {
        openURL(rawLink);
    }

    public static void ropen(String identifier) {
        openURL("https://" + identifier + ".com");
    }

    public static void browser() {
        openURL("https://www.google.com");
    }

    public static boolean pdf(String pdf) {
        if (pdf.endsWith(".pdf")) {
            fileOpen(pdf);
            return true;
        }
        return false;
    }

    public static boolean text(String text) {
        String[] validExts = {".txt", ".md", ".bashrc", ".zshrc"};
        for (String ext : validExts) {
            if (text.endsWith(ext)) {
                fileOpen(text);
                return true;
            }
        }
        return false;
    }

    public static boolean inspect(String program) {
        String[] validExts = {".py", ".java", ".js", ".c", ".cpp", ".v", ".ac", ".s", ".go", ".rs", ".sh", ".html", ".sql"};
        for (String ext : validExts) {
            if (program.endsWith(ext)) {
                fileOpen(program);
                return true;
            }
        }
        return false;
    }

    public static void acPage() {
        openURL("https://aclang.vercel.app");
    }

    public static String pageGet(String url) {
        // HTTP(S) GET returning the body as a string (HttpURLConnection, stdlib).
        // Adds https:// if no scheme; "" on error.
        String full = url.contains("://") ? url : "https://" + url;
        try {
            java.net.HttpURLConnection conn =
                (java.net.HttpURLConnection) new java.net.URL(full).openConnection();
            conn.setRequestMethod("GET");
            conn.setConnectTimeout(15000);
            conn.setReadTimeout(15000);
            conn.setInstanceFollowRedirects(true);
            java.io.InputStream in = conn.getInputStream();
            java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
            byte[] buf = new byte[4096];
            int n;
            while ((n = in.read(buf)) != -1) out.write(buf, 0, n);
            in.close();
            return out.toString("UTF-8");
        } catch (Exception e) {
            return "";
        }
    }

    public static void help() {
        String helpText = "=== Web Library Functions ===\n" +
            "web.open(link)           - Opens https://{link}\n" +
            "web.fileOpen(file)       - Opens file://{file}\n" +
            "web.popen(rawLink)       - Opens {rawLink} as-is\n" +
            "web.ropen(search)        - Opens https://{search}.com\n" +
            "web.browser()            - Opens https://www.google.com\n" +
            "web.pdf(pdf)             - Opens file://{pdf}, must be .pdf\n" +
            "web.text(text)           - Opens file://{text}, must be .txt, .md, .bashrc, or .zshrc\n" +
            "web.inspect(program)     - Opens file://{program}, must be source code\n" +
            "web.acPage()             - Opens the AC language website\n" +
            "web.pageGet(link)        - Gets HTML contents of a webpage\n" +
            "web.help()               - Shows this help message";
        System.out.println(helpText);
    }

    private static void openURL(String url) {
        try {
            if (Desktop.isDesktopSupported()) {
                Desktop.getDesktop().browse(new URI(url));
            } else {
                openURLFallback(url);
            }
        } catch (Exception e) {
            openURLFallback(url);
        }
    }

    private static void openURLFallback(String url) {
        try {
            String osName = System.getProperty("os.name").toLowerCase();
            if (osName.contains("win")) {
                Runtime.getRuntime().exec("cmd /c start " + url);
            } else if (osName.contains("mac")) {
                Runtime.getRuntime().exec("open " + url);
            } else if (osName.contains("linux") || osName.contains("unix")) {
                Runtime.getRuntime().exec("xdg-open " + url);
            }
        } catch (Exception e) {
            System.err.println("Could not open URL: " + e.getMessage());
        }
    }
}

class web {
    public static void open(String link) { WebLib.open(link); }
    public static void file_open(String file) { WebLib.fileOpen(file); }
    public static void popen(String rawLink) { WebLib.popen(rawLink); }
    public static void ropen(String identifier) { WebLib.ropen(identifier); }
    public static void browser() { WebLib.browser(); }
    public static long pdf(String pdf) { return WebLib.pdf(pdf) ? 1L : 0L; }
    public static long text(String text) { return WebLib.text(text) ? 1L : 0L; }
    public static long inspect(String program) { return WebLib.inspect(program) ? 1L : 0L; }
    public static void ac_page() { WebLib.acPage(); }
    public static String page_get(String url) { return WebLib.pageGet(url); }
    public static String help() {
        WebLib.help();
        return "=== Web Library Functions ===";
    }
}
