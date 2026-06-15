"""
JaSQL test suite — pytest
Run:  cd abudb-sql && python -m pytest test_jasql.py -v
"""
import os
import tempfile
import pytest
from abudb import Database
import jasql


# ─── Fixtures ─────────────────────────────────────────────────────────────────

@pytest.fixture
def db():
    return Database()


@pytest.fixture
def users(db):
    jasql.execute(db, 'CREATE TABLE users { id: int, name: string, age?: int }')
    jasql.execute(db, 'INSERT users { id: 1, name: "Alice", age: 30 }')
    jasql.execute(db, 'INSERT users { id: 2, name: "Bob",   age: 25 }')
    jasql.execute(db, 'INSERT users { id: 3, name: "Carol" }')
    return db


@pytest.fixture
def csv_file():
    f = tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False)
    f.write('id,name,score\n1,Alice,95\n2,Bob,87\n3,Carol,72\n')
    f.close()
    yield f.name
    os.unlink(f.name)


# ─── TAKE ─────────────────────────────────────────────────────────────────────

def test_take_all(users):
    r = jasql.execute(users, 'TAKE * FROM users')
    assert 'Alice' in r and 'Bob' in r and 'Carol' in r


def test_take_filter(users):
    r = jasql.execute(users, 'TAKE name FROM users IF age > 26')
    assert 'Alice' in r
    assert 'Bob' not in r


def test_take_order_asc(users):
    r = jasql.execute(users, 'TAKE name FROM users ORDER BY age ASC')
    lines = r.strip().split('\n')
    bob_i   = next(i for i, l in enumerate(lines) if 'Bob'   in l)
    alice_i = next(i for i, l in enumerate(lines) if 'Alice' in l)
    assert bob_i < alice_i


def test_take_top(users):
    r = jasql.execute(users, 'TAKE name FROM users ORDER BY id ASC TOP 2')
    assert 'Alice' in r and 'Bob' in r and 'Carol' not in r


def test_aggregate_size(users):
    r = jasql.execute(users, 'TAKE SIZE(*) FROM users')
    assert '3' in r


def test_aggregate_mean(users):
    r = jasql.execute(users, 'TAKE MEAN(age) FROM users')
    assert '27.5' in r   # (30 + 25) / 2; Carol has no age → excluded


def test_aggregate_sum(users):
    r = jasql.execute(users, 'TAKE SUM(age) FROM users')
    assert '55' in r


def test_aggregate_group_by(users):
    jasql.execute(users, 'INSERT users { id: 4, name: "Dave", age: 25 }')
    r = jasql.execute(users, 'TAKE age, SIZE(*) FROM users GROUP BY age ORDER BY age ASC')
    assert '25' in r and '30' in r


def test_union(users):
    r = jasql.execute(users, 'TAKE name FROM users IF id = 1\nUNION\nTAKE name FROM users IF id = 2')
    assert 'Alice' in r and 'Bob' in r


def test_like(users):
    r = jasql.execute(users, 'TAKE name FROM users IF name LIKE "A%"')
    assert 'Alice' in r and 'Bob' not in r


def test_in_list(users):
    r = jasql.execute(users, 'TAKE name FROM users IF id IN (1, 3)')
    assert 'Alice' in r and 'Carol' in r and 'Bob' not in r


def test_nil_filter(users):
    r = jasql.execute(users, 'TAKE name FROM users IF age = NIL')
    assert 'Carol' in r and 'Alice' not in r


# ─── JOIN ─────────────────────────────────────────────────────────────────────

def test_hash_join(db):
    jasql.execute(db, 'CREATE TABLE a { id: int, val: string }')
    jasql.execute(db, 'CREATE TABLE b { aid: int, score: int }')
    jasql.execute(db, 'INSERT a { id: 1, val: "alpha" }')
    jasql.execute(db, 'INSERT a { id: 2, val: "beta" }')
    jasql.execute(db, 'INSERT b { aid: 1, score: 90 }')
    jasql.execute(db, 'INSERT b { aid: 2, score: 80 }')
    r = jasql.execute(db, 'TAKE val, score FROM a JOIN b ON a.id = b.aid')
    assert 'alpha' in r and '90' in r
    assert 'beta'  in r and '80' in r


# ─── CREATE / DROP TABLE ──────────────────────────────────────────────────────

def test_create_table(db):
    r = jasql.execute(db, 'CREATE TABLE t { id: int, label?: string }')
    assert 'Created' in r
    assert 't' in db.tables
    assert db.tables['t'].primary_key == 'id'
    assert 'label' in db.tables['t'].nullable


def test_create_duplicate(db):
    jasql.execute(db, 'CREATE TABLE t { id: int }')
    r = jasql.execute(db, 'CREATE TABLE t { id: int }')
    assert 'Preposterous' in r and 'TableExists' in r


def test_drop_table(db):
    jasql.execute(db, 'CREATE TABLE t { id: int }')
    r = jasql.execute(db, 'DROP TABLE t')
    assert 'Dropped' in r
    assert 't' not in db.tables


def test_drop_nonexistent(db):
    r = jasql.execute(db, 'DROP TABLE ghost')
    assert 'Preposterous' in r and 'TableNotFound' in r


# ─── SCHEMA VALIDATION ────────────────────────────────────────────────────────

def test_insert_missing_required(db):
    jasql.execute(db, 'CREATE TABLE t { id: int, name: string }')
    r = jasql.execute(db, 'INSERT t { id: 1 }')
    assert 'Preposterous' in r and 'MissingFields' in r


def test_insert_type_error(db):
    jasql.execute(db, 'CREATE TABLE t { id: int }')
    r = jasql.execute(db, 'INSERT t { id: "hello" }')
    assert 'Preposterous' in r and 'TypeError' in r


def test_insert_int_coercion(db):
    jasql.execute(db, 'CREATE TABLE t { id: int }')
    r = jasql.execute(db, 'INSERT t { id: 7 }')
    assert 'Inserted' in r
    assert db.tables['t'].rows[0]['id'] == 7
    assert isinstance(db.tables['t'].rows[0]['id'], int)


def test_insert_float_coercion(db):
    jasql.execute(db, 'CREATE TABLE t { id: int, score: float }')
    r = jasql.execute(db, 'INSERT t { id: 1, score: 9 }')
    assert 'Inserted' in r
    assert isinstance(db.tables['t'].rows[0]['score'], float)


def test_insert_nullable_omit(db):
    jasql.execute(db, 'CREATE TABLE t { id: int, note?: string }')
    jasql.execute(db, 'INSERT t { id: 1 }')
    assert db.tables['t'].rows[0].get('note') is None


def test_update_type_error(users):
    r = jasql.execute(users, 'UPDATE users SET age = "bad" IF id = 1')
    assert 'Preposterous' in r and 'TypeError' in r
    # original value must be unchanged
    row = next(r for r in users.tables['users'].rows if r['id'] == 1)
    assert row['age'] == 30


def test_update_set_plain(users):
    jasql.execute(users, 'UPDATE users SET age = 31 IF id = 1')
    row = next(r for r in users.tables['users'].rows if r['id'] == 1)
    assert row['age'] == 31


def test_update_compound(users):
    jasql.execute(users, 'UPDATE users SET age += 5 IF id = 2')
    row = next(r for r in users.tables['users'].rows if r['id'] == 2)
    assert row['age'] == 30


# ─── RM / CROP ────────────────────────────────────────────────────────────────

def test_rm(users):
    jasql.execute(users, 'RM FROM users IF id = 3')
    assert len(users.tables['users'].rows) == 2
    assert all(r['id'] != 3 for r in users.tables['users'].rows)


def test_crop(users):
    r = jasql.execute(users, 'CROP 2 ASC FROM users BY age')
    assert len(users.tables['users'].rows) == 2


# ─── TRANSACTIONS ─────────────────────────────────────────────────────────────

def test_commit(db):
    jasql.execute(db, 'CREATE TABLE t { id: int }')
    jasql.execute(db, 'BEGIN')
    jasql.execute(db, 'INSERT t { id: 1 }')
    jasql.execute(db, 'COMMIT')
    assert len(db.tables['t'].rows) == 1
    assert not db._in_transaction


def test_rollback_insert(db):
    jasql.execute(db, 'CREATE TABLE t { id: int }')
    jasql.execute(db, 'BEGIN')
    jasql.execute(db, 'INSERT t { id: 1 }')
    jasql.execute(db, 'ROLLBACK')
    assert len(db.tables['t'].rows) == 0
    assert not db._in_transaction


def test_rollback_update(users):
    jasql.execute(users, 'BEGIN')
    jasql.execute(users, 'UPDATE users SET age = 99 IF id = 1')
    jasql.execute(users, 'ROLLBACK')
    row = next(r for r in users.tables['users'].rows if r['id'] == 1)
    assert row['age'] == 30


def test_rollback_drop_table(db):
    jasql.execute(db, 'CREATE TABLE t { id: int }')
    jasql.execute(db, 'BEGIN')
    jasql.execute(db, 'DROP TABLE t')
    assert 't' not in db.tables
    jasql.execute(db, 'ROLLBACK')
    assert 't' in db.tables


def test_rollback_create_table(db):
    jasql.execute(db, 'BEGIN')
    jasql.execute(db, 'CREATE TABLE t { id: int }')
    assert 't' in db.tables
    jasql.execute(db, 'ROLLBACK')
    assert 't' not in db.tables


def test_nested_begin_error(db):
    jasql.execute(db, 'BEGIN')
    r = jasql.execute(db, 'BEGIN')
    assert 'Preposterous' in r and 'TransactionError' in r
    jasql.execute(db, 'ROLLBACK')


def test_commit_no_transaction(db):
    r = jasql.execute(db, 'COMMIT')
    assert 'Preposterous' in r and 'TransactionError' in r


def test_rollback_no_transaction(db):
    r = jasql.execute(db, 'ROLLBACK')
    assert 'rolled back' in r   # graceful no-op


# ─── ALTER TABLE ──────────────────────────────────────────────────────────────

def test_alter_add_field(users):
    r = jasql.execute(users, 'ALTER TABLE users ADD score: int DEFAULT 0')
    assert 'Added' in r
    assert 'score' in users.tables['users'].schema
    for row in users.tables['users'].rows:
        assert row.get('score') == 0


def test_alter_add_nullable(users):
    jasql.execute(users, 'ALTER TABLE users ADD note?: string')
    assert 'note' in users.tables['users'].nullable


def test_alter_drop_field(users):
    jasql.execute(users, 'ALTER TABLE users DROP age')
    assert 'age' not in users.tables['users'].schema
    for row in users.tables['users'].rows:
        assert 'age' not in row


def test_alter_drop_pk_error(users):
    r = jasql.execute(users, 'ALTER TABLE users DROP id')
    assert 'Preposterous' in r and 'AlterError' in r


def test_alter_rename_field(users):
    jasql.execute(users, 'ALTER TABLE users RENAME name TO full_name')
    assert 'full_name' in users.tables['users'].schema
    assert 'name'      not in users.tables['users'].schema
    for row in users.tables['users'].rows:
        assert 'full_name' in row and 'name' not in row


def test_alter_rename_table(users):
    jasql.execute(users, 'ALTER TABLE users RENAME TO people')
    assert 'people' in users.tables
    assert 'users'  not in users.tables
    r = jasql.execute(users, 'TAKE name FROM people')
    assert 'Alice' in r


def test_alter_nonexistent_table(db):
    r = jasql.execute(db, 'ALTER TABLE ghost ADD x: int')
    assert 'Preposterous' in r and 'TableNotFound' in r


# ─── SAVE / LOAD ROUNDTRIP ────────────────────────────────────────────────────

def test_save_roundtrip(users):
    with tempfile.NamedTemporaryFile(suffix='.datac', delete=False) as f:
        path = f.name
    try:
        jasql.execute(users, f'SAVE users TO {path}')
        db2 = Database()
        jasql.execute(db2, f'IMPORT {path} TO users')
        r = jasql.execute(db2, 'TAKE name FROM users')
        assert 'Alice' in r and 'Bob' in r
    finally:
        os.unlink(path)


def test_save_nonexistent_table(db):
    r = jasql.execute(db, 'SAVE ghost TO /tmp/test_ghost.datac')
    assert 'Preposterous' in r and 'TableNotFound' in r


def test_allsave_empty(db):
    r = jasql.execute(db, 'ALLSAVE TO /tmp/test_empty.datac')
    assert 'Preposterous' in r and 'NoTables' in r


# ─── IMPORT CSV ───────────────────────────────────────────────────────────────

def test_import_csv_lazy(db, csv_file):
    r = jasql.execute(db, f'IMPORT {csv_file} TO data')
    assert 'lazy' in r
    assert db.tables['data'].is_lazy   # not yet loaded into memory


def test_import_csv_filter(db, csv_file):
    jasql.execute(db, f'IMPORT {csv_file} TO data')
    r = jasql.execute(db, 'TAKE name FROM data IF score > 90')
    assert 'Alice' in r
    assert 'Bob'   not in r


def test_import_csv_order(db, csv_file):
    jasql.execute(db, f'IMPORT {csv_file} TO data')
    r = jasql.execute(db, 'TAKE name FROM data ORDER BY score DESC TOP 1')
    assert 'Alice' in r


# ─── ERROR FORMAT ─────────────────────────────────────────────────────────────

def test_error_preposterous_prefix(db):
    r = jasql.execute(db, 'TAKE * FROM ghost')
    assert r.startswith('Preposterous:')


def test_error_line_number(db):
    r = jasql.execute(db, 'TAKE * FROM ghost', line=7)
    assert 'ln:7' in r


def test_error_caret(db):
    r = jasql.execute(db, 'TAKE * FROM ghost')
    assert '^' in r
    lines = r.split('\n')
    stmt_line  = next(l for l in lines if 'ghost' in l)
    caret_line = next(l for l in lines if '^'     in l)
    col = stmt_line.index('ghost') - 2   # strip 2-space indent
    assert caret_line[2 + col] == '^'


# ─── SCRIPT LINE TRACKING ─────────────────────────────────────────────────────

def test_script_line_tracking(db):
    script = (
        'CREATE TABLE t { id: int }\n'
        'INSERT t { id: 1 }\n'
        'TAKE * FROM ghost\n'
    )
    results = jasql.execute_script(db, script)
    error = next(r for r in results if 'Preposterous' in r)
    assert 'ln:3' in error


def test_script_all_succeed(db):
    script = (
        'CREATE TABLE t { id: int }\n'
        'INSERT t { id: 1 }\n'
        'INSERT t { id: 2 }\n'
    )
    results = jasql.execute_script(db, script)
    assert all('Preposterous' not in r for r in results)
    assert len(db.tables['t'].rows) == 2


# ─── REGEX DSL ────────────────────────────────────────────────────────────────

def test_regex_search_condition(db):
    jasql.execute(db, 'CREATE TABLE t { id: int, email: string }')
    jasql.execute(db, 'INSERT t { id: 1, email: "alice@corp.com" }')
    jasql.execute(db, 'INSERT t { id: 2, email: "bob@gmail.com"  }')
    jasql.execute(db, 'IMPORT classic-regex')
    r = jasql.execute(db, 'TAKE email FROM t IF email SEARCH "corp"')
    assert 'alice' in r and 'bob' not in r


def test_regex_match_condition(db):
    jasql.execute(db, 'CREATE TABLE t { id: int, code: string }')
    jasql.execute(db, 'INSERT t { id: 1, code: "ABC-123" }')
    jasql.execute(db, 'INSERT t { id: 2, code: "XYZ"     }')
    jasql.execute(db, 'IMPORT classic-regex')
    r = jasql.execute(db, r'TAKE code FROM t IF code MATCH "[A-Z]+-\d+"')
    assert 'ABC-123' in r and 'XYZ' not in r


def test_regex_replace_expression(db):
    jasql.execute(db, 'CREATE TABLE t { id: int, email: string }')
    jasql.execute(db, 'INSERT t { id: 1, email: "alice@corp.com" }')
    jasql.execute(db, 'IMPORT classic-regex')
    r = jasql.execute(db, 'TAKE regex.replace(email, "@.+", "@***") AS masked FROM t')
    assert 'alice@***' in r


def test_regex_update_set(db):
    jasql.execute(db, 'CREATE TABLE t { id: int, email: string }')
    jasql.execute(db, 'INSERT t { id: 1, email: "alice@corp.com" }')
    jasql.execute(db, 'IMPORT classic-regex')
    jasql.execute(db, 'UPDATE t SET email = regex.replace(email, "@.+", "@***") IF id = 1')
    assert db.tables['t'].rows[0]['email'] == 'alice@***'


# ─── INDEX ────────────────────────────────────────────────────────────────────

def test_index_build_and_use(users):
    jasql.execute(users, 'INDEX users ON id')
    r = jasql.execute(users, 'TAKE name FROM users IF id = 2')
    assert 'Bob' in r


def test_drop_index(users):
    jasql.execute(users, 'INDEX users ON id')
    r = jasql.execute(users, 'DROP INDEX users ON id')
    assert 'dropped' in r
