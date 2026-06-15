import re
import fnmatch
from datetime import date, datetime, timedelta
from itertools import product as cart_product
from abudb import Database


# ─── Structured errors ────────────────────────────────────────────────────────

class JaSQLError(Exception):
    """
    Preposterous: ErrorName found ln:N Char:M
      statement text
      ^
    """
    def __init__(self, kind, token=None, statement=None, line=None):
        self.kind      = kind
        self.token     = token        # offending token (used to find column)
        self.statement = statement
        self.line      = line

    def _col(self):
        if self.token and self.statement:
            idx = self.statement.find(str(self.token))
            return idx if idx >= 0 else None
        return None

    def __str__(self):
        loc = f"ln:{self.line or '?'}"
        col = self._col()
        if col is not None:
            loc += f" Char:{col}"
        lines = [f"Preposterous: {self.kind} found {loc}"]
        stmt  = (self.statement or '').strip()
        if stmt:
            lines.append(f"  {stmt}")
            if col is not None:
                lines.append(f"  {' ' * col}^")
        return '\n'.join(lines)


# ─── Value parsing ────────────────────────────────────────────────────────────

def _parse_literal(s):
    s = s.strip()
    # nil — explicit null, not an empty set
    if s.lower() == 'nil':   return None,  True
    # bool literals
    if s.lower() == 'true':  return True,  True
    if s.lower() == 'false': return False, True
    # quoted strings
    if (s.startswith('"') and s.endswith('"')) or (s.startswith("'") and s.endswith("'")):
        return s[1:-1], True
    # datetime:  dd.mm.yy hh:mm
    m = re.match(r'^(\d{2})\.(\d{2})\.(\d{2})\s+(\d{2}):(\d{2})$', s)
    if m:
        d,mo,y,h,mi = int(m.group(1)),int(m.group(2)),int(m.group(3)),int(m.group(4)),int(m.group(5))
        return datetime(2000+y,mo,d,h,mi), True
    # date:  dd.mm.yy
    m = re.match(r'^(\d{2})\.(\d{2})\.(\d{2})$', s)
    if m:
        d,mo,y = int(m.group(1)),int(m.group(2)),int(m.group(3))
        return date(2000+y,mo,d), True
    # deltat:  hh:mm:ss
    m = re.match(r'^(\d{1,3}):(\d{2}):(\d{2})$', s)
    if m:
        return timedelta(hours=int(m.group(1)),minutes=int(m.group(2)),seconds=int(m.group(3))), True
    try:
        return int(s), True
    except ValueError:
        try:
            return float(s), True
        except ValueError:
            return s, False  # field reference


def _split_csv(s):
    """Split on commas, respecting quoted strings and parenthesised groups."""
    values, current, in_quote, qc, depth = [], [], False, None, 0
    for ch in s:
        if ch in ('"', "'") and not in_quote:
            in_quote, qc = True, ch
            current.append(ch)
        elif ch == qc and in_quote:
            in_quote = False
            current.append(ch)
        elif not in_quote:
            if ch == '(':
                depth += 1
                current.append(ch)
            elif ch == ')':
                depth -= 1
                current.append(ch)
            elif ch == ',' and depth == 0:
                values.append(''.join(current).strip())
                current = []
            else:
                current.append(ch)
        else:
            current.append(ch)
    if current:
        values.append(''.join(current).strip())
    return values


def _extract_parens(s):
    """Extract content of the outermost (...) if s starts with '('. Returns (inner, rest) or None."""
    if not s.startswith('('):
        return None
    depth, i = 0, 0
    for i, ch in enumerate(s):
        if ch == '(':   depth += 1
        elif ch == ')': depth -= 1
        if depth == 0:
            return s[1:i], s[i+1:]
    return None


# ─── Condition evaluation (db passed for subquery resolution) ─────────────────

_OP_RE  = re.compile(r'^(.+?)\s*(>=|<=|!=|=|>|<)\s*(.+)$')
_AGG_RE = re.compile(r'^(SUM|MEAN|MAX|MIN|SIZE)\((\w*|\*)\)$', re.I)


def _eval_atom(row, cond, db=None):
    cond = cond.strip()

    # NOT prefix
    if cond.startswith('!'):
        return not _eval_atom(row, cond[1:].strip(), db)

    # NIL:  field = NIL  |  field != NIL
    m = re.match(r'^(.+?)\s*(!=|=)\s*NIL$', cond, re.I)
    if m:
        val = row.get(m.group(1).strip())
        return (val is None) if m.group(2) == '=' else (val is not None)

    # LIKE:  field LIKE "pattern"
    m = re.match(r'^(.+?)\s+LIKE\s+(.+)$', cond, re.I)
    if m:
        field = m.group(1).strip()
        pattern, _ = _parse_literal(m.group(2).strip())
        val = row.get(field)
        if val is None:
            return False
        return fnmatch.fnmatch(str(val).lower(), str(pattern).replace('%', '*').lower())

    # MATCH / NOT MATCH:  full regex match (anchored)
    m = re.match(r'^(.+?)\s+(NOT\s+MATCH|MATCH)\s+(.+)$', cond, re.I)
    if m:
        field = m.group(1).strip()
        negated = 'NOT' in m.group(2).upper()
        pattern, _ = _parse_literal(m.group(3).strip())
        val = row.get(field)
        if val is None:
            return False
        try:
            result = bool(re.fullmatch(str(pattern), str(val)))
        except re.error:
            result = False
        return not result if negated else result

    # SEARCH / NOT SEARCH:  partial regex match (unanchored)
    m = re.match(r'^(.+?)\s+(NOT\s+SEARCH|SEARCH)\s+(.+)$', cond, re.I)
    if m:
        field = m.group(1).strip()
        negated = 'NOT' in m.group(2).upper()
        pattern, _ = _parse_literal(m.group(3).strip())
        val = row.get(field)
        if val is None:
            return False
        try:
            result = bool(re.search(str(pattern), str(val)))
        except re.error:
            result = False
        return not result if negated else result

    # IN RANGE:  field IN RANGE lo hi
    m = re.match(r'^(.+?)\s+IN\s+RANGE\s+(\S+)\s+(\S+)$', cond, re.I)
    if m:
        field = m.group(1).strip()
        lo, _ = _parse_literal(m.group(2))
        hi, _ = _parse_literal(m.group(3))
        val = row.get(field)
        if val is None:
            return False
        try:
            return lo <= val <= hi
        except TypeError:
            return False

    # IN:  field IN (val, ...) or field IN (TAKE ...)  — subquery supported
    m = re.match(r'^(.+?)\s+IN\s+(\(.+\))$', cond, re.I)
    if m:
        field  = m.group(1).strip()
        parens = m.group(2)
        result = _extract_parens(parens)
        if result:
            inner, _ = result
            inner = inner.strip()
            if re.match(r'^TAKE\b', inner, re.I) and db is not None:
                # Subquery: collect the first field of every result row
                sq_rows, sq_fields = _execute_take(db, inner, raw=True)
                values = [r.get(sq_fields[0]) for r in sq_rows] if sq_rows and sq_fields else []
            else:
                values = [_parse_literal(v.strip())[0] for v in _split_csv(inner)]
            return row.get(field) in values
        return False

    # Standard operator:  field op value_or_subquery_or_fieldref
    m = _OP_RE.match(cond)
    if m:
        field, op, rhs_raw = m.group(1).strip(), m.group(2), m.group(3).strip()

        # Subquery on RHS:  grade > (TAKE MEAN(grade) FROM students)
        if rhs_raw.startswith('(') and db is not None:
            result = _extract_parens(rhs_raw)
            if result:
                inner, _ = result
                inner = inner.strip()
                if re.match(r'^TAKE\b', inner, re.I):
                    sq_rows, sq_fields = _execute_take(db, inner, raw=True)
                    if not sq_rows or not sq_fields:
                        return False
                    rhs = sq_rows[0].get(sq_fields[0])
                    is_literal = rhs is not None
                else:
                    rhs, is_literal = _parse_literal(inner)
            else:
                rhs, is_literal = _parse_literal(rhs_raw)
        else:
            rhs, is_literal = _parse_literal(rhs_raw)

        lhs = row.get(field)
        # outer.field — correlated subquery reference to the outer row
        if lhs is None and field.startswith('outer.'):
            outer_row = getattr(db, '_outer_row', None)
            if outer_row is not None:
                lhs = outer_row.get(field[6:])
        if not is_literal:
            rhs = row.get(rhs_raw)
            if rhs is None and rhs_raw.startswith('outer.'):
                outer_row = getattr(db, '_outer_row', None)
                if outer_row is not None:
                    rhs = outer_row.get(rhs_raw[6:])
        if lhs is None or rhs is None:
            return False

        try:
            if isinstance(rhs, (int, float)) and not isinstance(lhs, (int, float)):
                lhs = type(rhs)(lhs)
            elif isinstance(lhs, (int, float)) and not isinstance(rhs, (int, float)):
                rhs = type(lhs)(rhs)
        except (ValueError, TypeError):
            pass

        if op == '=':  return lhs == rhs
        if op == '!=': return lhs != rhs
        if op == '>=': return lhs >= rhs
        if op == '<=': return lhs <= rhs
        if op == '>':  return lhs > rhs
        if op == '<':  return lhs < rhs

    return False


def eval_condition(row, cond, db=None):
    or_parts = re.split(r'\s*\|\s*', cond)
    if len(or_parts) > 1:
        return any(eval_condition(row, p, db) for p in or_parts)
    and_parts = re.split(r'\s*&\s*', cond)
    if len(and_parts) > 1:
        return all(eval_condition(row, p, db) for p in and_parts)
    return _eval_atom(row, cond.strip(), db)


# ─── Sorting & aggregates ─────────────────────────────────────────────────────

def apply_order(rows, field, direction):
    reverse = direction.upper() in ('DESC', 'ZYX')
    def key(r):
        v = r.get(field)
        return (1, '') if v is None else (0, v)
    return sorted(rows, key=key, reverse=reverse)


def apply_aggregate(rows, func, field):
    func = func.upper()
    if func == 'SIZE':
        return len(rows)
    values = [r.get(field) for r in rows if r.get(field) is not None]
    if not values:
        return None
    if func == 'SUM':  return sum(values)
    if func == 'MEAN': return round(sum(values) / len(values), 4)
    if func == 'MAX':  return max(values)
    if func == 'MIN':  return min(values)


# ─── Field expression engine ─────────────────────────────────────────────────

def _tokenize_expr(s):
    """Tokenize arithmetic expression into ('OP', ch) and ('VAL', str) tokens."""
    tokens, i = [], 0
    while i < len(s):
        c = s[i]
        if c in ' \t':
            i += 1
        elif c in '+-*/':
            tokens.append(('OP', c))
            i += 1
        elif c in ('"', "'"):
            j = i + 1
            while j < len(s) and s[j] != c:
                j += 1
            tokens.append(('VAL', s[i:j + 1]))
            i = j + 1
        else:
            j = i
            while j < len(s) and s[j] not in '+-*/ \t':
                j += 1
            tok = s[i:j].strip()
            if tok:
                tokens.append(('VAL', tok))
            i = j
    return tokens


def _parse_inline_if(expr):
    """
    Parse 'IF cond, true_val, ELSE false_val, END' — paren-aware.
    Returns (cond_str, true_str, false_str) or None.
    """
    m = re.match(r'^IF\s+(.+)\bEND\b\s*$', expr, re.I | re.S)
    if not m:
        return None
    body = m.group(1).strip().rstrip(',').strip()

    # Find ', ELSE ' with paren depth tracking
    depth, i, else_pos = 0, 0, None
    while i < len(body):
        c = body[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
        elif depth == 0 and body[i:i+2] == ', ' and body[i+2:].upper().startswith('ELSE '):
            else_pos = i
            break
        i += 1
    if else_pos is None:
        return None

    before    = body[:else_pos]
    false_val = body[else_pos + 7:].strip()   # skip ', ELSE '

    # Find first ',' in before (paren-aware)
    depth, i, comma_pos = 0, 0, None
    while i < len(before):
        c = before[i]
        if c == '(':   depth += 1
        elif c == ')': depth -= 1
        elif c == ',' and depth == 0:
            comma_pos = i
            break
        i += 1
    if comma_pos is None:
        return None

    return before[:comma_pos].strip(), before[comma_pos+1:].strip(), false_val


def _eval_expr(row, expr, db=None):
    """Evaluate arithmetic/string expression: +, -, *, / with correct precedence.
    Also handles regex.fn(...) calls when classic-regex is imported."""
    expr = expr.strip()

    # Inline IF: IF cond, true_val, ELSE false_val, END
    if re.match(r'^IF\b', expr, re.I) and re.search(r'\bEND\b', expr, re.I):
        parsed = _parse_inline_if(expr)
        if parsed:
            cond_s, true_s, false_s = parsed
            return (_eval_expr(row, true_s, db)
                    if eval_condition(row, cond_s, db)
                    else _eval_expr(row, false_s, db))

    # regex.fn(args...) call — matches outermost parens only
    rx = re.match(r'^regex\.(\w+)\((.+)\)$', expr, re.S)
    if rx:
        return _eval_regex_call(row, rx.group(1), rx.group(2))

    # Scalar function call: UPPER(x), LENGTH(x), TODAY(), etc.
    sf = re.match(r'^(\w+)\(([^)]*)\)$', expr)
    if sf and sf.group(1).lower() in _SCALAR_FN_NAMES:
        return _eval_scalar_fn(row, sf.group(1), sf.group(2))

    # outer.field — correlated subquery outer-row reference
    if expr.startswith('outer.'):
        outer_row = getattr(db, '_outer_row', None)
        return outer_row.get(expr[6:]) if outer_row else None

    tokens = _tokenize_expr(expr)
    if not tokens:
        return None

    vals, ops = [], []
    for kind, tok in tokens:
        if kind == 'OP':
            ops.append(tok)
        else:
            v, is_lit = _parse_literal(tok)
            vals.append(v if is_lit else row.get(tok))

    if not vals:
        return None
    if not ops:
        return vals[0]

    # Pass 1: * and /  (higher precedence)
    i = 0
    while i < len(ops):
        if ops[i] in ('*', '/'):
            a, b = vals[i], vals[i + 1]
            try:
                r = (a * b) if ops[i] == '*' else (a / b)
                if isinstance(r, float) and r == int(r) and isinstance(a, int) and isinstance(b, int):
                    r = int(r)
            except (TypeError, ZeroDivisionError):
                r = None
            vals = vals[:i] + [r] + vals[i + 2:]
            ops  = ops[:i]  + ops[i + 1:]
        else:
            i += 1

    # Pass 2: + and -  (lower precedence)
    result = vals[0]
    for i, op in enumerate(ops):
        b = vals[i + 1]
        if op == '+':
            if isinstance(result, (str, type(None))) or isinstance(b, (str, type(None))):
                result = _fmt_val(result) + _fmt_val(b)
            else:
                try:
                    result = result + b
                except TypeError:
                    result = _fmt_val(result) + _fmt_val(b)
        elif op == '-':
            try:
                result = result - b
            except TypeError:
                result = None

    return result


def _param_to_literal(val):
    """Convert a Python value to a safe JaSQL literal string (for parameterized queries)."""
    from datetime import date as _date, datetime as _dt, timedelta as _td
    if val is None:            return 'NIL'
    if isinstance(val, bool):  return 'true' if val else 'false'
    if isinstance(val, int):   return str(val)
    if isinstance(val, float): return repr(val)
    if isinstance(val, _dt):   return val.strftime('%d.%m.%y %H:%M')
    if isinstance(val, _date): return val.strftime('%d.%m.%y')
    if isinstance(val, _td):
        total = int(val.total_seconds())
        h, r  = divmod(total, 3600)
        m, s  = divmod(r, 60)
        return f'{h:02d}:{m:02d}:{s:02d}'
    escaped = str(val).replace('\\', '\\\\').replace('"', '\\"')
    return f'"{escaped}"'


def _bind_params(statement, params):
    """Substitute ? (positional) or :name (named) placeholders with sanitized literals."""
    if isinstance(params, dict):
        for name, val in params.items():
            statement = statement.replace(f':{name}', _param_to_literal(val))
        return statement
    # Positional list/tuple
    it = iter(params)
    out, i = [], 0
    while i < len(statement):
        if statement[i] == '?' and (i == 0 or statement[i - 1] not in ('"', "'")):
            try:
                out.append(_param_to_literal(next(it)))
            except StopIteration:
                out.append('?')
        else:
            out.append(statement[i])
        i += 1
    return ''.join(out)


# ─── Scalar functions ─────────────────────────────────────────────────────────

_SCALAR_FN_NAMES = {
    'upper', 'lower', 'trim', 'ltrim', 'rtrim', 'length', 'substr',
    'concat', 'coalesce', 'str', 'abs', 'round', 'floor', 'ceil',
    'year', 'month', 'day', 'today', 'now', 'date_diff', 'date_add',
    'int', 'float',
}


def _eval_scalar_fn(row, fn, args_str):
    """Evaluate a scalar function call: UPPER(x), LENGTH(x), YEAR(d), etc."""
    raw = [a.strip() for a in _split_csv(args_str)] if args_str.strip() else []
    args = []
    for a in raw:
        v, is_lit = _parse_literal(a)
        args.append(v if is_lit else row.get(a))

    def g(i=0):
        return args[i] if i < len(args) else None

    def s(i=0):
        v = g(i)
        return str(v) if v is not None else ''

    fn = fn.lower()
    try:
        if fn == 'upper':   return s().upper()
        if fn == 'lower':   return s().lower()
        if fn == 'trim':    return s().strip()
        if fn == 'ltrim':   return s().lstrip()
        if fn == 'rtrim':   return s().rstrip()
        if fn == 'length':  return len(s()) if g() is not None else None
        if fn == 'substr':
            src   = s()
            start = int(g(1)) - 1   # 1-indexed
            return src[start:start + int(g(2))] if len(args) >= 3 else src[start:]
        if fn == 'concat':
            return ''.join(str(a) for a in args if a is not None)
        if fn == 'coalesce':
            return next((a for a in args if a is not None), None)
        if fn == 'str':    return str(g())   if g() is not None else None
        if fn == 'abs':    return abs(g())   if g() is not None else None
        if fn == 'round':
            return round(g(), int(g(1))) if len(args) > 1 else round(g())
        if fn == 'floor':
            import math; return math.floor(g()) if g() is not None else None
        if fn == 'ceil':
            import math; return math.ceil(g())  if g() is not None else None
        if fn == 'int':    return int(g())   if g() is not None else None
        if fn == 'float':  return float(g()) if g() is not None else None
        if fn == 'year':   return g().year   if hasattr(g(), 'year')  else None
        if fn == 'month':  return g().month  if hasattr(g(), 'month') else None
        if fn == 'day':    return g().day    if hasattr(g(), 'day')   else None
        if fn == 'today':
            from datetime import date; return date.today()
        if fn == 'now':
            from datetime import datetime; return datetime.now()
        if fn == 'date_diff':
            a0, a1 = g(0), g(1)
            return (a0 - a1).days if a0 is not None and a1 is not None else None
        if fn == 'date_add':
            from datetime import timedelta
            a0, a1 = g(0), g(1)
            return (a0 + timedelta(days=int(a1))) if a0 is not None and a1 is not None else None
    except (TypeError, ValueError, AttributeError):
        return None
    return None


def _eval_regex_call(row, fn, args_str):
    """Evaluate regex.fn(arg, ...) — args may be field refs or literals."""
    args = []
    for a in _split_csv(args_str):
        a = a.strip()
        v, is_lit = _parse_literal(a)
        args.append(v if is_lit else row.get(a))

    # Coerce first arg (the subject string) to str; None → ''
    def s(i): return str(args[i]) if args[i] is not None else ''
    def p(i): return str(args[i]) if i < len(args) else ''

    fn = fn.lower()
    try:
        if fn == 'match':
            return bool(re.fullmatch(p(1), s(0)))
        if fn == 'test':
            return bool(re.search(p(1), s(0)))
        if fn == 'search':
            hit = re.search(p(1), s(0))
            return hit.group(0) if hit else ''
        if fn == 'replace':
            return re.sub(p(1), p(2), s(0), count=1)
        if fn == 'replace_all':
            return re.sub(p(1), p(2), s(0))
        if fn == 'count':
            return len(re.findall(p(1), s(0)))
        if fn == 'escape':
            return re.escape(s(0))
        if fn == 'find_all':
            return ', '.join(re.findall(p(1), s(0)))
        if fn == 'groups':
            hit = re.search(p(1), s(0))
            return ', '.join(hit.groups()) if hit else ''
        if fn == 'split':
            return ', '.join(re.split(p(1), s(0)))
    except re.error:
        return None
    return None


def _parse_field(f):
    """Parse 'expr AS alias' → (expr, alias).  Plain field → (field, field).
    Uses greedy match so the LAST 'AS word' at end-of-string is the alias."""
    m = re.match(r'^(.+)\s+AS\s+(\w+)\s*$', f.strip(), re.I)
    if m:
        return m.group(1).strip(), m.group(2)
    s = f.strip()
    return s, s


# ─── Formatting ───────────────────────────────────────────────────────────────

def _fmt_val(v):
    """Display a value in datac canonical format."""
    if v is None:                 return ''
    if isinstance(v, bool):       return 'true' if v else 'false'
    if isinstance(v, datetime):   return v.strftime('%d.%m.%y %H:%M')
    if isinstance(v, date):       return v.strftime('%d.%m.%y')
    if isinstance(v, timedelta):
        total = int(v.total_seconds())
        h, rem = divmod(total, 3600)
        m, s   = divmod(rem, 60)
        return f'{h:02d}:{m:02d}:{s:02d}'
    if isinstance(v, float):
        return f'{v:.10g}'
    return str(v)


def _display_val(v):
    """Like _fmt_val but shows NIL explicitly for null values."""
    if v is None:
        return 'NIL'
    return _fmt_val(v)


def format_table(rows, fields):
    if not rows:
        return "(no results)"
    if fields == ['*']:
        fields = [k for k in rows[0].keys() if '.' not in k] or list(rows[0].keys())

    col_w = {f: len(f) for f in fields}
    for row in rows:
        for f in fields:
            col_w[f] = max(col_w[f], len(_display_val(row.get(f))))

    header  = ' | '.join(f.ljust(col_w[f]) for f in fields)
    divider = '-+-'.join('-' * col_w[f] for f in fields)
    lines   = [header, divider] + [
        ' | '.join(_display_val(row.get(f)).ljust(col_w[f]) for f in fields)
        for row in rows
    ]
    return '\n'.join(lines)


# ─── TAKE clause tokenizer ────────────────────────────────────────────────────

_CLAUSE_RE = re.compile(
    r'\b(TAKE|FROM|IF|JOIN|GROUP\s+BY|ORDER\s+BY|SEPARATE\s+BY|FILTER|TOP|BOTTOM|MIDDLE)\b',
    re.I
)

_WIN_RE = re.compile(r'^(RANK|LAG)\(([^)]*)\)$', re.I)


def _protect_parens(text):
    """Replace (...) blocks with stable placeholders so tokenizer ignores their content."""
    placeholders, result, i = {}, [], 0
    while i < len(text):
        if text[i] == '(':
            depth, j = 0, i
            for j in range(i, len(text)):
                if text[j] == '(':   depth += 1
                elif text[j] == ')': depth -= 1
                if depth == 0:       break
            key = f'__P{len(placeholders)}__'
            placeholders[key] = text[i:j + 1]
            result.append(key)
            i = j + 1
        else:
            result.append(text[i])
            i += 1
    return ''.join(result), placeholders


def _restore(s, placeholders):
    for key, val in placeholders.items():
        s = s.replace(key, val)
    return s


def _tokenize_take(text):
    clauses = {
        'fields': '', 'tables': '', 'pre_if': None, 'joins': [],
        'group': None, 'post_if': None, 'filter_fields': None,
        'order_field': None, 'order_dir': None,
        'limit_type': None, 'limit_args': None,
        'separate': None,
    }

    # Protect subquery (TAKE ...) blocks from being split by the keyword scanner
    protected, placeholders = _protect_parens(text)

    matches = list(_CLAUSE_RE.finditer(protected))
    tokens  = []
    for i, m in enumerate(matches):
        kw  = re.sub(r'\s+', ' ', m.group()).upper()
        end = matches[i + 1].start() if i + 1 < len(matches) else len(protected)
        raw_val = protected[m.end():end].strip()
        tokens.append((kw, _restore(raw_val, placeholders)))

    group_seen = False
    for kw, val in tokens:
        if kw == 'TAKE':
            clauses['fields'] = val
        elif kw == 'FROM':
            clauses['tables'] = val
        elif kw == 'IF':
            if group_seen:
                clauses['post_if'] = val          # HAVING
            else:
                clauses['pre_if']  = val          # WHERE
        elif kw == 'JOIN':
            m = re.match(r'^(\w+)\s+ON\s+(.+)$', val, re.I)
            if m:
                clauses['joins'].append((m.group(1), m.group(2).strip()))
        elif kw == 'GROUP BY':
            clauses['group'] = val.strip()
            group_seen = True
        elif kw == 'SEPARATE BY':
            clauses['separate'] = val.strip()
        elif kw == 'ORDER BY':
            parts = val.split()
            clauses['order_field'] = parts[0] if parts else None
            clauses['order_dir']   = parts[1].upper() if len(parts) > 1 else 'ASC'
        elif kw == 'FILTER':
            clauses['filter_fields'] = [f.strip() for f in val.split(',')] if val.strip() else []
        elif kw == 'TOP':
            clauses['limit_type'], clauses['limit_args'] = 'TOP',    int(val)
        elif kw == 'BOTTOM':
            clauses['limit_type'], clauses['limit_args'] = 'BOTTOM', int(val)
        elif kw == 'MIDDLE':
            parts = val.split()
            clauses['limit_type'], clauses['limit_args'] = 'MIDDLE', (int(parts[0]), int(parts[1]))

    return clauses


# ─── View / table resolver ───────────────────────────────────────────────────

def _resolve_table(db, tname):
    """Return a Table for tname, expanding views on the fly. Raises JaSQLError."""
    if tname in db.tables:
        return db.tables[tname]
    if tname in getattr(db, 'views', {}):
        sub_rows, _ = _execute_take(db, db.views[tname], raw=True)
        from abudb import Table as _Table
        t = _Table(tname)
        t.rows = sub_rows
        return t
    raise JaSQLError("TableNotFound", token=tname)


# ─── Window functions ─────────────────────────────────────────────────────────

def _apply_window(rows, parsed_take, separate_field, order_field, order_dir):
    """Partition rows by separate_field, compute RANK() and LAG() within each partition."""
    partitions = {}
    for row in rows:
        key = row.get(separate_field)
        partitions.setdefault(key, []).append(row)

    result = []
    for key in sorted(partitions, key=lambda x: (x is None, x or '')):
        part = partitions[key]
        if order_field:
            part = apply_order(part, order_field, order_dir or 'ASC')
        for i, row in enumerate(part):
            for expr, alias in parsed_take:
                wm = _WIN_RE.match(expr)
                if not wm:
                    continue
                fn  = wm.group(1).upper()
                arg = wm.group(2).strip()
                if fn == 'RANK':
                    row[alias] = i + 1
                elif fn == 'LAG':
                    parts_split = [a.strip() for a in arg.split(',', 1)]
                    field_name  = parts_split[0]
                    offset      = int(parts_split[1]) if len(parts_split) > 1 else 1
                    row[alias]  = part[i - offset].get(field_name) if i >= offset else None
        result.extend(part)
    return result


# ─── Hash join ───────────────────────────────────────────────────────────────

def _hash_join(rows, t2, join_cond, join_tname, base_tname):
    """
    Hash join for simple equality conditions: [t1.]f1 = [t2.]f2
    Builds a hash table on t2 (O(n)), probes with rows (O(m)).
    Returns new_rows list, or None if the condition is not a plain equality.
    """
    m = re.match(
        r'^(?:(\w+)\.)?(\w+)\s*=\s*(?:(\w+)\.)?(\w+)$',
        join_cond.strip()
    )
    if not m:
        return None

    qt1, f1, qt2, f2 = m.group(1), m.group(2), m.group(3), m.group(4)

    # Decide which side is the probe key (from rows/base) and build key (from t2)
    if qt2 == join_tname or (qt1 == base_tname and qt2 is None):
        probe_key, build_key = f1, f2
    elif qt1 == join_tname or (qt2 == base_tname and qt1 is None):
        probe_key, build_key = f2, f1
    else:
        probe_key, build_key = f1, f2   # unqualified: left = base, right = join

    # Build hash on the join table side
    build = {}
    for r2 in t2.iter_rows():
        k = r2.get(build_key)
        build.setdefault(k, []).append(r2)

    # Probe
    new_rows = []
    for r1 in rows:
        k = r1.get(probe_key)
        if k is None and base_tname:
            k = r1.get(f'{base_tname}.{probe_key}')
        for r2 in build.get(k, []):
            merged = dict(r1)
            if base_tname:
                for kk, v in r1.items():
                    if '.' not in kk:
                        merged[f'{base_tname}.{kk}'] = v
            for kk, v in r2.items():
                merged[f'{join_tname}.{kk}'] = v
                if kk not in merged:
                    merged[kk] = v
            new_rows.append(merged)
    return new_rows


# ─── Core TAKE execution ──────────────────────────────────────────────────────

def _run_take_clauses(db, clauses):
    fields_str  = clauses['fields']
    tables_str  = clauses['tables']
    take_fields = [f.strip() for f in _split_csv(fields_str)]

    # Parse AS aliases and expressions:  [(expr, alias), ...]
    parsed_take = [('*', '*') if f == '*' else _parse_field(f) for f in take_fields]

    if not tables_str:
        return [], [alias for _, alias in parsed_take]

    # ── Derived table: FROM (TAKE ... FROM ...) ──────────────────────────────
    ts = tables_str.strip()
    if ts.startswith('('):
        parens = _extract_parens(ts)
        if parens:
            inner, _ = parens
            if re.match(r'^TAKE\b', inner.strip(), re.I):
                sub_rows, _ = _execute_take(db, inner.strip(), raw=True)
                rows     = [r.copy() for r in sub_rows]
                skip_pre = False
                table_names = ['__derived__']
                # Jump straight to JOIN/WHERE processing
                goto_derived = True
            else:
                raise JaSQLError("SyntaxError", token="FROM (...)")
        else:
            raise JaSQLError("SyntaxError", token="FROM (")
    else:
        goto_derived = False

    if not goto_derived:
        # Parse KEEP ALL modifier: "students KEEP ALL, classes"
        keep_all_table = None
        raw_names      = [t.strip() for t in ts.split(',')]
        table_names    = []
        for t in raw_names:
            m = re.match(r'^(\w+)\s+KEEP\s+ALL$', t, re.I)
            if m:
                keep_all_table = m.group(1)
                table_names.append(keep_all_table)
            else:
                table_names.append(t)

        # ── Resolve base rows ────────────────────────────────────────────────

        # KEEP ALL on one of two tables → left join
        if keep_all_table and len(table_names) == 2:
            right_name = next(t for t in table_names if t != keep_all_table)
            try:
                rows = db.left_join(keep_all_table, right_name)
            except KeyError as e:
                raise JaSQLError("TableNotFound", token=str(e))
            skip_pre = False

        elif len(table_names) == 1:
            tname = table_names[0]
            tbl   = _resolve_table(db, tname)   # expands views

            pre_if   = clauses['pre_if']
            skip_pre = False

            # Index optimisation (eager tables only)
            if pre_if and not clauses['joins'] and not tbl.is_lazy:
                m = re.match(r'^(\w+)\s*=\s*(.+)$', pre_if.strip())
                if m:
                    idx_field = m.group(1)
                    idx_val, idx_is_lit = _parse_literal(m.group(2).strip())
                    if idx_is_lit:
                        indexed = db.get_indexed_rows(tname, idx_field, idx_val)
                        if indexed is not None:
                            rows     = [r.copy() for r in indexed]
                            skip_pre = True

            if not skip_pre:
                if pre_if and not clauses['joins']:
                    pred     = lambda r, _c=pre_if: eval_condition(r, _c, db)
                    rows     = [r.copy() for r in tbl.iter_rows(pred)]
                    skip_pre = True
                else:
                    rows = [r.copy() for r in tbl.iter_rows()]

        else:
            tables   = []
            for tname in table_names:
                tables.append(_resolve_table(db, tname))

            skip_pre = False
            if len(set(id(t) for t in tables)) == 1:
                rows = [r.copy() for r in tables[0].iter_rows()]
            else:
                rows = []
                for combo in cart_product(*[t.rows for t in tables]):
                    merged = {}
                    for tname, row in zip(table_names, combo):
                        for k, v in row.items():
                            merged[f'{tname}.{k}'] = v
                            merged[k] = v
                    rows.append(merged)

    # ── JOINs — hash join for equality, nested loop fallback ─────────────────
    base_tname = table_names[0] if table_names else None
    for join_tname, join_cond in (clauses['joins'] or []):
        if join_tname not in db.tables:
            raise JaSQLError("TableNotFound", token=join_tname)
        t2       = db.tables[join_tname]
        new_rows = _hash_join(rows, t2, join_cond, join_tname, base_tname)
        if new_rows is None:
            # Non-equality condition: fall back to nested loop
            new_rows = []
            for r1 in rows:
                for r2 in t2.iter_rows():
                    merged = dict(r1)
                    if base_tname:
                        for k, v in r1.items():
                            if '.' not in k:
                                merged[f'{base_tname}.{k}'] = v
                    for k, v in r2.items():
                        merged[f'{join_tname}.{k}'] = v
                        if k not in merged:
                            merged[k] = v
                    if eval_condition(merged, join_cond, db):
                        new_rows.append(merged)
        rows = new_rows

    # ── Pre-filter (WHERE) ───────────────────────────────────────────────────
    if clauses['pre_if'] and not skip_pre:
        rows = [r for r in rows if eval_condition(r, clauses['pre_if'], db)]

    # ── Aggregates + GROUP ───────────────────────────────────────────────────
    has_agg = any(_AGG_RE.match(expr) for expr, alias in parsed_take if expr != '*')

    if has_agg and clauses['group']:
        group_field = clauses['group']
        groups = {}
        for row in rows:
            groups.setdefault(row.get(group_field), []).append(row)

        result_rows = []
        for key in sorted(groups, key=lambda x: (x is None, x or '')):
            grp = groups[key]
            out = {group_field: key}
            for expr, alias in parsed_take:
                if expr in (group_field, '*') or alias == group_field:
                    continue
                m2 = _AGG_RE.match(expr)
                out[alias] = apply_aggregate(grp, m2.group(1), m2.group(2)) if m2 else grp[0].get(expr)
            result_rows.append(out)

        rows           = result_rows
        display_fields = [group_field] + [alias for expr, alias in parsed_take
                                          if expr != group_field and alias != group_field and expr != '*']

    elif has_agg:
        result = {}
        for expr, alias in parsed_take:
            m2 = _AGG_RE.match(expr)
            if m2:
                result[alias] = apply_aggregate(rows, m2.group(1), m2.group(2))
        return [result], list(result.keys())

    else:
        display_fields = [alias for _, alias in parsed_take]
        # Evaluate expressions and store under alias
        for row in rows:
            for expr, alias in parsed_take:
                if expr != '*':
                    row[alias] = _eval_expr(row, expr, db)

    # ── Post-filter (HAVING) ─────────────────────────────────────────────────
    if clauses['post_if']:
        rows = [r for r in rows if eval_condition(r, clauses['post_if'], db)]

    # ── ORDER BY ─────────────────────────────────────────────────────────────
    if clauses['order_field'] and not clauses['separate']:
        rows = apply_order(rows, clauses['order_field'], clauses['order_dir'] or 'ASC')

    # ── Window functions (RANK / LAG) — requires SEPARATE BY ─────────────────
    if clauses['separate']:
        has_win = any(_WIN_RE.match(expr) for expr, alias in parsed_take if expr != '*')
        if has_win:
            rows = _apply_window(rows, parsed_take, clauses['separate'],
                                 clauses['order_field'], clauses['order_dir'])
        elif clauses['order_field']:
            rows = apply_order(rows, clauses['order_field'], clauses['order_dir'] or 'ASC')

    # ── FILTER (DISTINCT) ────────────────────────────────────────────────────
    if clauses['filter_fields'] is not None:
        key_fields = clauses['filter_fields'] if clauses['filter_fields'] else display_fields
        key_fields = [f for f in key_fields if f != '*']
        seen, unique = set(), []
        for row in rows:
            key = tuple(row.get(f) for f in key_fields)
            if key not in seen:
                seen.add(key)
                unique.append(row)
        rows = unique

    # ── Limit ────────────────────────────────────────────────────────────────
    lt, la = clauses['limit_type'], clauses['limit_args']
    if   lt == 'TOP':    rows = rows[:la]
    elif lt == 'BOTTOM': rows = rows[-la:]
    elif lt == 'MIDDLE': rows = rows[la[0]-1:la[1]]

    # ── Resolve * ────────────────────────────────────────────────────────────
    if display_fields == ['*']:
        display_fields = ([k for k in rows[0].keys() if '.' not in k]
                          or list(rows[0].keys())) if rows else []

    return rows, display_fields


def _execute_take(db, statement, raw=False):
    stmt_norm = re.sub(r'\s+', ' ', statement).strip()

    # SIZE(tablename) with no FROM
    m = re.match(r'^TAKE\s+SIZE\((\w+)\)\s*$', stmt_norm, re.I)
    if m:
        tname = m.group(1)
        if tname not in db.tables:
            return ([], []) if raw else f"Table '{tname}' not found"
        size = len(db.tables[tname].rows)
        return ([{'SIZE': size}], ['SIZE']) if raw else f"SIZE: {size}"

    try:
        clauses = _tokenize_take(stmt_norm)
        rows, fields = _run_take_clauses(db, clauses)
    except (ValueError, JaSQLError):
        if raw:
            return [], []
        raise

    if raw:
        return rows, fields

    if not fields:
        return "(no results)"

    # Scalar aggregate output (no GROUP)
    if all(_AGG_RE.match(f) for f in fields) and len(rows) == 1:
        return '\n'.join(f"{k}: {v}" for k, v in rows[0].items())

    count = len(rows)
    return f"{format_table(rows, fields)}\n({count} row{'s' if count != 1 else ''})"


# ─── Mutation commands ────────────────────────────────────────────────────────

def _execute_update(db, raw, stmt=None):
    # Split on the LAST occurrence of ' IF ' to correctly locate the condition
    upper = raw.upper()
    if_pos = upper.rfind(' IF ')
    if if_pos < 0:
        raise JaSQLError("SyntaxError", token="UPDATE", statement=stmt)
    cond     = raw[if_pos + 4:].strip()
    preamble = raw[:if_pos].strip()    # "UPDATE table SET col=v, col=v"

    m = re.match(r'^UPDATE\s+(\w+)\s+SET\s+(.+)$', preamble, re.I | re.S)
    if not m:
        raise JaSQLError("SyntaxError", token="UPDATE", statement=stmt)
    tname, set_clause = m.group(1), m.group(2).strip()
    if tname not in db.tables:
        raise JaSQLError("TableNotFound", token=tname, statement=stmt)
    table = db.tables[tname]

    # Parse one or more assignments: field op= expr, ...
    assignments = []
    for part in _split_csv(set_clause):
        m2 = re.match(r'^(\w+)\s*(\+|-|\*|/)?=\s*(.+)$', part.strip())
        if not m2:
            raise JaSQLError("SyntaxError", token=part.strip()[:30], statement=stmt)
        field, op, val_str = m2.group(1), m2.group(2) or '', m2.group(3).strip()
        literal_val, _ = _parse_literal(val_str)
        assignments.append((field, op, val_str, literal_val))

    count = 0
    for row in table.rows:
        if eval_condition(row, cond, db):
            for field, op, val_str, literal_val in assignments:
                if   op == '':  new_val = _eval_expr(row, val_str, db)
                elif op == '+': new_val = row.get(field, 0) + literal_val
                elif op == '-': new_val = row.get(field, 0) - literal_val
                elif op == '*': new_val = row.get(field, 0) * literal_val
                elif op == '/': new_val = row.get(field, 0) / literal_val
                else:           new_val = None
                try:
                    row[field] = table.coerce_field(field, new_val)
                except ValueError as e:
                    raise JaSQLError("TypeError", token=str(e)[:60], statement=stmt)
            count += 1
    db.invalidate_indexes(tname)
    return f"Updated {count} row{'s' if count != 1 else ''}"


def _parse_row_dict(body, stmt=None):
    """Parse 'field: val, field: val' into a dict."""
    row = {}
    for part in _split_csv(body):
        m2 = re.match(r'(\w+)\s*:\s*(.+)', part.strip())
        if m2:
            row[m2.group(1)] = _parse_literal(m2.group(2).strip())[0]
    return row


def _execute_insert(db, raw, stmt=None):
    m = re.match(r'^INSERT\s+(\w+)\s+(.+)$', raw, re.I | re.S)
    if not m:
        raise JaSQLError("SyntaxError", token="INSERT", statement=stmt)
    tname, bodies_str = m.group(1), m.group(2).strip()
    if tname not in db.tables:
        raise JaSQLError("TableNotFound", token=tname, statement=stmt)
    table = db.tables[tname]

    # Collect all {...} row blocks
    row_bodies = re.findall(r'\{([^}]+)\}', bodies_str, re.S)
    if not row_bodies:
        raise JaSQLError("SyntaxError", token="INSERT body", statement=stmt)

    inserted = 0
    for body in row_bodies:
        row = _parse_row_dict(body.strip(), stmt)
        if not row:
            raise JaSQLError("SyntaxError", token="INSERT body", statement=stmt)
        if table.schema:
            missing = [f for f in table.schema
                       if f not in row and f not in table.nullable]
            if missing:
                raise JaSQLError("MissingFields", token=', '.join(missing), statement=stmt)
            try:
                row = table.validate_and_coerce(row)
            except ValueError as e:
                raise JaSQLError("TypeError", token=str(e)[:60], statement=stmt)
        # FK check
        try:
            db.check_fk_insert(tname, row)
        except ValueError as e:
            raise JaSQLError("FKViolation", token=str(e)[:80], statement=stmt)
        table.rows.append(row)
        inserted += 1

    db.invalidate_indexes(tname)
    n = inserted
    return f"Inserted {n} row{'s' if n != 1 else ''} into '{tname}'"


def _execute_rm(db, raw, stmt=None):
    m = re.match(r'^RM\s+FROM\s+(\w+)\s+IF\s+(.+)$', raw, re.I)
    if not m:
        raise JaSQLError("SyntaxError", token="RM", statement=stmt)
    tname, cond = m.group(1), m.group(2).strip()
    if tname not in db.tables:
        raise JaSQLError("TableNotFound", token=tname, statement=stmt)
    table       = db.tables[tname]
    before      = len(table.rows)
    to_delete   = [r for r in table.rows if eval_condition(r, cond, db)]
    try:
        db.check_fk_delete(tname, to_delete)
    except ValueError as e:
        raise JaSQLError("FKViolation", token=str(e)[:80], statement=stmt)
    table.rows  = [r for r in table.rows if not eval_condition(r, cond, db)]
    db.invalidate_indexes(tname)
    removed = before - len(table.rows)
    return f"Removed {removed} row{'s' if removed != 1 else ''}"


def _execute_crop(db, raw, stmt=None):
    m = re.match(r'^CROP\s+(\d+)\s+(ASC|DESC|ABC|ZYX)\s+FROM\s+(\w+)\s+BY\s+(\w+)$', raw, re.I)
    if not m:
        raise JaSQLError("SyntaxError", token="CROP", statement=stmt)
    n, direction, tname, field = int(m.group(1)), m.group(2).upper(), m.group(3), m.group(4)
    if tname not in db.tables:
        raise JaSQLError("TableNotFound", token=tname, statement=stmt)
    table      = db.tables[tname]
    table.rows = apply_order(table.rows, field, direction)[:n]
    db.invalidate_indexes(tname)
    return f"Cropped '{tname}' to {len(table.rows)} row{'s' if len(table.rows) != 1 else ''}"


def _datac_val(v):
    """Serialize a Python value back to datac literal syntax."""
    if v is None:               return 'NIL'
    if isinstance(v, bool):     return 'true' if v else 'false'
    if isinstance(v, datetime): return v.strftime('%d.%m.%y %H:%M')
    if isinstance(v, date):     return v.strftime('%d.%m.%y')
    if isinstance(v, timedelta):
        total = int(v.total_seconds())
        h, rem = divmod(total, 3600)
        m2, s  = divmod(rem, 60)
        return f'{h:02d}:{m2:02d}:{s:02d}'
    if isinstance(v, str):      return f'"{v}"'
    return str(v)


def _write_table_block(lines, table, tname):
    """Append schema preamble + data block for one table to lines list."""
    schema   = table.schema
    pk       = table.primary_key
    nullable = getattr(table, 'nullable', set())
    sub_ref  = pk or 'id'

    if pk and pk in schema:
        lines.append(f'class {schema[pk]} property: {pk}')
    for field, type_str in schema.items():
        if field == pk:
            continue
        lines.append(f'{type_str} sub.{sub_ref}: {field}')
        if field in nullable:
            lines.append(f'set inclusion {sub_ref}.{field} to nil')

    lines.append('')
    lines.append(f'{tname} {{')

    schema_fields = list(schema.keys())
    for row in table.rows:
        parts = []
        for field in schema_fields:
            if field in row:
                parts.append(f'{field}: {_datac_val(row[field])}')
            elif field not in nullable:
                parts.append(f'{field}: nil')   # required field missing → explicit nil
        for field, val in row.items():
            if field not in schema and '.' not in field:
                parts.append(f'{field}: {_datac_val(val)}')
        lines.append(f'    {", ".join(parts)}')

    lines.append('}')
    lines.append('')


def _execute_save(db, raw, stmt=None):
    m = re.match(r'^SAVE\s+(\w+)\s+TO\s+(\S+)$', raw, re.I)
    if not m:
        raise JaSQLError("SyntaxError", token="SAVE", statement=stmt)
    tname, path = m.group(1), m.group(2)
    if tname not in db.tables:
        raise JaSQLError("TableNotFound", token=tname, statement=stmt)
    lines = []
    _write_table_block(lines, db.tables[tname], tname)
    try:
        with open(path, 'w') as f:
            f.write('\n'.join(lines) + '\n')
    except OSError as e:
        raise JaSQLError("IOError", token=str(e)[:60], statement=stmt)
    n = len(db.tables[tname].rows)
    return f"Saved '{tname}' ({n} row{'s' if n != 1 else ''}) → '{path}'"


def _execute_mtsave(db, raw, stmt=None):
    m = re.match(r'^MTSAVE\s+(.+?)\s+TO\s+(\S+)$', raw, re.I)
    if not m:
        raise JaSQLError("SyntaxError", token="MTSAVE", statement=stmt)
    names = [n.strip() for n in m.group(1).split(',')]
    path  = m.group(2)
    missing = [n for n in names if n not in db.tables]
    if missing:
        raise JaSQLError("TableNotFound", token=', '.join(missing), statement=stmt)
    lines = []
    total = 0
    for tname in names:
        _write_table_block(lines, db.tables[tname], tname)
        total += len(db.tables[tname].rows)
    try:
        with open(path, 'w') as f:
            f.write('\n'.join(lines) + '\n')
    except OSError as e:
        raise JaSQLError("IOError", token=str(e)[:60], statement=stmt)
    summary = ', '.join(f"'{n}'" for n in names)
    return f"Saved {summary} ({total} total rows) → '{path}'"


def _execute_allsave(db, raw, stmt=None):
    m = re.match(r'^ALLSAVE\s+TO\s+(\S+)$', raw, re.I)
    if not m:
        raise JaSQLError("SyntaxError", token="ALLSAVE", statement=stmt)
    path = m.group(1)
    names = [n for n in db.tables if re.match(r'^\w+$', n)]
    seen, unique = set(), []
    for n in names:
        t = db.tables[n]
        if id(t) not in seen:
            seen.add(id(t))
            unique.append(n)
    if not unique:
        raise JaSQLError("NoTables", token="ALLSAVE", statement=stmt)
    lines = []
    total = 0
    for tname in unique:
        _write_table_block(lines, db.tables[tname], tname)
        total += len(db.tables[tname].rows)
    try:
        with open(path, 'w') as f:
            f.write('\n'.join(lines) + '\n')
    except OSError as e:
        raise JaSQLError("IOError", token=str(e)[:60], statement=stmt)
    summary = ', '.join(f"'{n}'" for n in unique)
    return f"Saved {summary} ({total} total rows) → '{path}'"


# ─── Statement splitting ──────────────────────────────────────────────────────

_TOP_LEVEL = {'TAKE', 'IMPORT', 'CONTENT', 'CONNECT', 'UPDATE',
              'INSERT', 'RM', 'CROP', 'SAVE', 'MTSAVE', 'ALLSAVE',
              'INDEX', 'DROP', 'CREATE', 'ALTER', 'BEGIN', 'COMMIT', 'ROLLBACK',
              'DESTROY', 'CHECKPOINT', 'BREAK', 'CHECK'}

_BLOCK_KW  = {'IF', 'LOOP', 'ELSE', 'END', 'BREAK'}


def _strip_comment(line):
    in_quote, qc = False, None
    for i, ch in enumerate(line):
        if ch in ('"', "'") and not in_quote:
            in_quote, qc = True, ch
        elif ch == qc and in_quote:
            in_quote = False
        elif not in_quote:
            if ch in ('#', ';'):
                return line[:i].strip()
            if ch == '-' and i + 1 < len(line) and line[i + 1] == '-':
                return line[:i].strip()
    return line


def split_statements(script, with_lines=False):
    """
    Split a JaSQL script into individual statements.
    with_lines=True → returns list of (statement_str, start_line_number) tuples.
    """
    statements, current, union_pending = [], [], False
    line_no = 0
    start_line = 1

    for raw_line in script.split('\n'):
        line_no += 1
        stripped = _strip_comment(raw_line.strip())
        if not stripped:
            continue
        upper = stripped.upper()
        first = upper.split()[0] if upper.split() else ''

        if upper == 'UNION':
            current.append(stripped)
            union_pending = True
        elif first in _TOP_LEVEL:
            if union_pending:
                current.append(stripped)
                union_pending = False
            elif current:
                statements.append(('\n'.join(current), start_line))
                current    = [stripped]
                start_line = line_no
            else:
                current    = [stripped]
                start_line = line_no
        elif stripped and current:
            current.append(stripped)

    if current:
        statements.append(('\n'.join(current), start_line))

    if with_lines:
        return statements
    return [s for s, _ in statements]


# ─── Script block control flow ───────────────────────────────────────────────

class _BreakSignal(Exception):
    pass


def _extract_scalar_from_result(result):
    """Pull a single numeric/bool value from a TAKE result string."""
    s = str(result).strip()
    # 'KEY: value' scalar-aggregate format — KEY may contain parens e.g. SUM(val)
    m = re.match(r'^([^:]+):\s*(.+)$', s)
    if m:
        v, is_lit = _parse_literal(m.group(2).strip())
        if is_lit:
            return v
    # Table with ≥1 data row — grab first cell of first data row
    lines = [l for l in s.split('\n') if l.strip() and not set(l.strip()).issubset(set('-+| '))]
    if len(lines) >= 2:
        cell = lines[1].split('|')[0].strip()
        v, is_lit = _parse_literal(cell)
        if is_lit:
            return v
    return None


def _eval_cond(db, cond_str):
    """Evaluate a script-level IF / LOOP UNTIL condition. Returns bool."""
    cond = cond_str.strip()

    # TAKE expr op value  (e.g. TAKE SIZE(*) FROM t > 0)
    m = re.match(r'^(.+?)\s*(!=|>=|<=|>|<|=)\s*(\S+)\s*$', cond)
    if m:
        take_part, op, rhs_raw = m.group(1).strip(), m.group(2), m.group(3)
        if re.match(r'^TAKE\b', take_part, re.I):
            val = _extract_scalar_from_result(execute(db, take_part))
            rhs, is_lit = _parse_literal(rhs_raw)
            if val is not None and is_lit:
                try:
                    if op == '=':  return val == rhs
                    if op == '!=': return val != rhs
                    if op == '>':  return val >  rhs
                    if op == '<':  return val <  rhs
                    if op == '>=': return val >= rhs
                    if op == '<=': return val <= rhs
                except TypeError:
                    pass
        # Had an operator — don't fall through to bare-TAKE path
        return False

    # Bare TAKE — truthy if any rows returned
    if re.match(r'^TAKE\b', cond, re.I):
        return '(no results)' not in str(execute(db, cond))

    return False


def _merge_script_lines(raw):
    """
    Merge comment-stripped lines into (stmt, line_no) pairs.
    Block keywords (IF/LOOP/ELSE/END/BREAK) always start a new statement.
    """
    stmts, current, start_line = [], [], None
    for text, ln in raw:
        first = text.split()[0].upper() if text.split() else ''
        is_new = first in _TOP_LEVEL or first in _BLOCK_KW
        if is_new:
            if current:
                stmts.append((' '.join(current), start_line))
            current, start_line = [text], ln
        elif current:
            current.append(text)
        else:
            current, start_line = [text], ln
    if current:
        stmts.append((' '.join(current), start_line))
    return stmts


def _parse_block(stmts, i):
    """
    Recursively parse stmts[i:] into a node tree.
    Stops at ELSE / ELSE IF / END FROM or end-of-input.
    Returns (nodes, new_i).
    Node shapes:
      ('stmt', text, line_no)
      ('if',   [(cond_or_None, nodes), ...], line_no)
      ('loop', 'until'|'forever', cond_or_None, nodes, line_no)
      ('break', line_no)
    """
    nodes = []
    while i < len(stmts):
        text, ln = stmts[i]
        upper = text.strip().upper()
        first = upper.split()[0] if upper.split() else ''

        if first == 'ELSE' or upper == 'END FROM':
            break

        if first == 'IF':
            cond = text.strip()[3:].strip()
            i   += 1
            body, i = _parse_block(stmts, i)
            branches = [(cond, body)]
            while i < len(stmts):
                t2 = stmts[i][0].strip()
                u2 = t2.upper()
                if u2.startswith('ELSE IF '):
                    cond2 = t2[8:].strip()
                    i    += 1
                    b2, i = _parse_block(stmts, i)
                    branches.append((cond2, b2))
                elif u2 == 'ELSE':
                    i    += 1
                    b3, i = _parse_block(stmts, i)
                    branches.append((None, b3))
                    break
                else:
                    break
            if i < len(stmts) and stmts[i][0].strip().upper() == 'END FROM':
                i += 1
            nodes.append(('if', branches, ln))

        elif upper.startswith('LOOP UNTIL '):
            cond = text.strip()[11:].strip()
            i   += 1
            body, i = _parse_block(stmts, i)
            if i < len(stmts) and stmts[i][0].strip().upper() == 'END FROM':
                i += 1
            nodes.append(('loop', 'until', cond, body, ln))

        elif upper == 'LOOP FOREVER':
            i   += 1
            body, i = _parse_block(stmts, i)
            if i < len(stmts) and stmts[i][0].strip().upper() == 'END FROM':
                i += 1
            nodes.append(('loop', 'forever', None, body, ln))

        elif upper == 'BREAK':
            nodes.append(('break', ln))
            i += 1

        else:
            nodes.append(('stmt', text, ln))
            i += 1

    return nodes, i


_MAX_LOOP_ITERATIONS = 100_000


def _exec_nodes(db, nodes, results, depth=0):
    """Execute a parsed node tree, collecting result strings."""
    for node in nodes:
        kind = node[0]

        if kind == 'stmt':
            _, text, ln = node
            results.append(execute(db, text, line=ln))

        elif kind == 'if':
            _, branches, ln = node
            for cond, body in branches:
                if cond is None or _eval_cond(db, cond):
                    _exec_nodes(db, body, results, depth + 1)
                    break

        elif kind == 'loop':
            _, loop_type, cond, body, ln = node
            iters = 0
            while True:
                if loop_type == 'until' and _eval_cond(db, cond):
                    break
                iters += 1
                if iters > _MAX_LOOP_ITERATIONS:
                    results.append(
                        f"Preposterous: InfiniteLoop found ln:{ln}\n"
                        f"  loop exceeded {_MAX_LOOP_ITERATIONS} iterations"
                    )
                    return
                try:
                    _exec_nodes(db, body, results, depth + 1)
                except _BreakSignal:
                    break

        elif kind == 'break':
            raise _BreakSignal()


# ─── Main dispatcher ──────────────────────────────────────────────────────────

def execute(db, statement, line=1, params=None):
    if params is not None:
        statement = _bind_params(statement, params)
    stmt = re.sub(r'\s+', ' ', statement).strip()

    try:
        # UNION
        if re.search(r'^\s*UNION\s*$', statement, re.M | re.I):
            parts = re.split(r'\s*\bUNION\b\s*', statement, flags=re.I)
            combined, display_fields = [], None
            for part in parts:
                part = part.strip()
                if not part:
                    continue
                rows, fields = _execute_take(db, part, raw=True)
                if display_fields is None:
                    display_fields = fields
                    combined.extend(rows)
                else:
                    for row in rows:
                        remapped = {display_fields[i]: row.get(f)
                                    for i, f in enumerate(fields) if i < len(display_fields)}
                        combined.append(remapped)
            count = len(combined)
            return f"{format_table(combined, display_fields or ['*'])}\n({count} row{'s' if count != 1 else ''})"

        # IMPORT classic-regex  (module — not a file)
        if re.match(r'^IMPORT\s+classic-regex$', stmt, re.I):
            db._jasql_modules = getattr(db, '_jasql_modules', set())
            db._jasql_modules.add('regex')
            return (
                "Loaded: classic-regex\n"
                "\n"
                "  Conditions (in IF / JOIN ON):\n"
                "    field MATCH \"pattern\"        -- full match (anchored)\n"
                "    field NOT MATCH \"pattern\"\n"
                "    field SEARCH \"pattern\"       -- partial match (unanchored)\n"
                "    field NOT SEARCH \"pattern\"\n"
                "\n"
                "  Expressions (in TAKE / UPDATE SET):\n"
                "    regex.match(field, \"pat\")         -- bool: full match\n"
                "    regex.test(field, \"pat\")          -- bool: partial match\n"
                "    regex.search(field, \"pat\")        -- first match or \"\"\n"
                "    regex.replace(field, \"pat\", \"r\") -- replace first\n"
                "    regex.replace_all(f, \"pat\", \"r\") -- replace all\n"
                "    regex.count(field, \"pat\")         -- number of matches\n"
                "    regex.escape(field)               -- escape special chars\n"
                "    regex.find_all(field, \"pat\")      -- comma-joined matches\n"
                "    regex.groups(field, \"pat\")        -- comma-joined capture groups\n"
                "    regex.split(field, \"pat\")         -- comma-joined split result"
            )

        # IMPORT file (.datac / .csv / .xlsx) [TO alias]
        m = re.match(r'^IMPORT\s+(\S+)(?:\s+TO\s+(\w+))?$', stmt, re.I)
        if m:
            filepath, alias = m.group(1), m.group(2)
            ext = filepath.rsplit('.', 1)[-1].lower() if '.' in filepath else ''
            if ext == 'csv':
                t = db.import_csv(filepath, alias)
                return f"Imported '{filepath}' as '{t.name}' (lazy, {len(t.schema)} field{'s' if len(t.schema) != 1 else ''})"
            elif ext == 'xlsx':
                t = db.import_xlsx(filepath, alias)
                return f"Imported '{filepath}' as '{t.name}' (lazy, {len(t.schema)} field{'s' if len(t.schema) != 1 else ''})"
            else:
                imported = db.import_datac(filepath, alias)
                if len(imported) == 1:
                    t = imported[0]
                    return f"Imported '{filepath}' as '{t.name}' (lazy, {len(t.schema)} field{'s' if len(t.schema) != 1 else ''})"
                names = [t.name for t in imported]
                return f"Imported '{filepath}' → {names} ({len(names)} table{'s' if len(names) != 1 else ''}, lazy)"

        # CREATE TABLE name { field: type, field?: type, ... }
        m = re.match(r'^CREATE\s+TABLE\s+(\w+)\s*\{(.+)\}$', stmt, re.I | re.S)
        if m:
            tname, body = m.group(1), m.group(2).strip()
            schema, nullable, pk = {}, set(), None
            for part in _split_csv(body):
                part = part.strip()
                if not part:
                    continue
                pm = re.match(r'^(\w+)(\?)?\s*:\s*(\w+)$', part)
                if not pm:
                    raise JaSQLError("SyntaxError", token=part[:30], statement=stmt, line=line)
                fname, is_null, ftype = pm.group(1), bool(pm.group(2)), pm.group(3)
                schema[fname] = ftype
                if is_null:
                    nullable.add(fname)
                if pk is None:
                    pk = fname
            try:
                db.create_table(tname, schema, pk, nullable)
            except KeyError:
                raise JaSQLError("TableExists", token=tname, statement=stmt, line=line)
            fields_desc = ', '.join(
                f"{f}{'?' if f in nullable else ''}: {t}" for f, t in schema.items()
            )
            return f"Created table '{tname}' ({fields_desc})"

        # CONTENT FROM
        m = re.match(r'^CONTENT\s+FROM\s+(\S+)\s+TO\s+(\w+)$', stmt, re.I)
        if m:
            filepath, alias = m.group(1), m.group(2)
            db.content_from(filepath, alias)
            return f"Loaded '{filepath}' as '{alias}' ({len(db.tables[alias].rows)} rows)"

        # CONNECT
        m = re.match(r'^CONNECT\s+(\w+)\.(\w+)\s+TO\s+(\w+)\.(\w+)$', stmt, re.I)
        if m:
            t1, f1, t2, f2 = m.group(1), m.group(2), m.group(3), m.group(4)
            mt = db.connect(t1, f1, t2, f2)
            return f"Connected '{t1}' and '{t2}' → merged table with {len(mt.rows)} rows"

        # INDEX
        m = re.match(r'^INDEX\s+(\w+)\s+ON\s+(\w+)$', stmt, re.I)
        if m:
            tname, field = m.group(1), m.group(2)
            try:
                n_keys = db.build_index(tname, field)
                return f"Index built on '{tname}.{field}' ({n_keys} unique value{'s' if n_keys != 1 else ''})"
            except KeyError:
                raise JaSQLError("TableNotFound", token=tname, statement=stmt, line=line)

        # DROP INDEX
        m = re.match(r'^DROP\s+INDEX\s+(\w+)\s+ON\s+(\w+)$', stmt, re.I)
        if m:
            tname, field = m.group(1), m.group(2)
            if db.drop_index(tname, field):
                return f"Index dropped on '{tname}.{field}'"
            return f"No index on '{tname}.{field}'"

        # DROP TABLE
        m = re.match(r'^DROP\s+TABLE\s+(\w+)$', stmt, re.I)
        if m:
            tname = m.group(1)
            try:
                db.drop_table(tname)
            except KeyError:
                raise JaSQLError("TableNotFound", token=tname, statement=stmt, line=line)
            return f"Dropped table '{tname}'"

        # DESTROY table (clear all rows, keep schema)
        m = re.match(r'^DESTROY\s+(\w+)$', stmt, re.I)
        if m:
            tname = m.group(1)
            if tname not in db.tables:
                raise JaSQLError("TableNotFound", token=tname, statement=stmt, line=line)
            db.tables[tname].rows = []
            db.invalidate_indexes(tname)
            return f"Destroyed '{tname}' (schema kept)"

        # CREATE VIEW name AS TAKE ...
        m = re.match(r'^CREATE\s+VIEW\s+(\w+)\s+AS\s+(.+)$', stmt, re.I)
        if m:
            vname, query = m.group(1), m.group(2).strip()
            try:
                db.create_view(vname, query)
            except KeyError as e:
                raise JaSQLError("ViewError", token=vname, statement=stmt, line=line)
            return f"Created view '{vname}'"

        # DROP VIEW name
        m = re.match(r'^DROP\s+VIEW\s+(\w+)$', stmt, re.I)
        if m:
            vname = m.group(1)
            try:
                db.drop_view(vname)
            except KeyError:
                raise JaSQLError("ViewNotFound", token=vname, statement=stmt, line=line)
            return f"Dropped view '{vname}'"

        # CHECKPOINT name
        m = re.match(r'^CHECKPOINT\s+(\w+)$', stmt, re.I)
        if m:
            name = m.group(1)
            try:
                db.checkpoint(name)
            except RuntimeError as e:
                raise JaSQLError("TransactionError", token=name, statement=stmt, line=line)
            return f"Checkpoint '{name}' saved"

        # ROLLBACK TO name
        m = re.match(r'^ROLLBACK\s+TO\s+(\w+)$', stmt, re.I)
        if m:
            name = m.group(1)
            try:
                db.rollback_to(name)
            except KeyError:
                raise JaSQLError("CheckpointNotFound", token=name, statement=stmt, line=line)
            return f"Rolled back to checkpoint '{name}'"

        # BEGIN / COMMIT / ROLLBACK
        if stmt.upper() == 'BEGIN':
            try:
                db.begin()
            except RuntimeError as e:
                raise JaSQLError("TransactionError", token="BEGIN", statement=stmt, line=line)
            return "Transaction started"

        if stmt.upper() == 'COMMIT':
            try:
                db.commit()
            except RuntimeError:
                raise JaSQLError("TransactionError", token="COMMIT", statement=stmt, line=line)
            return "Transaction committed"

        if stmt.upper() == 'ROLLBACK':
            db.rollback()
            return "Transaction rolled back"

        # ALTER TABLE
        m = re.match(r'^ALTER\s+TABLE\s+(\w+)\s+(.+)$', stmt, re.I)
        if m:
            tname, clause = m.group(1), m.group(2).strip()
            if tname not in db.tables and not re.match(r'^RENAME\s+TO\b', clause, re.I):
                raise JaSQLError("TableNotFound", token=tname, statement=stmt, line=line)
            tbl = db.tables.get(tname)

            # ADD field: type[?] [DEFAULT val]
            am = re.match(r'^ADD\s+(\w+)(\?)?\s*:\s*(\w+)(?:\s+DEFAULT\s+(.+))?$', clause, re.I)
            if am:
                fname, is_null, ftype, default_str = \
                    am.group(1), bool(am.group(2)), am.group(3), am.group(4)
                default = _parse_literal(default_str.strip())[0] if default_str else None
                try:
                    tbl.alter_add_field(fname, ftype, nullable=is_null, default=default)
                except KeyError as e:
                    raise JaSQLError("FieldExists", token=fname, statement=stmt, line=line)
                return f"Added '{fname}: {ftype}{'?' if is_null else ''}' to '{tname}'"

            # DROP field
            dm = re.match(r'^DROP\s+(\w+)$', clause, re.I)
            if dm:
                fname = dm.group(1)
                try:
                    tbl.alter_drop_field(fname)
                except (KeyError, ValueError) as e:
                    raise JaSQLError("AlterError", token=str(e)[:50], statement=stmt, line=line)
                return f"Dropped '{fname}' from '{tname}'"

            # RENAME TO new_table_name
            rtm = re.match(r'^RENAME\s+TO\s+(\w+)$', clause, re.I)
            if rtm:
                new_name = rtm.group(1)
                try:
                    db.alter_rename_table(tname, new_name)
                except KeyError as e:
                    raise JaSQLError("AlterError", token=str(e)[:50], statement=stmt, line=line)
                return f"Renamed table '{tname}' → '{new_name}'"

            # RENAME old_field TO new_field
            rfm = re.match(r'^RENAME\s+(\w+)\s+TO\s+(\w+)$', clause, re.I)
            if rfm:
                old_f, new_f = rfm.group(1), rfm.group(2)
                try:
                    tbl.alter_rename_field(old_f, new_f)
                except KeyError as e:
                    raise JaSQLError("AlterError", token=str(e)[:50], statement=stmt, line=line)
                return f"Renamed '{old_f}' → '{new_f}' in '{tname}'"

            raise JaSQLError("SyntaxError", token=clause[:30], statement=stmt, line=line)

        if re.match(r'^TAKE\b',    stmt, re.I): return _execute_take(db, statement)
        if re.match(r'^UPDATE\b',  stmt, re.I): return _execute_update(db, stmt, stmt)
        if re.match(r'^INSERT\b',  stmt, re.I): return _execute_insert(db, stmt, stmt)
        if re.match(r'^RM\b',      stmt, re.I): return _execute_rm(db, stmt, stmt)
        if re.match(r'^CROP\b',    stmt, re.I): return _execute_crop(db, stmt, stmt)
        if re.match(r'^SAVE\b',    stmt, re.I): return _execute_save(db, stmt, stmt)
        if re.match(r'^MTSAVE\b',  stmt, re.I): return _execute_mtsave(db, stmt, stmt)
        if re.match(r'^ALLSAVE\b', stmt, re.I): return _execute_allsave(db, stmt, stmt)

        return f"Unknown command: {stmt[:60]}"

    except JaSQLError as e:
        if e.line is None:
            e.line = line
        if e.statement is None:
            e.statement = stmt
        return str(e)


def execute_script(db, script):
    """
    Execute a JaSQL script with full block control flow:
    IF / ELSE IF / ELSE / END FROM, LOOP UNTIL cond / LOOP FOREVER / BREAK.
    """
    raw = []
    for ln, line in enumerate(script.split('\n'), 1):
        text = _strip_comment(line.strip())
        if text:
            raw.append((text, ln))

    stmts      = _merge_script_lines(raw)
    tree, _    = _parse_block(stmts, 0)
    results    = []
    try:
        _exec_nodes(db, tree, results)
    except _BreakSignal:
        results.append("Preposterous: BreakOutsideLoop found ln:?")
    return results
