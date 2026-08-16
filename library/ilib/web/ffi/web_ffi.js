// Web library FFI for JavaScript (browser environment)

const web = {
  open: function(link) {
    window.open("https://" + String(link), "_blank");
  },

  file_open: function(file) {
    window.open("file://" + String(file), "_blank");
  },

  popen: function(raw_link) {
    window.open(String(raw_link), "_blank");
  },

  ropen: function(identifier) {
    window.open("https://" + String(identifier) + ".com", "_blank");
  },

  browser: function() {
    window.open("https://www.google.com", "_blank");
  },

  pdf: function(pdf) {
    const pdf_str = String(pdf);
    if (pdf_str.endsWith(".pdf")) {
      web.file_open(pdf_str);
      return true;
    }
    return false;
  },

  text: function(text) {
    const text_str = String(text);
    const valid_exts = [".txt", ".md", ".bashrc", ".zshrc"];
    if (valid_exts.some(ext => text_str.endsWith(ext))) {
      web.file_open(text_str);
      return true;
    }
    return false;
  },

  inspect: function(program) {
    const prog_str = String(program);
    const valid_exts = [".py", ".java", ".js", ".c", ".cpp", ".v", ".ac", ".s", ".go", ".rs", ".sh", ".html", ".sql"];
    if (valid_exts.some(ext => prog_str.endsWith(ext))) {
      web.file_open(prog_str);
      return true;
    }
    return false;
  },

  ac_page: function() {
    window.open("https://aclang.vercel.app", "_blank");
  },

  page_get: function(url) {
    // Synchronous GET so it returns the HTML string directly (AC values aren't promises).
    // Adds https:// if no scheme. Subject to the browser's CORS policy. "" on error.
    var u = String(url);
    if (u.indexOf("://") === -1) u = "https://" + u;
    try {
      if (typeof XMLHttpRequest !== "undefined") {
        var x = new XMLHttpRequest();
        x.open("GET", u, false);   // synchronous
        x.send(null);
        return x.responseText;
      }
      // Node fallback: shell out to curl synchronously. Uses the argv-array form (no
      // shell string is built), so '$(...)'/backticks/';' etc. in the URL are inert.
      return require("child_process")
        .execFileSync("curl", ["-sL", "--max-time", "15", "--", u]).toString();
    } catch (e) {
      return "";
    }
  },

  help: function() {
    const help_text = `=== Web Library Functions ===
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
web.help()               - Shows this help message`;
    console.log(help_text);
  }
};
