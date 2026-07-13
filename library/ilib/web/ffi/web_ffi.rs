use std::process::Command;

pub struct WebLib;

impl WebLib {
    pub fn open(&self, link: &str) {
        Self::open_url(&format!("https://{}", link));
    }

    pub fn file_open(&self, file: &str) {
        Self::open_url(&format!("file://{}", file));
    }

    pub fn popen(&self, raw_link: &str) {
        Self::open_url(raw_link);
    }

    pub fn ropen(&self, identifier: &str) {
        Self::open_url(&format!("https://{}.com", identifier));
    }

    pub fn browser(&self) {
        Self::open_url("https://www.google.com");
    }

    pub fn pdf(&self, pdf: &str) -> i64 {
        if pdf.ends_with(".pdf") {
            self.file_open(pdf);
            1
        } else {
            0
        }
    }

    pub fn text(&self, text: &str) -> i64 {
        let valid_exts = [".txt", ".md", ".bashrc", ".zshrc"];
        if valid_exts.iter().any(|ext| text.ends_with(ext)) {
            self.file_open(text);
            1
        } else {
            0
        }
    }

    pub fn inspect(&self, program: &str) -> i64 {
        let valid_exts = [".py", ".java", ".js", ".c", ".cpp", ".v", ".ac", ".s", ".go", ".rs", ".sh", ".html", ".sql"];
        if valid_exts.iter().any(|ext| program.ends_with(ext)) {
            self.file_open(program);
            1
        } else {
            0
        }
    }

    pub fn ac_page(&self) {
        Self::open_url("https://aclang.vercel.app");
    }

    pub fn page_get(&self, url: &str) -> String {
        // HTTP(S) GET via curl (no external crate). Adds https:// if no scheme; only allows
        // safe http(s) URLs (blocks shell/arg injection). Returns "" on error.
        let u = if url.contains("://") { url.to_string() } else { format!("https://{}", url) };
        let ok = (u.starts_with("http://") || u.starts_with("https://"))
            && !u.chars().any(|c| c.is_control() || " '\"`\\;|&$<>()*?".contains(c));
        if !ok { return String::new(); }
        match Command::new("curl").args(["-sL", "--max-time", "15", "--", &u]).output() {
            Ok(o) => String::from_utf8_lossy(&o.stdout).into_owned(),
            Err(_) => String::new(),
        }
    }

    pub fn help(&self) -> String {
        let help_text = r#"=== Web Library Functions ===
web.open(link)           - Opens https://{link}
web.file_open(file)      - Opens file://{file}
web.popen(raw_link)      - Opens {raw_link} as-is
web.ropen(search)        - Opens https://{search}.com
web.browser()            - Opens https://www.google.com
web.pdf(pdf)             - Opens file://{pdf}, must be .pdf
web.text(text)           - Opens file://{text}, must be .txt, .md, .bashrc, or .zshrc
web.inspect(program)     - Opens file://{program}, must be source code
web.ac_page()            - Opens the AC language website
web.page_get(link)       - Gets HTML contents of a webpage
web.help()               - Shows this help message"#;
        println!("{}", help_text);
        help_text.to_string()
    }

    fn open_url(url: &str) {
        #[cfg(target_os = "windows")]
        {
            let _ = Command::new("cmd")
                .args(&["/C", "start", url])
                .output();
        }
        #[cfg(target_os = "macos")]
        {
            let _ = Command::new("open").arg(url).output();
        }
        #[cfg(target_os = "linux")]
        {
            let _ = Command::new("xdg-open").arg(url).output();
        }
    }
}

pub static web: WebLib = WebLib;
