#!/usr/bin/env python3
"""AC-IDE — Interactive REPL for the AC language."""

import subprocess
import tempfile
import shutil
import os
import sys
import readline
import atexit
import time
import re

HISTORY_FILE = os.path.expanduser("~/.ac_history")
DEFAULT_COMPILER = os.environ.get("AC_COMPILER", os.path.join(os.path.dirname(__file__), "ac"))

VERSION = "0.2.0"

# ── ANSI colours ──────────────────────────────────────────────────────────────

def _has_color():
    return sys.stdout.isatty() and os.environ.get("TERM", "") != "dumb"

USE_COLOR = _has_color()

def _c(code, text):
    return f"\033[{code}m{text}\033[0m" if USE_COLOR else text

def red(t):    return _c("31", t)
def green(t):  return _c("32", t)
def yellow(t): return _c("33", t)
def cyan(t):   return _c("36", t)
def bold(t):   return _c("1",  t)
def dim(t):    return _c("2",  t)

# ── Minimal syntax highlighter ────────────────────────────────────────────────

KEYWORDS = {
    "IF", "ELSEIF", "OTHER", "WHILST", "FOR", "in", "range", "sequence",
    "Make", "func", "return", "bundle", "use", "ilib", "elib", "clib", "flib",
    "from", "try", "catch", "after", "raise", "free",
    "Term.display", "Term.ask", "to_dec", "to_int", "to_string", "to_bool",
    "/kill", "/stop", "/end", "/restart",
}

def highlight(line):
    if not USE_COLOR:
        return line
    # Comments
    if "/*" in line:
        idx = line.index("/*")
        return highlight(line[:idx]) + dim(line[idx:])
    # Strings $...$
    def repl_string(m):
        return yellow(m.group(0))
    line = re.sub(r'r?\$[^$]*\$', repl_string, line)
    # Keywords
    for kw in KEYWORDS:
        line = re.sub(r'\b' + re.escape(kw) + r'\b', cyan(kw), line)
    # Numbers
    line = re.sub(r'\b(\d+\.?\d*)\b', _c("35", r'\1'), line)
    return line

# ── History ───────────────────────────────────────────────────────────────────

def load_history():
    if os.path.exists(HISTORY_FILE):
        try:
            readline.read_history_file(HISTORY_FILE)
        except Exception:
            pass

def save_history():
    try:
        readline.write_history_file(HISTORY_FILE)
    except Exception:
        pass

# ── REPL Core ─────────────────────────────────────────────────────────────────

SUPPORTED_BACKENDS = [
    "PY", "JS", "C", "CPP", "BNY", "GO", "V", "RS", "Java", "HTML", "ASM", "LIB"
]

class ACIDE:
    def __init__(self):
        self.target    = "PY"
        self.compiler  = DEFAULT_COMPILER
        self.functions = []   # persisted function/bundle definitions
        self.body      = []   # persisted mainloop body lines
        self.imports   = []   # persisted use/import statements
        self.temp_dir  = tempfile.mkdtemp(prefix="ac_ide_")
        self.timing    = False
        self._run_count = 0

    # ── Lifecycle ─────────────────────────────────────────────────────────────

    def cleanup(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def reset(self):
        self.functions.clear()
        self.body.clear()
        self.imports.clear()
        self._run_count = 0
        print(green("Session reset."))

    # ── File helpers ──────────────────────────────────────────────────────────

    def _source_path(self):
        return os.path.join(self.temp_dir, "repl.ac")

    def _output_ext(self):
        return {
            "PY": "py", "JS": "js", "BNY": "acb", "GO": "go",
            "V": "v", "RS": "rs", "HTML": "html", "ASM": "s",
            "C": "c", "CPP": "cpp", "Java": "java", "LIB": "so",
        }.get(self.target, self.target.lower())

    def _output_path(self):
        return os.path.join(self.temp_dir, f"repl.{self._output_ext()}")

    def _classify(self, line):
        stripped = line.strip()
        if stripped.startswith("use "):
            return "import"
        if stripped.startswith("Make ") or stripped.startswith("bundle "):
            return "func"
        return "body"

    def _write_program(self, extra_body=None):
        lines = [f"AC->{self.target}", ""]
        for imp in self.imports:
            lines.append(imp)
        if self.imports:
            lines.append("")
        for fn in self.functions:
            lines.append(fn)
        if self.functions:
            lines.append("")
        lines.append("<mainloop>")
        for b in self.body:
            lines.append("    " + b)
        if extra_body:
            for b in extra_body:
                lines.append("    " + b)
        lines.append("    /kill")
        lines.append("<mainloop>")
        src = "\n".join(lines) + "\n"
        with open(self._source_path(), "w") as f:
            f.write(src)
        return src

    # ── Compile + run ─────────────────────────────────────────────────────────

    def _run_cmd(self):
        return {
            "PY":   ["python3", self._output_path()],
            "JS":   ["node",    self._output_path()],
            "BNY":  [self._output_path()],
        }.get(self.target)

    def execute(self, lines, persist=True):
        body_lines = []
        func_lines = []
        import_lines = []

        for line in lines:
            kind = self._classify(line)
            if kind == "import":
                import_lines.append(line)
            elif kind == "func":
                func_lines.append(line)
            else:
                body_lines.append(line)

        self._write_program(extra_body=body_lines)

        t0 = time.time()
        result = subprocess.run(
            [self.compiler, self._source_path(), "--target", self.target, "--no-cache"],
            capture_output=True, text=True
        )
        compile_time = time.time() - t0

        if result.returncode != 0:
            err = result.stderr.strip()
            print(red("Compilation error:"))
            for ln in err.splitlines():
                print("  " + ln)
            return False

        run_cmd = self._run_cmd()
        if not run_cmd:
            print(yellow(f"[Generated: {self._output_path()}]"))
            if persist:
                self._persist(import_lines, func_lines, body_lines)
            return True

        t1 = time.time()
        run_result = subprocess.run(run_cmd, capture_output=True, text=True)
        run_time = time.time() - t1

        if run_result.stdout:
            print(run_result.stdout, end="")
        if run_result.stderr:
            print(red("Runtime error:"))
            for ln in run_result.stderr.splitlines():
                print("  " + ln)

        if self.timing:
            print(dim(f"  [compile: {compile_time:.3f}s  run: {run_time:.3f}s]"))

        if run_result.returncode == 0 and persist:
            self._persist(import_lines, func_lines, body_lines)

        return run_result.returncode == 0

    def _persist(self, import_lines, func_lines, body_lines):
        self.imports.extend(import_lines)
        self.functions.extend(func_lines)
        self.body.extend(body_lines)

    # ── Commands ──────────────────────────────────────────────────────────────

    def _cmd_help(self, _args):
        print(bold("AC-IDE Commands"))
        print(f"  {cyan(':help')}           This message")
        print(f"  {cyan(':exit')}           Exit")
        print(f"  {cyan(':reset')}          Clear session state")
        print(f"  {cyan(':show')}           Print the current program")
        print(f"  {cyan(':list')}           List session variables and functions")
        print(f"  {cyan(':target <name>')}  Switch backend ({', '.join(SUPPORTED_BACKENDS)})")
        print(f"  {cyan(':undo')}           Remove last body line")
        print(f"  {cyan(':run')}            Re-run the current session program")
        print(f"  {cyan(':time on|off')}    Toggle timing display")
        print(f"  {cyan(':load <file>')}    Load and run an .ac file")
        print(f"  {cyan(':save <file>')}    Save current session to a file")

    def _cmd_show(self, _args):
        src = self._write_program()
        print(bold("── Current Program ──────────────────"))
        for ln in src.splitlines():
            print(highlight(ln))
        print(bold("─────────────────────────────────────"))

    def _cmd_list(self, _args):
        print(bold("Imports:"))
        for imp in self.imports or ["  (none)"]:
            print("  " + imp)
        print(bold("Functions:"))
        for fn in self.functions or ["  (none)"]:
            print("  " + fn)
        print(bold("Body lines:"))
        for b in self.body or ["  (none)"]:
            print("  " + b)

    def _cmd_undo(self, _args):
        if self.body:
            removed = self.body.pop()
            print(dim(f"Removed: {removed}"))
        else:
            print(yellow("Nothing to undo."))

    def _cmd_reset(self, _args):
        self.reset()

    def _cmd_run(self, _args):
        self.execute([], persist=False)

    def _cmd_target(self, args):
        if not args:
            print(f"Current target: {bold(self.target)}")
            print(f"Available: {', '.join(SUPPORTED_BACKENDS)}")
        else:
            tgt = args[0].upper() if args[0].upper() in [b.upper() for b in SUPPORTED_BACKENDS] else args[0]
            # find exact case-matched backend
            matched = next((b for b in SUPPORTED_BACKENDS if b.upper() == tgt), None)
            if matched:
                self.target = matched
                print(green(f"Target → {self.target}"))
            else:
                print(red(f"Unknown backend: {args[0]}"))

    def _cmd_time(self, args):
        if not args:
            print(f"Timing: {'on' if self.timing else 'off'}")
        else:
            self.timing = args[0].lower() == "on"
            print(green(f"Timing {'enabled' if self.timing else 'disabled'}"))

    def _cmd_load(self, args):
        if not args:
            print(red("Usage: :load <file>"))
            return
        path = " ".join(args)
        if not os.path.exists(path):
            print(red(f"File not found: {path}"))
            return
        try:
            with open(path) as f:
                src = f.read()
            result = subprocess.run(
                [self.compiler, path],
                capture_output=True, text=True
            )
            if result.stdout: print(result.stdout, end="")
            if result.stderr: print(red(result.stderr), end="")
        except Exception as e:
            print(red(f"Error: {e}"))

    def _cmd_save(self, args):
        if not args:
            print(red("Usage: :save <file>"))
            return
        path = " ".join(args)
        src = self._write_program()
        try:
            with open(path, "w") as f:
                f.write(src)
            print(green(f"Saved → {path}"))
        except Exception as e:
            print(red(f"Error: {e}"))

    COMMANDS = {
        "help":   _cmd_help,
        "show":   _cmd_show,
        "list":   _cmd_list,
        "undo":   _cmd_undo,
        "reset":  _cmd_reset,
        "run":    _cmd_run,
        "target": _cmd_target,
        "time":   _cmd_time,
        "load":   _cmd_load,
        "save":   _cmd_save,
        "exit":   None,
        "quit":   None,
    }

    def handle_command(self, line):
        parts = line[1:].split()
        if not parts:
            return
        cmd = parts[0].lower()
        args = parts[1:]
        if cmd in ("exit", "quit"):
            raise EOFError
        handler = self.COMMANDS.get(cmd)
        if handler:
            handler(self, args)
        else:
            print(red(f"Unknown command: {cmd}"))
            print(dim("Type :help for available commands."))

# ── Indented block detection ──────────────────────────────────────────────────

BLOCK_OPENERS = re.compile(
    r'^\s*(IF\b|ELSEIF\b|OTHER\b|WHILST\b|FOR\b|Make\b|bundle\b|try\b|catch\b|after\b)'
)

def needs_continuation(lines):
    """Return True if the buffer looks like an incomplete indented block."""
    if not lines:
        return False
    last = lines[-1]
    if BLOCK_OPENERS.match(last):
        return True
    # If last non-empty line is indented relative to the first, we're inside a block
    if len(lines) > 1 and last.startswith("    "):
        return True
    return False

# ── Main loop ─────────────────────────────────────────────────────────────────

def main():
    load_history()
    atexit.register(save_history)

    ide = ACIDE()
    atexit.register(ide.cleanup)

    print(bold(f"AC-IDE v{VERSION}") + dim("  |  Type :help for commands"))
    print(dim(f"Target: {ide.target}  |  Compiler: {ide.compiler}"))
    print()

    buffer = []

    while True:
        try:
            is_cont = bool(buffer)
            prompt = dim("... ") if is_cont else (cyan("ac") + dim("> "))
            line = input(prompt)

            # Command
            if not is_cont and line.strip().startswith(":"):
                ide.handle_command(line.strip())
                continue

            # Empty line: flush buffer if we have one
            if not line.strip():
                if buffer:
                    ide.execute(buffer)
                    buffer.clear()
                continue

            buffer.append(line)

            # Auto-detect if we need more input (block openers)
            if not needs_continuation(buffer):
                ide.execute(buffer)
                buffer.clear()

        except EOFError:
            if buffer:
                ide.execute(buffer)
            print("\n" + dim("Goodbye."))
            break
        except KeyboardInterrupt:
            print()
            if buffer:
                buffer.clear()
                print(dim("Buffer cleared."))
            else:
                print(dim("Ctrl-C: type :exit to quit."))


if __name__ == "__main__":
    main()
