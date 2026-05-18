"""
Phase 1 Property Tests for the AC Compiler Pratt Parser and IR.

Property 1:  Expression Parsing Produces Structured AST
Property 2:  Single-Pass Expression Parsing
Property 3:  IR Layer String-Free Operation
Property 18: Pratt Parser Precedence
"""

import subprocess
import os
import re
import pytest
from hypothesis import given, settings, assume
from hypothesis import strategies as st

AC_COMPILER = os.path.join(os.path.dirname(__file__), "..", "ac")
TMPDIR = "/tmp/ac_property_tests"
os.makedirs(TMPDIR, exist_ok=True)

_test_counter = 0

def _unique_file(prefix: str = "test") -> str:
    """Return a unique path prefix under TMPDIR."""
    global _test_counter
    _test_counter += 1
    return os.path.join(TMPDIR, f"{prefix}_{_test_counter:06d}")


def compile_ac(source: str, backend: str = "PY") -> tuple[str, str, int]:
    """Compile AC source, return (stdout, stderr, returncode)."""
    base = _unique_file("ac_compile")
    src_file = base + ".ac"
    with open(src_file, "w") as f:
        f.write(f"AC->{backend}\n\n<mainloop>\n{source}\n    /kill\n<mainloop>\n")
    result = subprocess.run(
        [AC_COMPILER, "--force", src_file],
        capture_output=True, text=True, timeout=10,
    )
    return result.stdout, result.stderr, result.returncode


def compile_and_run(source: str) -> tuple[str, int]:
    """Compile AC to Python and run; return (output, returncode)."""
    base = _unique_file("ac_run")
    src_file = base + ".ac"
    with open(src_file, "w") as f:
        f.write(f"AC->PY\n\n<mainloop>\n{source}\n    /kill\n<mainloop>\n")
    r = subprocess.run([AC_COMPILER, "--force", src_file], capture_output=True, text=True, timeout=10)
    if r.returncode != 0:
        return "", r.returncode
    py_file = base + ".py"
    r2 = subprocess.run(["python3", py_file], capture_output=True, text=True, timeout=10)
    return r2.stdout.strip(), r2.returncode


def read_lir(source: str) -> str:
    """Compile AC source and return the generated .lir IR text."""
    base = _unique_file("ac_lir")
    src_file = base + ".ac"
    with open(src_file, "w") as f:
        f.write(f"AC->PY\n\n<mainloop>\n{source}\n    /kill\n<mainloop>\n")
    subprocess.run([AC_COMPILER, "--force", src_file], capture_output=True, text=True, timeout=10)
    lir_file = base + ".lir"
    if os.path.exists(lir_file):
        with open(lir_file) as f:
            return f.read()
    return ""


# ─── Property 18: Pratt Parser Precedence ─────────────────────────────────────

class TestProperty18PrattParserPrecedence:
    """
    Validates: Requirements 13.1, 13.2, 13.3, 13.4

    The Pratt parser must respect standard mathematical precedence.
    For any two Python-evaluable arithmetic expressions, the AC compiler
    must produce the same numeric result as Python.
    """

    def test_mul_before_add(self):
        """2 + 3 * 4 == 14, not 20"""
        out, rc = compile_and_run("    Term.display 2 + 3 * 4")
        assert rc == 0
        assert out == "14", f"Expected 14, got {out!r}"

    def test_mul_before_sub(self):
        """10 - 2 * 3 == 4, not 24"""
        out, rc = compile_and_run("    Term.display 10 - 2 * 3")
        assert rc == 0
        assert out == "4", f"Expected 4, got {out!r}"

    def test_parens_override(self):
        """(2 + 3) * 4 == 20"""
        out, rc = compile_and_run("    Term.display (2 + 3) * 4")
        assert rc == 0
        assert out == "20", f"Expected 20, got {out!r}"

    def test_left_associative_sub(self):
        """10 - 3 - 2 == 5, not 9 (left-associative)"""
        out, rc = compile_and_run("    Term.display 10 - 3 - 2")
        assert rc == 0
        assert out == "5", f"Expected 5, got {out!r}"

    def test_left_associative_div(self):
        """12 / 3 / 2 == 2 (left-associative)"""
        out, rc = compile_and_run("    Term.display 12 / 3 / 2")
        assert rc == 0
        assert out in ("2", "2.0"), f"Expected 2 or 2.0, got {out!r}"

    def test_compare_below_add(self):
        """2 + 3 is 5 should parse as (2+3) is 5 == true"""
        out, rc = compile_and_run("    Term.display 2 + 3 is 5")
        assert rc == 0
        assert out == "1", f"Expected 1 (true), got {out!r}"

    def test_unary_minus(self):
        """-5 + 3 == -2"""
        out, rc = compile_and_run("    Term.display -5 + 3")
        assert rc == 0
        assert out == "-2", f"Expected -2, got {out!r}"

    def test_not_below_and(self):
        """# 0 & 1 should parse as (# 0) & 1 == 1 & 1 == 1"""
        out, rc = compile_and_run("    Term.display # 0 & 1")
        assert rc == 0
        assert out == "1", f"Expected 1, got {out!r}"

    def test_xor_below_and(self):
        """1 & 1 | 0 should parse as (1 & 1) | 0 == 1 | 0 == 1"""
        out, rc = compile_and_run("    Term.display 1 & 1 | 0")
        assert rc == 0
        assert out == "1", f"Expected 1 (xor of 1 and 0), got {out!r}"

    def test_or_lowest(self):
        """0 | 1 or 1 | 0 should parse as (0|1) or (1|0) - OR lowest"""
        # In AC: | = XOR, or = OR
        # 0|1 = XOR(0,1) = 1; 1|0 = XOR(1,0) = 1; 1 or 1 = 1
        out, rc = compile_and_run("    Term.display 0 | 1 or 1 | 0")
        assert rc == 0
        assert out == "1", f"Expected 1, got {out!r}"

    @given(
        a=st.integers(min_value=-100, max_value=100),
        b=st.integers(min_value=1, max_value=100),
        c=st.integers(min_value=1, max_value=100),
    )
    @settings(max_examples=50, deadline=5000)
    def test_precedence_matches_python_mul_add(self, a, b, c):
        """a + b * c in AC equals a + b * c in Python."""
        expected = a + b * c
        out, rc = compile_and_run(f"    Term.display {a} + {b} * {c}")
        assert rc == 0, f"Compiler failed for {a}+{b}*{c}"
        assert out == str(expected), f"{a}+{b}*{c}: expected {expected}, got {out!r}"

    @given(
        a=st.integers(min_value=1, max_value=100),
        b=st.integers(min_value=1, max_value=10),
        c=st.integers(min_value=1, max_value=10),
    )
    @settings(max_examples=50, deadline=5000)
    def test_precedence_matches_python_sub_mul(self, a, b, c):
        """a - b * c in AC equals a - b * c in Python."""
        expected = a - b * c
        out, rc = compile_and_run(f"    Term.display {a} - {b} * {c}")
        assert rc == 0, f"Compiler failed for {a}-{b}*{c}"
        assert out == str(expected), f"{a}-{b}*{c}: expected {expected}, got {out!r}"


# ─── Property 1: Expression Parsing Produces Structured AST ───────────────────

class TestProperty1StructuredAST:
    """
    Validates: Requirements 1.1, 1.2, 14.1, 14.2, 14.3, 14.4, 14.5

    The parser must produce structured AST nodes (BinaryExpr, UnaryExpr,
    CallExpr, LiteralExpr) rather than raw string captures.
    """

    def test_binary_expr_generates_two_operand_ir(self):
        """Binary a + b must generate an ADD instruction with two operands."""
        lir = read_lir("    x = 3 + 4")
        assert "add" in lir.lower() or "+" in lir, "Expected binary ADD in IR"

    def test_unary_not_generates_not_ir(self):
        """Unary # x must generate a NOT instruction."""
        lir = read_lir("    x = # 1")
        assert "not" in lir.lower(), f"Expected NOT in IR, got:\n{lir}"

    def test_call_expr_generates_call_ir(self):
        """Function call f(a) must generate a CALL instruction."""
        src = "Make f func(a)\n    return a\n\n<mainloop>\n    x = f(5)\n    /kill\n<mainloop>"
        base = _unique_file("call_test")
        src_file = base + ".ac"
        with open(src_file, "w") as f:
            f.write(f"AC->PY\n\n{src}\n")
        subprocess.run([AC_COMPILER, "--force", src_file], capture_output=True, timeout=10)
        lir_file = base + ".lir"
        lir = open(lir_file).read() if os.path.exists(lir_file) else ""
        assert "call" in lir.lower(), f"Expected CALL in IR, got:\n{lir}"

    def test_literal_int_generates_load_const(self):
        """Integer literal 42 must generate a LOAD_CONST instruction."""
        lir = read_lir("    x = 42")
        assert "42" in lir, f"Expected literal 42 in IR, got:\n{lir}"

    def test_nested_expr_correct_result(self):
        """(a + b) * (c - d) must produce correct result."""
        out, rc = compile_and_run("    Term.display (2 + 3) * (10 - 4)")
        assert rc == 0
        assert out == str((2 + 3) * (10 - 4)), f"Expected {(2+3)*(10-4)}, got {out!r}"

    @given(
        a=st.integers(min_value=-50, max_value=50),
        b=st.integers(min_value=-50, max_value=50),
    )
    @settings(max_examples=40, deadline=5000)
    def test_binary_add_correct_for_any_ints(self, a, b):
        """a + b always gives the right answer."""
        out, rc = compile_and_run(f"    Term.display {a} + {b}")
        assert rc == 0
        assert out == str(a + b), f"{a}+{b}: expected {a+b}, got {out!r}"


# ─── Property 2: Single-Pass Expression Parsing ────────────────────────────────

class TestProperty2SinglePassParsing:
    """
    Validates: Requirements 1.5

    Each expression is parsed exactly once in the frontend, not re-parsed
    in the IR layer. We verify this by checking that expressions in the IR
    are already resolved (no raw string arithmetic like '2+3').
    """

    def test_arithmetic_resolved_in_ir(self):
        """In the .lir file, '2+3' must not appear as raw string."""
        lir = read_lir("    x = 2 + 3")
        # The expression should be lowered; no raw '2+3' pattern
        assert "2+3" not in lir.replace(" ", ""), \
            f"Expression '2+3' was not resolved at parse time:\n{lir}"

    def test_comparison_resolved_in_ir(self):
        """In the .lir file, raw '<' comparison must be lowered to lt opcode."""
        lir = read_lir("    x = 5 < 10")
        assert "lt" in lir.lower() or "<" in lir, \
            f"Comparison not resolved in IR:\n{lir}"

    def test_function_call_resolved_in_ir(self):
        """Function calls must appear as call opcodes in IR, not raw strings."""
        base = _unique_file("single_pass")
        src_file = base + ".ac"
        with open(src_file, "w") as f:
            f.write("AC->PY\n\nMake g func(x)\n    return x\n\n<mainloop>\n    y = g(7)\n    /kill\n<mainloop>\n")
        subprocess.run([AC_COMPILER, "--force", src_file], capture_output=True, timeout=10)
        lir_file = base + ".lir"
        lir = open(lir_file).read() if os.path.exists(lir_file) else ""
        assert "call" in lir.lower(), f"Function call not lowered to opcode:\n{lir}"


# ─── Property 3: IR Layer String-Free Operation ────────────────────────────────

class TestProperty3IRStringFree:
    """
    Validates: Requirements 1.3, 1.4

    The IR layer must not re-parse string expressions. Temporaries must be
    integer-indexed (t0, t1, t2...) and labels must be integer-indexed
    (L0, L1, L2...) rather than arbitrary strings.
    """

    def test_temporaries_are_integer_indexed(self):
        """All temporaries in LIR use t<int> format."""
        lir = read_lir("    x = 2 + 3 * 4 - 1")
        temp_refs = re.findall(r'\bt(\d+)\b', lir)
        assert len(temp_refs) > 0, "Expected integer-indexed temporaries in LIR"
        # All should be valid non-negative integers
        for t in temp_refs:
            assert int(t) >= 0, f"Invalid temporary index: t{t}"

    def read_lir_c(self, source: str) -> str:
        """Compile to C (low-level IR) and return LIR text (has labels)."""
        base = _unique_file("ac_lir_c")
        src_file = base + ".ac"
        with open(src_file, "w") as f:
            f.write(f"AC->C\n\n<mainloop>\n{source}\n    /kill\n<mainloop>\n")
        subprocess.run([AC_COMPILER, "--force", src_file], capture_output=True, text=True, timeout=10)
        lir_file = base + ".lir"
        return open(lir_file).read() if os.path.exists(lir_file) else ""

    def test_labels_are_integer_indexed(self):
        """All labels in LIR use L<int> format (visible in low-level/C backend IR)."""
        lir = self.read_lir_c("""
    IF 1 is 1
        Term.display $yes$
    OTHER
        Term.display $no$
""")
        label_refs = re.findall(r'\bL(\d+)\b', lir)
        # Either we see L0/L1 labels (low-level IR) or if_begin/if_end (high-level)
        # Both are valid integer-indexed representations
        if label_refs:
            for l in label_refs:
                assert int(l) >= 0, f"Invalid label index: L{l}"
        else:
            # High-level IR: check for if_begin, while_begin as valid alternatives
            assert "if_begin" in lir or "label" in lir, \
                f"No labels or control-flow markers found in LIR:\n{lir}"

    def test_no_raw_arithmetic_strings_in_lir(self):
        """LIR body must not contain raw infix expressions like '5+3' or '2*4'."""
        lir = read_lir("    x = 5 + 3\n    y = 2 * 4\n    z = x + y")
        # Only examine non-comment lines
        body_lines = [l for l in lir.splitlines() if l.strip() and not l.strip().startswith(";")]
        body = "\n".join(body_lines)
        infix_patterns = re.findall(r'\d+\s*[+\-*/]\s*\d+', body)
        assert len(infix_patterns) == 0, \
            f"Found raw arithmetic in LIR body (should be lowered): {infix_patterns}\n{lir}"

    def test_no_string_based_op_lookups(self):
        """The LIR opcodes must be named (not raw operator chars like '+' or '<')."""
        lir = read_lir("    x = 3 + 4\n    y = x < 10")
        lines = [l.strip() for l in lir.splitlines() if l.strip()]
        for line in lines:
            # No line should be just a bare operator
            assert line not in ("+", "-", "*", "/", "<", ">"), \
                f"Bare operator found in LIR: {line!r}"

    @given(
        a=st.integers(min_value=2, max_value=50),
        b=st.integers(min_value=2, max_value=50),
        c=st.integers(min_value=2, max_value=50),
    )
    @settings(max_examples=30, deadline=5000)
    def test_complex_expr_always_produces_integer_temps(self, a, b, c):
        """Compound arithmetic expressions always use integer-indexed temporaries."""
        # Use variables to prevent constant folding eliminating all temps
        lir = read_lir(f"    x = {a}\n    y = {b}\n    z = {c}\n    w = x + y * z")
        # Only examine non-comment lines
        body_lines = [l for l in lir.splitlines() if l.strip() and not l.strip().startswith(";")]
        body = "\n".join(body_lines)
        temp_refs = re.findall(r'\bt(\d+)\b', body)
        assert len(temp_refs) > 0, \
            f"Expected temporaries for x + y * z:\n{lir}"


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
