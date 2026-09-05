import re
from datetime import date, datetime, timedelta


def _strip_comment(line):
    """; style comments, respecting quoted strings."""
    in_quote, qc = False, None
    for i, ch in enumerate(line):
        if ch in ('"', "'") and not in_quote:
            in_quote, qc = True, ch
        elif ch == qc and in_quote:
            in_quote = False
        elif ch == ';' and not in_quote:
            return line[:i]
    return line


def parse_value(s):
    s = s.strip().rstrip(',')
    if s.lower() == 'nil': return None
    if s.lower() == 'true':  return True
    if s.lower() == 'false': return False
    if (s.startswith('"') and s.endswith('"')) or \
       (s.startswith("'") and s.endswith("'")):
        return s[1:-1]
    m = re.match(r'^(\d{2})\.(\d{2})\.(\d{2})\s+(\d{2}):(\d{2})(?::(\d{2}))?$', s)
    if m:
        d,mo,y,h,mi = int(m.group(1)),int(m.group(2)),int(m.group(3)),int(m.group(4)),int(m.group(5))
        return datetime(2000+y, mo, d, h, mi, int(m.group(6)) if m.group(6) else 0)
    m = re.match(r'^(\d{2})\.(\d{2})\.(\d{2})$', s)
    if m:
        return date(2000+int(m.group(3)), int(m.group(2)), int(m.group(1)))
    m = re.match(r'^(\d{1,3}):(\d{2}):(\d{2})$', s)
    if m:
        return timedelta(hours=int(m.group(1)), minutes=int(m.group(2)), seconds=int(m.group(3)))
    try:    return int(s)
    except ValueError: pass
    try:    return float(s)
    except ValueError: pass
    return s


def _parse_row_line(line):
    """Parse one data line into a dict; nil fields are omitted (absent = null)."""
    row   = {}
    parts = re.split(r',\s*(?=\w+\s*:)', line.strip())
    for part in parts:
        m = re.match(r'(\w+)\s*:\s*(.+)', part.strip())
        if m:
            raw_val = m.group(2).strip()
            if raw_val.lower() == 'nil':
                continue          # absent field → datac_null returns 1
            row[m.group(1)] = parse_value(raw_val)
    return row


# ── Full parse (eager) ────────────────────────────────────────────────────────

def parse_datac(filepath):
    """
    Parse a .datac file.
    Returns list of (schema, primary_key, table_name, rows, nullable_fields).
    """
    with open(filepath) as f:
        content = f.read()

    tables           = []
    current_schema   = {}
    current_pk       = None
    current_nullable = set()
    table_name       = None
    in_block         = False
    block_lines      = []

    for raw in content.split('\n'):
        line = _strip_comment(raw).strip()
        if not line:
            continue

        if not in_block:
            m = re.match(r'^class\s+(\w+)\s+property:\s+(\w+)$', line)
            if m:
                current_schema[m.group(2)] = m.group(1)
                current_pk = m.group(2)
                continue

            m = re.match(r'^(\w+)\s+sub\.\w+:\s+(\w+)$', line)
            if m:
                current_schema[m.group(2)] = m.group(1)
                continue

            m = re.match(r'^set\s+inclusion\s+\w+\.(\w+)\s+to\s+nil$', line, re.I)
            if m:
                current_nullable.add(m.group(1))
                continue

            m = re.match(r'^(\w+)\s*\{$', line)
            if m:
                table_name  = m.group(1)
                in_block    = True
                block_lines = []
                continue

        else:
            if line == '}':
                in_block = False
                rows = [r for r in (_parse_row_line(l) for l in block_lines if l.strip()) if r]
                tables.append((dict(current_schema), current_pk, table_name,
                               rows, set(current_nullable)))
                current_schema = {}; current_pk = None
                current_nullable = set(); table_name = None
            else:
                block_lines.append(line)

    return tables


# ── Schema-only parse (fast, no rows loaded) ──────────────────────────────────

def parse_datac_schemas(filepath):
    """
    Read only schema headers — skip all data rows.
    Returns list of (schema, primary_key, table_name, nullable_fields).
    """
    tables           = []
    current_schema   = {}
    current_pk       = None
    current_nullable = set()

    with open(filepath) as f:
        lines = iter(f)
        for raw in lines:
            line = _strip_comment(raw).strip()
            if not line:
                continue

            m = re.match(r'^class\s+(\w+)\s+property:\s+(\w+)$', line)
            if m:
                current_schema[m.group(2)] = m.group(1)
                current_pk = m.group(2)
                continue

            m = re.match(r'^(\w+)\s+sub\.\w+:\s+(\w+)$', line)
            if m:
                current_schema[m.group(2)] = m.group(1)
                continue

            m = re.match(r'^set\s+inclusion\s+\w+\.(\w+)\s+to\s+nil$', line, re.I)
            if m:
                current_nullable.add(m.group(1))
                continue

            m = re.match(r'^(\w+)\s*\{$', line)
            if m:
                tname = m.group(1)
                tables.append((dict(current_schema), current_pk,
                               tname, set(current_nullable)))
                current_schema = {}; current_pk = None; current_nullable = set()
                for raw2 in lines:          # consume + discard the data block
                    if _strip_comment(raw2).strip() == '}':
                        break

    return tables


# ── Row streaming (lazy, one table at a time) ─────────────────────────────────

def stream_datac_rows(filepath, table_name):
    """
    Generator: yield one row dict at a time for the named table.
    Skips all other table blocks without loading them.
    """
    with open(filepath) as f:
        lines     = iter(f)
        in_target = False

        for raw in lines:
            line = _strip_comment(raw).strip()
            if not line:
                continue

            if not in_target:
                m = re.match(r'^(\w+)\s*\{$', line)
                if m:
                    if m.group(1) == table_name:
                        in_target = True
                    else:
                        for raw2 in lines:      # skip non-target block
                            if _strip_comment(raw2).strip() == '}':
                                break
            else:
                if line == '}':
                    return
                row = _parse_row_line(line)
                if row:
                    yield row
