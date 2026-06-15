#!/usr/bin/env python3
import sys
import os
from abudb import Database
from jasql import execute, split_statements

BANNER = """
  AbuDB + JaSQL  —  JaSQL's an SQL
  Type queries and press Enter twice to run.
  Run a script: python3 repl.py script.jasql
  Commands: .tables  .schema <table>  .indexes  .exit
"""

PROMPT      = "abudb> "
CONT_PROMPT = "    .. "


def run_repl(db):
    print(BANNER)
    buffer = []

    while True:
        try:
            prompt = PROMPT if not buffer else CONT_PROMPT
            line = input(prompt)
        except (EOFError, KeyboardInterrupt):
            print()
            break

        # Built-in dot-commands
        stripped = line.strip()
        if not buffer and stripped.startswith('.'):
            _dot_command(db, stripped)
            continue

        if stripped == '' and buffer:
            # Execute accumulated statement
            statement = '\n'.join(buffer)
            buffer = []
            for stmt in split_statements(statement):
                try:
                    result = execute(db, stmt)
                    print(result)
                except Exception as e:
                    print(f"Error: {e}")
            print()
        elif stripped:
            buffer.append(stripped)


def _dot_command(db, cmd):
    parts = cmd.split()
    if parts[0] == '.exit':
        print("Bye.")
        sys.exit(0)
    elif parts[0] == '.tables':
        if db.tables:
            for name, t in db.tables.items():
                print(f"  {name}  ({len(t.rows)} rows)")
        else:
            print("  (no tables loaded)")
    elif parts[0] == '.schema' and len(parts) > 1:
        tname = parts[1]
        if tname in db.tables:
            t = db.tables[tname]
            for field, typ in t.schema.items():
                pk = " [primary]" if field == t.primary_key else ""
                print(f"  {field}: {typ}{pk}")
        else:
            print(f"  Table '{tname}' not found")
    elif parts[0] == '.indexes':
        if db.indexes:
            for (tname, field), idx in db.indexes.items():
                n_keys = len(idx)
                n_rows = sum(len(v) for v in idx.values())
                print(f"  {tname}.{field}  ({n_keys} unique values, {n_rows} rows indexed)")
        else:
            print("  (no indexes)")
    else:
        print(f"  Unknown command: {cmd}")
    print()


def run_script(db, filepath):
    with open(filepath) as f:
        script = f.read()
    for stmt in split_statements(script):
        stmt_display = stmt.replace('\n', ' ')[:60]
        print(f">> {stmt_display}")
        try:
            result = execute(db, stmt)
            print(result)
        except Exception as e:
            print(f"Error: {e}")
        print()


if __name__ == '__main__':
    db = Database()
    if len(sys.argv) > 1:
        script_path = sys.argv[1]
        if not os.path.exists(script_path):
            print(f"File not found: {script_path}")
            sys.exit(1)
        run_script(db, script_path)
    else:
        run_repl(db)
