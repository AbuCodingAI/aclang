#!/usr/bin/env python3
"""
test_backends.py — AC compiler cross-backend regression tests.

Compiles small AC snippets to every installed backend, runs the output,
and asserts the expected stdout value.

Requirements: python3, node, javac/java, go, rustc (only installed ones tested)
Usage: python3 test/test_backends.py  (from project root, or from test/ dir)
"""

import subprocess
import sys
import os
import tempfile
import shutil

# Resolve compiler path relative to this file
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
COMPILER = os.path.join(PROJECT_ROOT, "ac-compiler", "ac")
if not os.path.isfile(COMPILER):
    COMPILER = os.path.join(PROJECT_ROOT, "ac")

PASS = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"
SKIP = "\033[33mSKIP\033[0m"


def has_tool(*names):
    return all(shutil.which(n) for n in names)


def run(cmd, cwd=None, input_text=None):
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)
    return r.returncode, r.stdout.strip(), r.stderr.strip()


def compile_ac(src_text, backend, tmpdir):
    src = os.path.join(tmpdir, "prog.ac")
    with open(src, "w") as f:
        f.write(f"AC->{backend}\n{src_text}")
    rc, out, err = run([COMPILER, src, "--force"], cwd=tmpdir)
    return rc, out, err, tmpdir


# Each test case: (name, AC body, expected_stdout)
# Body is everything after the AC->BACKEND header line.
TESTS = [
    (
        "arithmetic",
        "<mainloop>\n    x = 10\n    y = 3\n    z = x + y\n    Term.display z\n    /kill\n<mainloop>",
        "13",
    ),
    (
        "constant_folding",
        "<mainloop>\n    x = 3 + 4\n    y = 10 - 2\n    Term.display x\n    Term.display y\n    /kill\n<mainloop>",
        "7\n8",
    ),
    (
        "while_loop",
        "<mainloop>\n    i = 1\n    s = 0\n    WHILST i #> 5\n        s += i\n        i += 1\n    Term.display s\n    /kill\n<mainloop>",
        "15",
    ),
    (
        "if_else",
        "<mainloop>\n    x = 7\n    IF x > 5\n        Term.display 1\n    OTHER\n        Term.display 0\n    /kill\n<mainloop>",
        "1",
    ),
    (
        "function_call",
        "Make twice func(n)\n    return n + n\n\n<mainloop>\n    r = twice(6)\n    Term.display r\n    /kill\n<mainloop>",
        "12",
    ),
    (
        "for_loop",
        "<mainloop>\n    items = [1, 2, 3, 4, 5]\n    total = 0\n    FOR each in items\n        total += each\n    Term.display total\n    /kill\n<mainloop>",
        "15",
    ),
    (
        "float_arithmetic",
        "<mainloop>\n    x = 3.14\n    y = 2.5\n    z = x + y\n    Term.display z\n    /kill\n<mainloop>",
        # Python/JS/Java/Go/Rust use shortest-repr; C/C++ use %.17g — same double, different decimal
        "5.640000000000001",
    ),
    (
        "is_operator",
        "<mainloop>\n    a = 5\n    b = 5\n    IF a is b\n        Term.display 1\n    OTHER\n        Term.display 0\n    /kill\n<mainloop>",
        "1",
    ),
    (
        "neq_operator",
        "<mainloop>\n    a = 5\n    b = 6\n    IF a #= b\n        Term.display 1\n    OTHER\n        Term.display 0\n    /kill\n<mainloop>",
        "1",
    ),
    (
        "multiply_operator",
        "<mainloop>\n    x = 6\n    y = 7\n    z = x @ y\n    Term.display z\n    /kill\n<mainloop>",
        "42",
    ),
]

# Backend definitions: (name, file_ext, run_fn)
def make_backends(tmpdir):
    prog_base = os.path.join(tmpdir, "prog")

    def run_py():
        return run(["python3", prog_base + ".py"])

    def run_js():
        return run(["node", prog_base + ".js"])

    def run_java():
        rc, out, err = run(["javac", prog_base + ".java"], cwd=tmpdir)
        if rc != 0:
            return rc, out, err
        return run(["java", "-cp", tmpdir, "prog"])

    def run_go():
        return run(["go", "run", prog_base + ".go"])

    def run_rs():
        bin_path = prog_base + "_rs"
        rc, out, err = run(["rustc", prog_base + ".rs", "-o", bin_path])
        if rc != 0:
            return rc, out, err
        return run([bin_path])

    def run_c():
        bin_path = prog_base + "_c"
        rc, out, err = run(["gcc", prog_base + ".c", "-o", bin_path])
        if rc != 0:
            return rc, out, err
        return run([bin_path])

    def run_cpp():
        bin_path = prog_base + "_cpp"
        rc, out, err = run(["g++", prog_base + ".cpp", "-o", bin_path])
        if rc != 0:
            return rc, out, err
        return run([bin_path])

    backends = [
        ("PY",   ".py",   has_tool("python3"),       run_py),
        ("JS",   ".js",   has_tool("node"),           run_js),
        ("Java", ".java", has_tool("javac", "java"),  run_java),
        ("GO",   ".go",   has_tool("go"),             run_go),
        ("RS",   ".rs",   has_tool("rustc"),          run_rs),
        ("C",    ".c",    has_tool("gcc"),            run_c),
        ("CPP",  ".cpp",  has_tool("g++"),            run_cpp),
    ]
    return backends


def run_tests():
    passed = failed = skipped = 0
    tmpdir = tempfile.mkdtemp(prefix="ac_test_")
    try:
        backends = make_backends(tmpdir)
        # C uses low-level IR — array-based for-loops not supported
        C_SKIP_TESTS = {"for_loop"}

        for test_name, body, expected in TESTS:
            for backend, ext, available, run_fn in backends:
                label = f"{test_name}/{backend}"
                if not available:
                    print(f"  {SKIP}  {label} (tool not installed)")
                    skipped += 1
                    continue

                # C backend uses low-level IR; skip array for-loop
                if backend == "C" and test_name in C_SKIP_TESTS:
                    print(f"  {SKIP}  {label} (C uses low-level IR, no array for-loop)")
                    skipped += 1
                    continue

                # Compile
                rc, _, err, td = compile_ac(body, backend, tmpdir)
                if rc != 0 and "error" in err.lower():
                    print(f"  {FAIL}  {label}: compile error: {err[:120]}")
                    failed += 1
                    continue

                # Run
                rc2, actual, stderr2 = run_fn()
                if actual == expected:
                    print(f"  {PASS}  {label}")
                    passed += 1
                else:
                    print(f"  {FAIL}  {label}: expected {expected!r}, got {actual!r}")
                    if stderr2:
                        print(f"         stderr: {stderr2[:120]}")
                    failed += 1

    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

    print(f"\n{'='*50}")
    print(f"Results: {passed} passed, {failed} failed, {skipped} skipped")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(run_tests())
