#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* Allow only http(s) URLs of safe characters — blocks shell injection via popen. */
static int web_safe_url(const char* url) {
    if (!url) return 0;
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) return 0;
    for (const unsigned char* c = (const unsigned char*)url; *c; c++) {
        if (*c < 0x20 || *c == ' ' || *c == '\'' || *c == '"' || *c == '`' ||
            *c == '\\' || *c == ';' || *c == '|' || *c == '&' || *c == '$' ||
            *c == '<' || *c == '>' || *c == '(' || *c == ')' || *c == '*' || *c == '?')
            return 0;
    }
    return 1;
}

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
    snprintf(cmd, sizeof(cmd), "xdg-open %s", url);
    system(cmd);
#endif
}

void web_open(const char* link) {
    char url[512];
    snprintf(url, sizeof(url), "https://%s", link);
    open_url(url);
}

void web_file_open(const char* file) {
    char url[512];
    snprintf(url, sizeof(url), "file://%s", file);
    open_url(url);
}

void web_popen(const char* raw_link) {
    open_url(raw_link);
}

void web_ropen(const char* identifier) {
    char url[512];
    snprintf(url, sizeof(url), "https://%s.com", identifier);
    open_url(url);
}

void web_browser() {
    open_url("https://www.google.com");
}

int web_pdf(const char* pdf) {
    if (strlen(pdf) > 4 && strcmp(pdf + strlen(pdf) - 4, ".pdf") == 0) {
        web_file_open(pdf);
        return 1;
    }
    return 0;
}

int web_text(const char* text) {
    const char* valid_exts[] = {".txt", ".md", ".bashrc", ".zshrc"};
    size_t text_len = strlen(text);
    for (int i = 0; i < 4; i++) {
        size_t ext_len = strlen(valid_exts[i]);
        if (text_len > ext_len && strcmp(text + text_len - ext_len, valid_exts[i]) == 0) {
            web_file_open(text);
            return 1;
        }
    }
    return 0;
}

int web_inspect(const char* program) {
    const char* valid_exts[] = {".py", ".java", ".js", ".c", ".cpp", ".v", ".ac", ".s", ".go", ".rs", ".sh", ".html", ".sql"};
    size_t prog_len = strlen(program);
    for (int i = 0; i < 13; i++) {
        size_t ext_len = strlen(valid_exts[i]);
        if (prog_len > ext_len && strcmp(program + prog_len - ext_len, valid_exts[i]) == 0) {
            web_file_open(program);
            return 1;
        }
    }
    return 0;
}

void web_ac_page() {
    open_url("https://aclang.vercel.app");
}

/* Fetch a page's contents via HTTP(S) GET (delegates to curl). Returns a heap string,
   "" on any error. The returned pointer is owned by this function (freed on next call). */
const char* web_page_get(const char* url) {
    static char* buf = NULL;
    free(buf);
    buf = NULL;
    if (!web_safe_url(url)) { buf = strdup(""); return buf; }
    char cmd[1100];
    snprintf(cmd, sizeof(cmd), "curl -sL --max-time 15 -- \"%s\"", url);
    FILE* p = popen(cmd, "r");
    if (!p) { buf = strdup(""); return buf; }
    size_t cap = 8192, len = 0;
    buf = (char*)malloc(cap);
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), p)) > 0) {
        if (len + n + 1 > cap) { cap = (len + n + 1) * 2; buf = (char*)realloc(buf, cap); }
        memcpy(buf + len, chunk, n);
        len += n;
    }
    pclose(p);
    buf[len] = '\0';
    return buf;
}

void web_help() {
    printf("=== Web Library Functions ===\n");
    printf("web_open(link)           - Opens https://{link}\n");
    printf("web_file_open(file)      - Opens file://{file}\n");
    printf("web_popen(raw_link)      - Opens {raw_link} as-is\n");
    printf("web_ropen(search)        - Opens https://{search}.com\n");
    printf("web_browser()            - Opens https://www.google.com\n");
    printf("web_pdf(pdf)             - Opens file://{pdf}, must be .pdf\n");
    printf("web_text(text)           - Opens file://{text}, must be .txt, .md, .bashrc, or .zshrc\n");
    printf("web_inspect(program)     - Opens file://{program}, must be source code\n");
    printf("web_ac_page()            - Opens the AC language website\n");
    printf("web_page_get(link)       - Returns the page's HTML contents as a string\n");
    printf("web_help()               - Shows this help message\n");
}
