import java.awt.Desktop;
import java.net.URI;

public class WebLib {
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
        // Stub - would need java.net.http for real implementation
        return null;
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
