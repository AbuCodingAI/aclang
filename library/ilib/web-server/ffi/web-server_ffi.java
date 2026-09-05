// AC ilib: web-server — Java FFI. Pure JDK stdlib, no dependency: java.net gives
// real blocking sockets (ServerSocket/Socket), so HTTP serving and the SQL worker
// connection both work like the C++ core — no JS-style subprocess-bridge trick
// needed here.
import java.io.*;
import java.net.*;
import java.nio.channels.SocketChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

class WebServerLib {
    static ServerSocket listener;
    static Socket conn;
    static String reqMethod = "", reqPath = "", reqBody = "";
    static Map<String, String> reqQuery = new HashMap<>();
    static Map<String, String> reqHeaders = new HashMap<>();

    static Process sqlWorker;
    static String sqlSockPath = "/tmp/ac_ws_java_" + ProcessHandle.current().pid() + ".sock";

    static Path ilibDir(String lib) {
        Path rel = Paths.get("library", "ilib", lib);
        String acp = System.getenv("AC_PATH");
        if (acp != null) {
            Path cand = Paths.get(acp).resolve(rel);
            if (Files.isDirectory(cand)) return cand;
        }
        Path cand = Paths.get(".").resolve(rel);
        if (Files.isDirectory(cand)) return cand;
        try {
            String jarPath = new File(WebServerLib.class.getProtectionDomain()
                .getCodeSource().getLocation().toURI()).getParent();
            if (jarPath != null) {
                Path fromJar = Paths.get(jarPath).resolve("..").resolve(rel);
                if (Files.isDirectory(fromJar)) return fromJar;
            }
        } catch (Exception ignored) {}
        return cand;
    }

    // ── SQL: persistent jasql_cli.py worker, real Unix-domain socket (Java 16+) ──

    static boolean ensureSqlWorker() {
        if (sqlChannel != null) return true;
        try {
            Path script = ilibDir("web-server").resolve("jasql_cli.py");
            ProcessBuilder pb = new ProcessBuilder("python3", script.toString(), "--serve", sqlSockPath);
            pb.redirectOutput(ProcessBuilder.Redirect.DISCARD);
            pb.redirectError(ProcessBuilder.Redirect.DISCARD);
            sqlWorker = pb.start();
        } catch (IOException e) {
            return false;
        }
        UnixDomainSocketAddress addr = UnixDomainSocketAddress.of(sqlSockPath);
        for (int i = 0; i < 150; i++) {
            if (Files.exists(Paths.get(sqlSockPath))) {
                try {
                    SocketChannel ch = SocketChannel.open(StandardProtocolFamily.UNIX);
                    ch.connect(addr);
                    sqlChannel = ch;
                    return true;
                } catch (IOException ignored) {}
            }
            try { Thread.sleep(20); } catch (InterruptedException ignored) {}
        }
        return false;
    }

    static SocketChannel sqlChannel;

    static String sqlRoundtrip(String reqLine) {
        if (!ensureSqlWorker())
            return "{\"ok\":false,\"error\":\"jasql worker failed to start (is python3 on PATH?)\"}";
        try {
            sqlChannel.write(java.nio.ByteBuffer.wrap((reqLine + "\n").getBytes(StandardCharsets.UTF_8)));
            java.nio.ByteBuffer buf = java.nio.ByteBuffer.allocate(65536);
            StringBuilder sb = new StringBuilder();
            while (sb.indexOf("\n") < 0) {
                buf.clear();
                int n = sqlChannel.read(buf);
                if (n <= 0) { sqlChannel.close(); sqlChannel = null; return "{\"ok\":false,\"error\":\"jasql worker connection lost\"}"; }
                buf.flip();
                sb.append(StandardCharsets.UTF_8.decode(buf));
            }
            return sb.toString();
        } catch (IOException e) {
            try { sqlChannel.close(); } catch (IOException ignored) {}
            sqlChannel = null;
            return "{\"ok\":false,\"error\":\"jasql worker io error: " + e.getMessage() + "\"}";
        }
    }

    static String jsonEscape(String s) {
        StringBuilder out = new StringBuilder();
        for (char c : s.toCharArray()) {
            switch (c) {
                case '"': out.append("\\\""); break;
                case '\\': out.append("\\\\"); break;
                case '\n': out.append("\\n"); break;
                case '\r': out.append("\\r"); break;
                case '\t': out.append("\\t"); break;
                default:
                    if (c < 0x20) out.append(String.format("\\u%04x", (int) c));
                    else out.append(c);
            }
        }
        return out.toString();
    }

    static String jsonExtractString(String json, String key) {
        String needle = "\"" + key + "\":\"";
        int p = json.indexOf(needle);
        if (p < 0) return "";
        p += needle.length();
        StringBuilder out = new StringBuilder();
        while (p < json.length() && json.charAt(p) != '"') {
            char c = json.charAt(p);
            if (c == '\\' && p + 1 < json.length()) {
                char n = json.charAt(p + 1);
                switch (n) {
                    case 'n': out.append('\n'); break;
                    case 'r': out.append('\r'); break;
                    case 't': out.append('\t'); break;
                    default: out.append(n);
                }
                p += 2;
            } else {
                out.append(c); p++;
            }
        }
        return out.toString();
    }

    static boolean jsonExtractOk(String json) {
        int p = json.indexOf("\"ok\":");
        return p >= 0 && json.startsWith("true", p + 5);
    }

    static String dbCall(String reqLine) {
        String resp = sqlRoundtrip(reqLine);
        return jsonExtractOk(resp) ? jsonExtractString(resp, "result")
                                    : ("Preposterous: " + jsonExtractString(resp, "error"));
    }

    // ── HTTP serving: real blocking ServerSocket/accept, minimal HTTP/1.1 parsing ─

    static final Map<Integer, String> REASONS = new HashMap<>();
    static {
        REASONS.put(200, "OK"); REASONS.put(201, "Created"); REASONS.put(204, "No Content");
        REASONS.put(301, "Moved Permanently"); REASONS.put(302, "Found");
        REASONS.put(400, "Bad Request"); REASONS.put(401, "Unauthorized"); REASONS.put(403, "Forbidden");
        REASONS.put(404, "Not Found"); REASONS.put(405, "Method Not Allowed"); REASONS.put(500, "Internal Server Error");
    }

    static long listen(long port) {
        try {
            if (listener != null) listener.close();
            listener = new ServerSocket((int) port);
            return 1;
        } catch (IOException e) {
            return 0;
        }
    }

    static long accept() {
        if (listener == null) return 0;
        try {
            if (conn != null) conn.close();
            conn = listener.accept();
            InputStream in = conn.getInputStream();
            BufferedReader r = new BufferedReader(new InputStreamReader(in, StandardCharsets.ISO_8859_1));
            String reqLine = r.readLine();
            if (reqLine == null) return 0;
            String[] parts = reqLine.split(" ");
            reqMethod = parts.length > 0 ? parts[0] : "";
            String rawPath = parts.length > 1 ? parts[1] : "/";
            int q = rawPath.indexOf('?');
            String qs = "";
            if (q >= 0) { qs = rawPath.substring(q + 1); rawPath = rawPath.substring(0, q); }
            reqPath = URLDecoder.decode(rawPath, StandardCharsets.UTF_8);
            reqQuery = new HashMap<>();
            for (String pair : qs.split("&")) {
                if (pair.isEmpty()) continue;
                int eq = pair.indexOf('=');
                if (eq >= 0) reqQuery.put(URLDecoder.decode(pair.substring(0, eq), StandardCharsets.UTF_8),
                                           URLDecoder.decode(pair.substring(eq + 1), StandardCharsets.UTF_8));
                else reqQuery.put(URLDecoder.decode(pair, StandardCharsets.UTF_8), "");
            }
            reqHeaders = new HashMap<>();
            String hline;
            while ((hline = r.readLine()) != null && !hline.isEmpty()) {
                int c = hline.indexOf(':');
                if (c >= 0) reqHeaders.put(hline.substring(0, c).trim().toLowerCase(), hline.substring(c + 1).trim());
            }
            int contentLen = 0;
            try { contentLen = Integer.parseInt(reqHeaders.getOrDefault("content-length", "0")); } catch (NumberFormatException ignored) {}
            char[] bodyChars = new char[contentLen];
            if (contentLen > 0) r.read(bodyChars, 0, contentLen);
            reqBody = new String(bodyChars);
            return 1;
        } catch (IOException e) {
            return 0;
        }
    }

    static long sendResponse(long status, String body, String contentType) {
        if (conn == null) return 0;
        String reason = REASONS.getOrDefault((int) status, "OK");
        String resp = "HTTP/1.1 " + status + " " + reason + "\r\n" +
                      "Content-Type: " + contentType + "\r\n" +
                      "Content-Length: " + body.getBytes(StandardCharsets.UTF_8).length + "\r\n" +
                      "Connection: close\r\n\r\n" + body;
        try {
            conn.getOutputStream().write(resp.getBytes(StandardCharsets.UTF_8));
            conn.close();
            conn = null;
            return 1;
        } catch (IOException e) {
            return 0;
        }
    }

    static void close() {
        try { if (conn != null) conn.close(); } catch (IOException ignored) {}
        try { if (listener != null) listener.close(); } catch (IOException ignored) {}
        conn = null; listener = null;
    }

    static void dbStop() {
        try { if (sqlChannel != null) sqlChannel.close(); } catch (IOException ignored) {}
        sqlChannel = null;
        if (sqlWorker != null) { sqlWorker.destroy(); sqlWorker = null; }
    }
}

class server {
    public static long listen(long port) { return WebServerLib.listen(port); }
    public static long accept() { return WebServerLib.accept(); }
    public static String req_method() { return WebServerLib.reqMethod; }
    public static String req_path() { return WebServerLib.reqPath; }
    public static String req_body() { return WebServerLib.reqBody; }
    public static String req_query(String key) { return WebServerLib.reqQuery.getOrDefault(key, ""); }
    public static String req_header(String name) { return WebServerLib.reqHeaders.getOrDefault(name.toLowerCase(), ""); }
    public static long respond(long status, String body) { return WebServerLib.sendResponse(status, body, "text/plain; charset=utf-8"); }
    public static long respond_json(long status, String jsonBody) { return WebServerLib.sendResponse(status, jsonBody, "application/json"); }
    public static void close() { WebServerLib.close(); }

    public static String db_run(String statement) {
        return WebServerLib.dbCall("{\"cmd\":\"run\",\"stmt\":\"" + WebServerLib.jsonEscape(statement) + "\"}");
    }
    public static String db_run_p(String statement, String paramsJson) {
        String pj = (paramsJson == null || paramsJson.isEmpty()) ? "null" : paramsJson;
        return WebServerLib.dbCall("{\"cmd\":\"run\",\"stmt\":\"" + WebServerLib.jsonEscape(statement) + "\",\"params\":" + pj + "}");
    }
    public static String db_import(String path) {
        return WebServerLib.dbCall("{\"cmd\":\"import\",\"path\":\"" + WebServerLib.jsonEscape(path) + "\"}");
    }
    public static String db_reset() { return WebServerLib.dbCall("{\"cmd\":\"reset\"}"); }
    public static void db_stop() { WebServerLib.dbStop(); }

    public static String help() {
        String helpText = "=== web-server Library ===\n" +
            "One request in flight at a time (single blocking accept loop, no threads).\n\n" +
            "server.listen(port)\n" +
            "server.accept()                          - blocks for next request\n" +
            "server.req_method() / req_path() / req_body()\n" +
            "server.req_query(key) / req_header(name)\n" +
            "server.respond(status, body) / respond_json(status, json_body)\n" +
            "server.close()\n\n" +
            "server.db_run(statement)                 - no placeholders; trusted/static text only\n" +
            "server.db_run_p(statement, paramsJson)   - ?/:name placeholders, bound server-side\n" +
            "                                               by JaSQL (JSON array/object string) -\n" +
            "                                               the safe path for request-derived data\n" +
            "server.db_import(path) / db_reset() / db_stop()";
        System.out.println(helpText);
        return helpText;
    }
}
