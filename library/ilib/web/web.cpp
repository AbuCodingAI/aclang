#include "web.hpp"
#include <cstdlib>
#include <cstdio>
#include <string>
#include <cstring>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#endif

// Only allow http(s) URLs built from safe characters: blocks shell-metacharacter
// injection before the URL is ever handed to a shell (popen/system).
static bool web_safe_url(const char* url) {
    if (!url) return false;
    std::string u(url);
    if (u.rfind("http://", 0) != 0 && u.rfind("https://", 0) != 0) return false;
    for (unsigned char c : u) {
        if (c < 0x20 || c == ' ' || c == '\'' || c == '"' || c == '`' ||
            c == '\\' || c == ';' || c == '|' || c == '&' || c == '$' ||
            c == '<' || c == '>' || c == '(' || c == ')' || c == '*' || c == '?')
            return false;
    }
    return true;
}

static void open_url(const char* url) {
    if (!url) return;
#ifdef _WIN32
    // ShellExecute passes the URL as a single parameter — no shell command line to inject into.
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#else
  #ifdef __APPLE__
    const char* opener = "open";
  #else
    const char* opener = "xdg-open";
  #endif
    // Spawn via execvp with the URL as a distinct argv element — NO shell, so a URL
    // containing '$(...)', backticks, ';', quotes etc. is just a URL, never a command.
    pid_t pid = fork();
    if (pid == 0) {
        char* av[] = { (char*)opener, (char*)url, nullptr };
        execvp(opener, av);
        _exit(127);
    }
    if (pid > 0) { int st; waitpid(pid, &st, 0); }
#endif
}

extern "C" {

void ac_web_open(const char* link) {
    char url[512];
    snprintf(url, sizeof(url), "https://%s", link);
    open_url(url);
}

void ac_web_file_open(const char* file) {
    char url[512];
    snprintf(url, sizeof(url), "file://%s", file);
    open_url(url);
}

void ac_web_popen(const char* raw_link) {
    open_url(raw_link);
}

void ac_web_ropen(const char* identifier) {
    char url[512];
    snprintf(url, sizeof(url), "https://%s.com", identifier);
    open_url(url);
}

void ac_web_browser() {
    open_url("https://www.google.com");
}

int ac_web_pdf(const char* pdf) {
    if (!pdf) return 0;
    if (strlen(pdf) > 4 && strcmp(pdf + strlen(pdf) - 4, ".pdf") == 0) {
        ac_web_file_open(pdf);
        return 1;
    }
    return 0;
}

int ac_web_text(const char* text) {
    if (!text) return 0;
    const char* valid_exts[] = {".txt", ".md", ".bashrc", ".zshrc"};
    size_t text_len = strlen(text);
    for (int i = 0; i < 4; i++) {
        size_t ext_len = strlen(valid_exts[i]);
        if (text_len > ext_len && strcmp(text + text_len - ext_len, valid_exts[i]) == 0) {
            ac_web_file_open(text);
            return 1;
        }
    }
    return 0;
}

int ac_web_inspect(const char* program) {
    if (!program) return 0;
    const char* valid_exts[] = {".py", ".java", ".js", ".c", ".cpp", ".v", ".ac", ".s", ".go", ".rs", ".sh", ".html", ".sql"};
    size_t prog_len = strlen(program);
    for (int i = 0; i < 13; i++) {
        size_t ext_len = strlen(valid_exts[i]);
        if (prog_len > ext_len && strcmp(program + prog_len - ext_len, valid_exts[i]) == 0) {
            ac_web_file_open(program);
            return 1;
        }
    }
    return 0;
}

void ac_web_ac_page() {
    open_url("https://aclang.vercel.app");
}

// Fetch a page's contents over HTTP(S) and return them as a string.
// Delegates to curl (ubiquitous, follows redirects, honors a timeout) — matching the
// library's "use the OS tools" philosophy. Returns "" on any error so AC always gets a string.
const char* ac_web_page_get(const char* url) {
    static std::string buf;          // owns the result; valid until the next page_get call
    buf.clear();
    if (!web_safe_url(url)) return "";
    std::string cmd = "curl -sL --max-time 15 -- \"";
    cmd += url;                        // validated above: no shell metacharacters
    cmd += "\"";
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return "";
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), p)) > 0) buf.append(chunk, n);
    pclose(p);
    return buf.c_str();
}

const char* ac_web_help() {
    return "=== Web Library Functions ===\nweb_open(link)\nweb_file_open(file)\nweb_popen(raw_link)\nweb_ropen(search)\nweb_browser()\nweb_pdf(pdf)\nweb_text(text)\nweb_inspect(program)\nweb_ac_page()\nweb_page_get(link)\nweb_help()\n";
}

} // extern "C"
