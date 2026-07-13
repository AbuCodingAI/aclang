# AC Web Library — native Python implementation
# Opening URLs/files in the default browser, and fetching page contents.
import webbrowser as _wb
import urllib.request as _urlreq

_SOURCE_EXTS = (".py", ".java", ".js", ".c", ".cpp", ".v", ".ac",
                ".s", ".go", ".rs", ".sh", ".html", ".sql")
_TEXT_EXTS = (".txt", ".md", ".bashrc", ".zshrc")


def _has_scheme(url):
    return "://" in url


def web_open(link):
    """Open a URL, adding https:// if no scheme is present."""
    link = str(link)
    _wb.open(link if _has_scheme(link) else "https://" + link)


def web_file_open(file):
    """Open a local file in the browser."""
    _wb.open("file://" + str(file))


def web_popen(raw_link):
    """Open a raw URL exactly as given (no modifications)."""
    _wb.open(str(raw_link))


def web_ropen(identifier):
    """Open https://{identifier}.com"""
    _wb.open("https://" + str(identifier) + ".com")


def web_browser():
    """Open the default browser to Google."""
    _wb.open("https://www.google.com")


def web_pdf(pdf):
    """Open a PDF file (must end in .pdf). Returns True if opened."""
    pdf = str(pdf)
    if pdf.endswith(".pdf"):
        web_file_open(pdf)
        return True
    return False


def web_text(text):
    """Open a text file (.txt, .md, .bashrc, .zshrc). Returns True if opened."""
    text = str(text)
    if text.endswith(_TEXT_EXTS):
        web_file_open(text)
        return True
    return False


def web_inspect(program):
    """Open a source file for inspection. Returns True if opened."""
    program = str(program)
    if program.endswith(_SOURCE_EXTS):
        web_file_open(program)
        return True
    return False


def web_ac_page():
    """Open the AC language website."""
    _wb.open("https://aclang.vercel.app")


def web_page_get(url):
    """Fetch a page's contents as a string (HTTP GET). Adds https:// if needed.
    Returns "" on any error so callers always get a string."""
    url = str(url)
    if not _has_scheme(url):
        url = "https://" + url
    try:
        with _urlreq.urlopen(url, timeout=15) as resp:
            return resp.read().decode("utf-8", "replace")
    except Exception:
        return ""


def web_help():
    """Print help for all web functions."""
    print("""=== Web Library Functions ===
web.open(link)        - Opens https://{link} (or {link} if it has a scheme)
web.file_open(file)   - Opens file://{file}
web.popen(raw_link)   - Opens {raw_link} as-is
web.ropen(search)     - Opens https://{search}.com
web.browser()         - Opens https://www.google.com
web.pdf(pdf)          - Opens {pdf} if it ends in .pdf
web.text(text)        - Opens {text} if .txt/.md/.bashrc/.zshrc
web.inspect(program)  - Opens {program} if a source file
web.ac_page()         - Opens the AC language website
web.page_get(link)    - Returns the page's HTML contents as a string
web.help()            - Shows this help message""")
