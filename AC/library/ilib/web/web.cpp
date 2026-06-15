#include "web.hpp"
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#endif

static void open_url(const char* url) {
#ifdef _WIN32
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "start %s", url);
    system(cmd);
#elif __APPLE__
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "open %s", url);
    system(cmd);
#else
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "xdg-open '%s'", url);
    system(cmd);
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
    if (strlen(pdf) > 4 && strcmp(pdf + strlen(pdf) - 4, ".pdf") == 0) {
        ac_web_file_open(pdf);
        return 1;
    }
    return 0;
}

int ac_web_text(const char* text) {
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

const char* ac_web_help() {
    return "=== Web Library Functions ===\nweb_open(link)\nweb_file_open(file)\nweb_popen(raw_link)\nweb_ropen(search)\nweb_browser()\nweb_pdf(pdf)\nweb_text(text)\nweb_inspect(program)\nweb_ac_page()\nweb_help()\n";
}

} // extern "C"
