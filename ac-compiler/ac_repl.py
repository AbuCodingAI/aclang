#!/usr/bin/env python3

import subprocess
import tempfile
import shutil
import os
import readline
import atexit
import time

HISTORY_FILE = os.path.expanduser("~/.ac_history")
DEFAULT_COMPILER = os.environ.get("AC_COMPILER", "ac")


# ------------------------
# History
# ------------------------
def load_history():
    if os.path.exists(HISTORY_FILE):
        readline.read_history_file(HISTORY_FILE)


def save_history():
    readline.write_history_file(HISTORY_FILE)


# ------------------------
# REPL Core
# ------------------------
class ACRepl:
    def __init__(self):
        self.target = "PY"
        self.compiler = DEFAULT_COMPILER
        self.program = []
        self.temp_dir = tempfile.mkdtemp(prefix="ac_repl_")
        self.timing = False

    # ------------------------
    # Lifecycle
    # ------------------------
    def cleanup(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def reset(self):
        self.program = []
        print("Session reset.")

    # ------------------------
    # Internal helpers
    # ------------------------
    def _source_path(self):
        return os.path.join(self.temp_dir, "repl.ac")

    def _output_path(self):
        ext = {
            "PY": "py",
            "JS": "js",
            "BNY": "acb"
        }.get(self.target, self.target.lower())
        return os.path.join(self.temp_dir, f"repl.{ext}")

    def _write_program(self):
        with open(self._source_path(), "w") as f:
            f.write(f"AC->{self.target}\n")
            f.write("\n".join(self.program))
            f.write("\n")

    # ------------------------
    # Compile + Run
    # ------------------------
    def execute(self, code):
        self.program.append(code)

        self._write_program()

        try:
            # Compile
            compile_start = time.time()
            result = subprocess.run(
                [self.compiler, self._source_path(), "--target", self.target],
                capture_output=True,
                text=True
            )
            compile_time = time.time() - compile_start

            if result.returncode != 0:
                print("Compilation error:\n", result.stderr)
                self.program.pop()  # rollback
                return

            # Run
            run_start = time.time()

            cmd = {
                "PY": ["python3", self._output_path()],
                "JS": ["node", self._output_path()],
                "BNY": [self._output_path()],
            }.get(self.target)

            if not cmd:
                print(f"Execution for backend '{self.target}' not implemented.")
                self.program.pop()
                return

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True
            )

            run_time = time.time() - run_start

            if result.stdout:
                print(result.stdout, end="")

            if result.stderr:
                print("Runtime error:\n", result.stderr)

            if self.timing:
                print(f"[compile: {compile_time:.3f}s | run: {run_time:.3f}s]")

        except FileNotFoundError:
            print("Error: AC compiler not found.")
            self.program.pop()
        except Exception as e:
            print("Error:", e)
            self.program.pop()

    # ------------------------
    # Commands
    # ------------------------
    def handle_command(self, line):
        parts = line[1:].split()

        if not parts:
            return

        cmd = parts[0]

        if cmd == "help":
            print("""
Commands:
  :help           Show this message
  :exit           Exit REPL
  :reset          Clear session
  :target <name>  Set backend (PY, JS, etc.)
  :time on|off    Toggle timing
""")

        elif cmd == "exit":
            raise EOFError

        elif cmd == "reset":
            self.reset()

        elif cmd == "target":
            if len(parts) < 2:
                print("Usage: :target PY|JS|...")
            else:
                self.target = parts[1].upper()
                print(f"Target set to {self.target}")

        elif cmd == "time":
            if len(parts) < 2:
                print("Usage: :time on|off")
            else:
                self.timing = parts[1].lower() == "on"
                print(f"Timing {'enabled' if self.timing else 'disabled'}")

        else:
            print(f"Unknown command: {cmd}")


# ------------------------
# Main Loop
# ------------------------
def main():
    load_history()
    atexit.register(save_history)

    repl = ACRepl()
    atexit.register(repl.cleanup)

    print("=" * 50)
    print("AC REPL (Production)")
    print("=" * 50)
    print("Type :help for commands\n")

    buffer = []

    while True:
        try:
            prompt = "ac> " if not buffer else "... "
            line = input(prompt).strip()

            if not line:
                continue

            if line.startswith(":"):
                repl.handle_command(line)
                continue

            buffer.append(line)

            # simple execution model (can upgrade later)
            code = "\n".join(buffer)
            repl.execute(code)
            buffer.clear()

        except EOFError:
            print("\nGoodbye.")
            break
        except KeyboardInterrupt:
            print("\nCancelled.")
            buffer.clear()


if __name__ == "__main__":
    main()