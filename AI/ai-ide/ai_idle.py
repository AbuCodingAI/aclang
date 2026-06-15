#!/usr/bin/env python3
"""
AI IDLE — Integrated Development Environment for the AI Language
Part of the Abu Development Kit (ADK)
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import subprocess
import threading
import os
import re
import sys

# ── THEME ──────────────────────────────────────────────────────────────────
THEME = {
    "bg":           "#1e1e2e",
    "bg2":          "#181825",
    "bg3":          "#313244",
    "fg":           "#cdd6f4",
    "fg_dim":       "#6c7086",
    "accent":       "#89b4fa",
    "accent2":      "#b4befe",
    "green":        "#a6e3a1",
    "yellow":       "#f9e2af",
    "red":          "#f38ba8",
    "pink":         "#f5c2e7",
    "teal":         "#94e2d5",
    "mauve":        "#cba6f7",
    "orange":       "#fab387",
    "selection":    "#45475a",
    "line_num_bg":  "#181825",
    "line_num_fg":  "#45475a",
    "cursor":       "#f5e0dc",
    "output_bg":    "#11111b",
    "error_fg":     "#f38ba8",
    "info_fg":      "#89dceb",
}

# ── SYNTAX RULES ──────────────────────────────────────────────────────────
SYNTAX = [
    # target declaration
    (r'\bAI->VM\b',                                      "target",   THEME["orange"]),
    # keywords
    (r'\b(Make|func|return|pass|break|continue|skip)\b', "keyword",  THEME["mauve"]),
    (r'\b(IF|ELSEIF|OTHER|WHILST|FOR|IN)\b',             "control",  THEME["pink"]),
    (r'\b(dec|int|string|bool)\b',                       "type",     THEME["teal"]),
    (r'\b(Term\.display|Term\.ask|Term\.halt)\b',        "builtin",  THEME["green"]),
    (r'\b(xsub|is|not|AND|OR)\b',                        "operator", THEME["yellow"]),
    # mainloop tag
    (r'<mainloop>',                                       "tag",      THEME["orange"]),
    # /kill
    (r'/kill\b',                                          "kill",     THEME["red"]),
    # strings $...$
    (r'\$[^$]*\$',                                        "string",   THEME["green"]),
    # dec literals (numbers with decimal points)
    (r'\bdec\s+\d+\.\d+',                                "dec_lit",  THEME["teal"]),
    # numbers
    (r'\b\d+(\.\d+)?\b',                                  "number",   THEME["orange"]),
    # operators
    (r'[+\-*/@|]|#>|#<|#=|#\|',                          "op",       THEME["fg"]),
    # line comments
    (r'--[^\n]*',                                         "comment",  THEME["fg_dim"]),
    # block comments
    (r'/\*.*?\*/',                                        "comment",  THEME["fg_dim"]),
]

# ── LINE NUMBER WIDGET ─────────────────────────────────────────────────────
class LineNumbers(tk.Canvas):
    def __init__(self, parent, text_widget, **kwargs):
        super().__init__(parent, **kwargs)
        self.text_widget = text_widget
        self.config(
            bg=THEME["line_num_bg"],
            highlightthickness=0,
            width=48,
        )
        self.text_widget.bind("<Configure>", self._redraw)
        self.text_widget.bind("<KeyRelease>", self._redraw)
        self.text_widget.bind("<MouseWheel>", self._redraw)
        self.text_widget.bind("<Button-4>",   self._redraw)
        self.text_widget.bind("<Button-5>",   self._redraw)
        self._redraw()

    def _redraw(self, event=None):
        self.delete("all")
        i = self.text_widget.index("@0,0")
        while True:
            dline = self.text_widget.dlineinfo(i)
            if dline is None:
                break
            y = dline[1]
            linenum = str(i).split(".")[0]
            self.create_text(
                40, y,
                anchor="ne",
                text=linenum,
                fill=THEME["line_num_fg"],
                font=("JetBrains Mono", 11),
            )
            i = self.text_widget.index(f"{i}+1line")
            if i == self.text_widget.index(f"{i}+0line"):
                break

# ── EDITOR WITH SYNTAX HIGHLIGHTING ───────────────────────────────────────
class AIEditor(tk.Frame):
    def __init__(self, parent, **kwargs):
        super().__init__(parent, bg=THEME["bg"], **kwargs)

        # Editor text widget
        self.text = tk.Text(
            self,
            font=("JetBrains Mono", 12),
            bg=THEME["bg"],
            fg=THEME["fg"],
            insertbackground=THEME["cursor"],
            selectbackground=THEME["selection"],
            selectforeground=THEME["fg"],
            relief="flat",
            bd=0,
            padx=12,
            pady=8,
            undo=True,
            autoseparators=True,
            tabs=("32",),
            wrap="none",
        )

        # Scrollbars
        vscroll = tk.Scrollbar(self, orient="vertical",   command=self.text.yview)
        hscroll = tk.Scrollbar(self, orient="horizontal", command=self.text.xview)
        self.text.configure(yscrollcommand=vscroll.set, xscrollcommand=hscroll.set)

        # Line numbers
        self.line_numbers = LineNumbers(self, self.text)

        # Layout
        self.line_numbers.pack(side="left",   fill="y")
        vscroll.pack(       side="right",  fill="y")
        hscroll.pack(       side="bottom", fill="x")
        self.text.pack(     side="left",   fill="both", expand=True)

        # Configure syntax tags
        for _, tag, color in SYNTAX:
            self.text.tag_configure(tag, foreground=color)
        self.text.tag_configure("comment", foreground=THEME["fg_dim"], font=("JetBrains Mono", 12, "italic"))

        # Debounce timer
        self._highlight_id = None
        self.text.bind("<KeyRelease>", self._on_key)
        self.text.bind("<<Modified>>", self._on_modified)

    def _on_key(self, event=None):
        if self._highlight_id:
            self.after_cancel(self._highlight_id)
        self._highlight_id = self.after(120, self._highlight)
        self.line_numbers._redraw()

    def _on_modified(self, event=None):
        self.text.edit_modified(False)

    def _highlight(self):
        content = self.text.get("1.0", "end-1c")
        # Remove all syntax tags
        for _, tag, _ in SYNTAX:
            self.text.tag_remove(tag, "1.0", "end")

        for pattern, tag, _ in reversed(SYNTAX):
            flags = re.DOTALL if "comment" in tag and "/*" in pattern else 0
            for m in re.finditer(pattern, content, flags):
                start = self._offset_to_index(content, m.start())
                end   = self._offset_to_index(content, m.end())
                self.text.tag_add(tag, start, end)

    def _offset_to_index(self, content, offset):
        lines = content[:offset].split("\n")
        row = len(lines)
        col = len(lines[-1])
        return f"{row}.{col}"

    def get_content(self):
        return self.text.get("1.0", "end-1c")

    def set_content(self, text):
        self.text.delete("1.0", "end")
        self.text.insert("1.0", text)
        self._highlight()
        self.line_numbers._redraw()

    def insert_example(self, code):
        self.set_content(code)

# ── OUTPUT PANEL ───────────────────────────────────────────────────────────
class OutputPanel(tk.Frame):
    def __init__(self, parent, **kwargs):
        super().__init__(parent, bg=THEME["output_bg"], **kwargs)

        header = tk.Frame(self, bg=THEME["bg2"])
        header.pack(fill="x")
        tk.Label(header, text="  Output", font=("JetBrains Mono", 10, "bold"),
                 bg=THEME["bg2"], fg=THEME["fg_dim"]).pack(side="left", pady=4)

        self.clear_btn = tk.Button(
            header, text="Clear", command=self.clear,
            font=("JetBrains Mono", 9),
            bg=THEME["bg3"], fg=THEME["fg_dim"],
            relief="flat", bd=0, padx=8, pady=2,
            activebackground=THEME["selection"],
            activeforeground=THEME["fg"],
            cursor="hand2",
        )
        self.clear_btn.pack(side="right", padx=6, pady=4)

        self.text = tk.Text(
            self,
            font=("JetBrains Mono", 11),
            bg=THEME["output_bg"],
            fg=THEME["fg"],
            relief="flat",
            bd=0,
            padx=12,
            pady=8,
            state="disabled",
            wrap="word",
        )
        self.text.pack(fill="both", expand=True)
        self.text.tag_configure("error",   foreground=THEME["error_fg"])
        self.text.tag_configure("info",    foreground=THEME["info_fg"])
        self.text.tag_configure("success", foreground=THEME["green"])
        self.text.tag_configure("prompt",  foreground=THEME["accent"])

        scroll = tk.Scrollbar(self, command=self.text.yview)
        self.text.configure(yscrollcommand=scroll.set)

    def write(self, text, tag=None):
        self.text.configure(state="normal")
        if tag:
            self.text.insert("end", text, tag)
        else:
            self.text.insert("end", text)
        self.text.see("end")
        self.text.configure(state="disabled")

    def clear(self):
        self.text.configure(state="normal")
        self.text.delete("1.0", "end")
        self.text.configure(state="disabled")

# ── EXAMPLES PANEL ─────────────────────────────────────────────────────────
EXAMPLES = {
    "Hello World": "hello.ai",
    "0.1 + 0.2 = 0.3": "classic.ai",
    "Money & Tax":     "money.ai",
    "Division":        "division.ai",
    "Fibonacci":       "fibonacci.ai",
    "Compound Interest": "compound.ai",
}

class ExamplesPanel(tk.Frame):
    def __init__(self, parent, on_load, examples_dir, **kwargs):
        super().__init__(parent, bg=THEME["bg2"], **kwargs)
        self.on_load      = on_load
        self.examples_dir = examples_dir

        header = tk.Frame(self, bg=THEME["bg2"])
        header.pack(fill="x", padx=8, pady=(10, 4))
        tk.Label(header, text="Examples", font=("JetBrains Mono", 10, "bold"),
                 bg=THEME["bg2"], fg=THEME["accent"]).pack(anchor="w")

        tk.Frame(self, bg=THEME["bg3"], height=1).pack(fill="x", padx=8, pady=4)

        for label, filename in EXAMPLES.items():
            btn = tk.Button(
                self,
                text=f"  {label}",
                anchor="w",
                font=("JetBrains Mono", 10),
                bg=THEME["bg2"],
                fg=THEME["fg"],
                relief="flat",
                bd=0,
                padx=8,
                pady=6,
                activebackground=THEME["selection"],
                activeforeground=THEME["accent"],
                cursor="hand2",
                command=lambda f=filename: self._load(f),
            )
            btn.pack(fill="x", padx=4)
            btn.bind("<Enter>", lambda e, b=btn: b.config(bg=THEME["bg3"]))
            btn.bind("<Leave>", lambda e, b=btn: b.config(bg=THEME["bg2"]))

        tk.Frame(self, bg=THEME["bg2"]).pack(fill="both", expand=True)

        info = tk.Label(
            self,
            text="AI Language\nAbu Development Kit",
            font=("JetBrains Mono", 8),
            bg=THEME["bg2"],
            fg=THEME["fg_dim"],
            justify="center",
        )
        info.pack(pady=12)

    def _load(self, filename):
        path = os.path.join(self.examples_dir, filename)
        if os.path.exists(path):
            with open(path, "r") as f:
                self.on_load(f.read(), path)
        else:
            messagebox.showerror("Not found", f"Example file not found:\n{path}")

# ── MAIN IDLE ──────────────────────────────────────────────────────────────
class AIDLE(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("AI IDLE — Abu Development Kit")
        self.geometry("1280x800")
        self.configure(bg=THEME["bg"])
        self.minsize(800, 500)

        # Resolve paths
        self.script_dir   = os.path.dirname(os.path.abspath(__file__))
        self.examples_dir = os.path.join(os.path.dirname(self.script_dir), "examples")
        self.ai_binary    = self._find_ai_binary()

        self.current_file = None
        self._run_process = None

        self._build_ui()
        self._load_example("hello.ai")

        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _find_ai_binary(self):
        candidates = [
            os.path.join(self.script_dir, "..", "ai-compiler", "ai"),
            os.path.join(self.script_dir, "..", "ai-compiler", "ai.exe"),
            "ai",
        ]
        for c in candidates:
            path = os.path.normpath(c)
            if os.path.isfile(path) and os.access(path, os.X_OK):
                return path
        return None

    def _build_ui(self):
        # ── TOOLBAR ────────────────────────────────────────────────────────
        toolbar = tk.Frame(self, bg=THEME["bg2"], height=44)
        toolbar.pack(fill="x")
        toolbar.pack_propagate(False)

        # Logo
        tk.Label(toolbar, text=" AI ", font=("JetBrains Mono", 14, "bold"),
                 bg=THEME["accent"], fg=THEME["bg"], padx=6).pack(side="left", pady=6, padx=(8, 0))
        tk.Label(toolbar, text="IDLE", font=("JetBrains Mono", 12),
                 bg=THEME["bg2"], fg=THEME["fg"]).pack(side="left", padx=(6, 20), pady=6)

        # File name label
        self.file_label = tk.Label(
            toolbar, text="untitled.ai",
            font=("JetBrains Mono", 10),
            bg=THEME["bg2"], fg=THEME["fg_dim"],
        )
        self.file_label.pack(side="left", pady=6)

        # Run button
        self.run_btn = tk.Button(
            toolbar, text="▶  Run",
            font=("JetBrains Mono", 11, "bold"),
            bg=THEME["green"], fg=THEME["bg"],
            relief="flat", bd=0, padx=16, pady=4,
            activebackground=THEME["teal"],
            activeforeground=THEME["bg"],
            cursor="hand2",
            command=self._run,
        )
        self.run_btn.pack(side="right", padx=(0, 8), pady=5)

        # Stop button
        self.stop_btn = tk.Button(
            toolbar, text="■  Stop",
            font=("JetBrains Mono", 11, "bold"),
            bg=THEME["bg3"], fg=THEME["fg_dim"],
            relief="flat", bd=0, padx=12, pady=4,
            activebackground=THEME["red"],
            activeforeground=THEME["bg"],
            cursor="hand2",
            command=self._stop,
            state="disabled",
        )
        self.stop_btn.pack(side="right", padx=4, pady=5)

        # File buttons
        for label, cmd in [("New", self._new), ("Open", self._open), ("Save", self._save), ("Save As", self._save_as)]:
            b = tk.Button(
                toolbar, text=label,
                font=("JetBrains Mono", 10),
                bg=THEME["bg3"], fg=THEME["fg"],
                relief="flat", bd=0, padx=10, pady=4,
                activebackground=THEME["selection"],
                activeforeground=THEME["fg"],
                cursor="hand2",
                command=cmd,
            )
            b.pack(side="right", padx=2, pady=5)

        # ── MAIN PANES ─────────────────────────────────────────────────────
        main_pane = tk.PanedWindow(self, orient="horizontal",
                                   bg=THEME["bg"], sashwidth=4,
                                   sashrelief="flat", sashpad=0)
        main_pane.pack(fill="both", expand=True)

        # Examples sidebar
        self.examples_panel = ExamplesPanel(
            main_pane,
            on_load=self._load_from_example,
            examples_dir=self.examples_dir,
            width=180,
        )
        main_pane.add(self.examples_panel, minsize=160, width=190)

        # Editor + output vertical split
        right_pane = tk.PanedWindow(main_pane, orient="vertical",
                                    bg=THEME["bg"], sashwidth=4,
                                    sashrelief="flat", sashpad=0)
        main_pane.add(right_pane, minsize=400)

        # Editor
        self.editor = AIEditor(right_pane)
        right_pane.add(self.editor, minsize=200)

        # Output
        self.output = OutputPanel(right_pane, height=200)
        right_pane.add(self.output, minsize=100, height=220)

        # ── STATUS BAR ─────────────────────────────────────────────────────
        status = tk.Frame(self, bg=THEME["bg2"], height=22)
        status.pack(fill="x")
        status.pack_propagate(False)

        self.status_label = tk.Label(
            status, text="Ready",
            font=("JetBrains Mono", 9),
            bg=THEME["bg2"], fg=THEME["fg_dim"],
            anchor="w",
        )
        self.status_label.pack(side="left", padx=8)

        ai_status = tk.Label(
            status,
            text=f"  ai binary: {'found' if self.ai_binary else 'not built — run make in ai-compiler/'}  ",
            font=("JetBrains Mono", 9),
            bg=THEME["bg2"],
            fg=THEME["green"] if self.ai_binary else THEME["yellow"],
        )
        ai_status.pack(side="right", padx=4)

        # Keyboard shortcuts
        self.bind("<Control-Return>", lambda e: self._run())
        self.bind("<Control-s>",      lambda e: self._save())
        self.bind("<Control-o>",      lambda e: self._open())
        self.bind("<Control-n>",      lambda e: self._new())

    # ── FILE OPS ───────────────────────────────────────────────────────────
    def _load_example(self, filename):
        path = os.path.join(self.examples_dir, filename)
        if os.path.exists(path):
            with open(path) as f:
                self.editor.set_content(f.read())
            self.current_file = path
            self.file_label.config(text=os.path.basename(path))

    def _load_from_example(self, code, path):
        self.editor.set_content(code)
        self.current_file = None  # treat as unsaved copy
        self.file_label.config(text=f"{os.path.basename(path)} (example)")
        self.output.clear()
        self.status_label.config(text=f"Loaded example: {os.path.basename(path)}")

    def _new(self):
        self.editor.set_content("AI->VM\n\n<mainloop>\n    \n<mainloop>\n")
        self.current_file = None
        self.file_label.config(text="untitled.ai")
        self.output.clear()

    def _open(self):
        path = filedialog.askopenfilename(
            filetypes=[("AI source", "*.ai"), ("All files", "*.*")],
            initialdir=self.examples_dir,
        )
        if path:
            with open(path) as f:
                self.editor.set_content(f.read())
            self.current_file = path
            self.file_label.config(text=os.path.basename(path))

    def _save(self):
        if self.current_file and not self.current_file.endswith("(example)"):
            with open(self.current_file, "w") as f:
                f.write(self.editor.get_content())
            self.status_label.config(text=f"Saved: {self.current_file}")
        else:
            self._save_as()

    def _save_as(self):
        path = filedialog.asksaveasfilename(
            defaultextension=".ai",
            filetypes=[("AI source", "*.ai"), ("All files", "*.*")],
            initialdir=self.examples_dir,
        )
        if path:
            with open(path, "w") as f:
                f.write(self.editor.get_content())
            self.current_file = path
            self.file_label.config(text=os.path.basename(path))
            self.status_label.config(text=f"Saved: {path}")

    def _get_run_path(self):
        """Save content to a temp file and return its path."""
        import tempfile
        tmp = tempfile.NamedTemporaryFile(
            suffix=".ai", delete=False,
            mode="w", encoding="utf-8",
        )
        tmp.write(self.editor.get_content())
        tmp.close()
        return tmp.name

    # ── RUN ────────────────────────────────────────────────────────────────
    def _run(self):
        if self._run_process and self._run_process.poll() is None:
            return  # already running

        self.output.clear()
        self.output.write("─" * 60 + "\n", "info")

        if not self.ai_binary:
            self.output.write(
                "⚠  AI binary not found.\n"
                "   Build it first:\n"
                "   cd AI/ai-compiler && make\n\n"
                "   Running syntax check only (no binary).\n",
                "error",
            )
            self._syntax_check_only()
            return

        # Save to temp file
        src_path = self._get_run_path()
        self.output.write(f"▶  Running {os.path.basename(src_path)}\n", "prompt")
        self.output.write("─" * 60 + "\n", "info")

        self.run_btn.config(state="disabled", bg=THEME["bg3"], fg=THEME["fg_dim"])
        self.stop_btn.config(state="normal",  bg=THEME["red"], fg=THEME["bg"])
        self.status_label.config(text="Running…")

        def target():
            try:
                self._run_process = subprocess.Popen(
                    [self.ai_binary, src_path],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                )
                stdout, stderr = self._run_process.communicate(timeout=30)
                rc = self._run_process.returncode

                def show():
                    if stdout:
                        self.output.write(stdout)
                    if stderr:
                        for line in stderr.splitlines():
                            tag = "info" if line.startswith("Suggestion:") else "error"
                            self.output.write(line + "\n", tag)
                    self.output.write("─" * 60 + "\n", "info")
                    if rc == 0:
                        self.output.write("✓  Done\n", "success")
                        self.status_label.config(text="Done")
                    else:
                        self.output.write(f"✗  Exit code {rc}\n", "error")
                        self.status_label.config(text=f"Error (exit {rc})")
                    self._reset_run_btns()

                self.after(0, show)
            except subprocess.TimeoutExpired:
                self._run_process.kill()
                self.after(0, lambda: self.output.write("⚠  Timed out (30s)\n", "error"))
                self.after(0, self._reset_run_btns)
            except Exception as e:
                self.after(0, lambda: self.output.write(f"⚠  {e}\n", "error"))
                self.after(0, self._reset_run_btns)
            finally:
                try:
                    os.unlink(src_path)
                except Exception:
                    pass

        threading.Thread(target=target, daemon=True).start()

    def _syntax_check_only(self):
        """Highlight basic syntax errors without the binary."""
        code = self.editor.get_content()
        issues = []
        for i, line in enumerate(code.splitlines(), 1):
            stripped = line.strip()
            if stripped and not stripped.startswith("--") and not stripped.startswith("/*"):
                if stripped.count("$") % 2 != 0:
                    issues.append(f"  Line {i}: unclosed string literal ($)")
        if issues:
            self.output.write("Syntax issues found:\n", "error")
            for issue in issues:
                self.output.write(issue + "\n", "error")
        else:
            self.output.write("No obvious syntax issues detected.\n", "success")
            self.output.write("(Build the ai binary to run the program.)\n", "info")

    def _stop(self):
        if self._run_process and self._run_process.poll() is None:
            self._run_process.terminate()
            self.output.write("\n⚠  Stopped by user\n", "error")
            self.status_label.config(text="Stopped")
        self._reset_run_btns()

    def _reset_run_btns(self):
        self.run_btn.config(state="normal", bg=THEME["green"], fg=THEME["bg"])
        self.stop_btn.config(state="disabled", bg=THEME["bg3"], fg=THEME["fg_dim"])

    def _on_close(self):
        if self._run_process and self._run_process.poll() is None:
            self._run_process.terminate()
        self.destroy()


if __name__ == "__main__":
    app = AIDLE()
    app.mainloop()
