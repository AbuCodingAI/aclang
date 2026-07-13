/* AC ilib: web - C-compatible header (libacweb.so / acweb.dll)
   use ilib web */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void        ac_web_open(const char* link);
void        ac_web_file_open(const char* file);
void        ac_web_popen(const char* raw_link);
void        ac_web_ropen(const char* identifier);
void        ac_web_browser(void);
int         ac_web_pdf(const char* pdf);
int         ac_web_text(const char* text);
int         ac_web_inspect(const char* program);
void        ac_web_ac_page(void);
const char* ac_web_page_get(const char* url);
const char* ac_web_help(void);

static inline void        web_open(const char* link)        { ac_web_open(link); }
static inline void        web_file_open(const char* file)   { ac_web_file_open(file); }
static inline void        web_popen(const char* raw_link)   { ac_web_popen(raw_link); }
static inline void        web_ropen(const char* identifier) { ac_web_ropen(identifier); }
static inline void        web_browser(void)                 { ac_web_browser(); }
static inline int         web_pdf(const char* pdf)          { return ac_web_pdf(pdf); }
static inline int         web_text(const char* text)        { return ac_web_text(text); }
static inline int         web_inspect(const char* program)  { return ac_web_inspect(program); }
static inline void        web_ac_page(void)                 { ac_web_ac_page(); }
static inline const char* web_page_get(const char* url)     { return ac_web_page_get(url); }
static inline const char* web_help(void)                    { return ac_web_help(); }

#ifdef __cplusplus
}
#endif

/* Call-form shims: the AC compiler emits web_x(...) on C and web.x(...) on C++. */
#ifdef __cplusplus
struct _ac_web_ns {
    void (*open)(const char*) = ac_web_open;
    void (*file_open)(const char*) = ac_web_file_open;
    void (*popen)(const char*) = ac_web_popen;
    void (*ropen)(const char*) = ac_web_ropen;
    void (*browser)() = ac_web_browser;
    int (*pdf)(const char*) = ac_web_pdf;
    int (*text)(const char*) = ac_web_text;
    int (*inspect)(const char*) = ac_web_inspect;
    void (*ac_page)() = ac_web_ac_page;
    const char* (*page_get)(const char*) = ac_web_page_get;
    const char* (*help)() = ac_web_help;
};
static _ac_web_ns web;
#else
#define web_open ac_web_open
#define web_file_open ac_web_file_open
#define web_popen ac_web_popen
#define web_ropen ac_web_ropen
#define web_browser ac_web_browser
#define web_pdf ac_web_pdf
#define web_text ac_web_text
#define web_inspect ac_web_inspect
#define web_ac_page ac_web_ac_page
#define web_page_get ac_web_page_get
#define web_help ac_web_help
#endif
