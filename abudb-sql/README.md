# JaSQL

**JaSQL's an SQL** — a query language for AbuDB, the Abu Ecosystem's in-process database.

JaSQL is a DSL that sits on top of AbuDB (Python). You write SQL-style statements; the runtime handles parsing, type validation, lazy loading, hash joins, transactions, and structured errors. JaSQL is deliberately *not* named after the A-languages (AC, AI, AC+) because its syntax is normal — English keywords, standard operators, quoted strings.

**SCT score: 9.2** *(Shariff Complexity Theorem, 2026 — easier to learn than Python at 10.0)*

---

## Quick Start

```python
from abudb import Database
import jasql

db = Database()
results = jasql.execute_script(db, """
    CREATE TABLE users { id: int, name: string, age?: int }
    INSERT users { id: 1, name: "Alice", age: 30 }
    INSERT users { id: 2, name: "Bob",   age: 25 }
    TAKE name FROM users IF age > 26
""")
for r in results:
    print(r)
```

Single statement:
```python
print(jasql.execute(db, 'TAKE * FROM users ORDER BY age DESC'))
```

---

## Data Types

| Type       | Example value         | Notes                        |
|------------|----------------------|------------------------------|
| `int`      | `42`                  | Integer                      |
| `float`    | `3.14`                | Floating-point               |
| `string`   | `"hello"`             | Quoted string                |
| `bool`     | `true` / `false`      |                              |
| `date`     | `25.05.26`            | dd.mm.yy                     |
| `datetime` | `25.05.26 14:30`      | dd.mm.yy hh:mm               |
| `deltat`   | `01:30:00`            | hh:mm:ss duration            |
| nil        | `NIL`                 | Null / absent value          |

---

## DDL — Tables

### CREATE TABLE
```sql
CREATE TABLE name { field: type, field?: type, ... }
```
- First field is automatically the primary key
- `?` after the field name marks it as nullable (optional on insert)

```sql
CREATE TABLE products { sku: string, name: string, price: float, stock?: int }
```

### DROP TABLE
```sql
DROP TABLE products
```

### ALTER TABLE
```sql
ALTER TABLE products ADD discount?: float DEFAULT 0.0
ALTER TABLE products DROP stock
ALTER TABLE products RENAME price TO unit_price
ALTER TABLE products RENAME TO inventory
```

---

## DML — Rows

### INSERT
```sql
INSERT users { id: 1, name: "Alice", age: 30 }
```
Required (non-nullable) fields must be present. Types are validated and coerced.

### UPDATE
```sql
UPDATE users SET age = 31       IF id = 1
UPDATE users SET age += 5       IF name = "Bob"
UPDATE users SET score *= 1.1   IF active = true
```
Compound operators: `+=`, `-=`, `*=`, `/=`

### RM (delete)
```sql
RM FROM users IF age < 18
```

### CROP (keep top N)
```sql
CROP 100 DESC FROM products BY price   -- keep the 100 most expensive
CROP 50  ASC  FROM logs     BY date    -- keep the 50 oldest
```

---

## TAKE (query)

```sql
TAKE field, expr AS alias, * FROM table [clauses...]
```

### Clauses (in order)

| Clause         | Example                              | Notes                        |
|----------------|--------------------------------------|------------------------------|
| `IF`           | `IF age > 18 & active = true`        | Filter (WHERE)               |
| `JOIN`         | `JOIN orders ON users.id = orders.uid` | See JOIN section            |
| `GROUP BY`     | `GROUP BY department`                | Aggregate grouping           |
| `IF` (after GROUP BY) | `IF SIZE(*) > 5`            | Having                       |
| `ORDER BY`     | `ORDER BY age DESC`                  | `ASC`/`DESC` or `ABC`/`ZYX` |
| `FILTER`       | `FILTER name, dept`                  | Distinct by fields           |
| `TOP n`        | `TOP 10`                             | First n rows                 |
| `BOTTOM n`     | `BOTTOM 5`                           | Last n rows                  |
| `MIDDLE s e`   | `MIDDLE 3 7`                         | Rows s through e (1-indexed) |

### Aggregates

```sql
TAKE SIZE(*), SUM(price), MEAN(age), MAX(score), MIN(score) FROM table
TAKE dept, SIZE(*), MEAN(salary) FROM staff GROUP BY dept ORDER BY dept ASC
```

### Expressions in TAKE / UPDATE SET

Arithmetic with correct precedence:
```sql
TAKE price * 1.2 AS with_tax FROM products
TAKE first + " " + last AS full_name FROM contacts
```

### UNION
```sql
TAKE name FROM staff   IF dept = "eng"
UNION
TAKE name FROM contractors IF active = true
```

---

## Conditions

| Syntax                       | Meaning                                  |
|------------------------------|------------------------------------------|
| `field = value`              | Equality                                 |
| `field != value`             | Inequality                               |
| `field > value`              | Greater than (also `>=`, `<`, `<=`)      |
| `field = NIL`                | Is null                                  |
| `field != NIL`               | Is not null                              |
| `field LIKE "pat%"`          | Glob pattern (`%` = wildcard)            |
| `field IN (a, b, c)`         | Value in list                            |
| `field IN RANGE 10 20`       | Inclusive range                          |
| `field IN (TAKE ... FROM ...)` | Subquery                               |
| `cond & cond`                | AND                                      |
| `cond \| cond`               | OR                                       |
| `!cond`                      | NOT                                      |

Scalar subquery on RHS:
```sql
TAKE name FROM students IF grade > (TAKE MEAN(grade) FROM students)
```

---

## JOIN

```sql
TAKE name, total FROM users JOIN orders ON users.id = orders.uid
```

JaSQL uses a **hash join** for equality conditions — O(n+m), not O(n×m). Non-equality conditions fall back to a nested loop.

### KEEP ALL (left join)
```sql
TAKE name, total FROM users KEEP ALL, orders
```
Returns all rows from `users`, with NULLs for unmatched `orders` rows. Requires a prior `CONNECT`.

### CONNECT (define a relationship)
```sql
CONNECT users.id TO orders.user_id
```
`CONNECT` registers a foreign-key-style relationship and merges the tables. Required before `KEEP ALL`.

---

## Import and Export

### IMPORT
```sql
IMPORT data/people.datac              -- datac format (lazy, multi-table)
IMPORT exports/report.csv  TO sales   -- CSV (lazy)
IMPORT data/sheet.xlsx     TO budget  -- Excel (lazy, requires openpyxl)
```
All imports are **lazy** — schema is read eagerly; rows stream from disk only when queried. Filter pushdown means only matching rows enter memory.

### IMPORT classic-regex
```sql
IMPORT classic-regex
```
Activates the regex DSL (see Regex section). This is a module load, not a file import.

### SAVE / MTSAVE / ALLSAVE
```sql
SAVE users TO backup/users.datac
MTSAVE users, orders TO backup/snapshot.datac
ALLSAVE TO backup/full.datac
```

---

## Transactions

```sql
BEGIN
INSERT orders { id: 1, total: 500 }
UPDATE inventory SET qty -= 1 IF sku = "ABC"
COMMIT
```

```sql
BEGIN
UPDATE prices SET amount *= 1.05 IF category = "food"
ROLLBACK   -- undo everything since BEGIN
```

- `ROLLBACK` restores all tables, indexes, and connections to their state at `BEGIN`
- Nested `BEGIN` raises `Preposterous: TransactionError`
- `COMMIT` with no active transaction raises `Preposterous: TransactionError`

---

## Indexes

```sql
INDEX users ON id         -- build hash index on users.id
DROP INDEX users ON id    -- remove it
```

Indexes are used automatically for `IF field = value` conditions on eager (fully-loaded) tables.

---

## Regex DSL

Activate with `IMPORT classic-regex`, then use in any `IF` condition or `TAKE`/`UPDATE SET` expression.

### Conditions
```sql
TAKE email FROM contacts IF email MATCH "[a-z]+@corp\.com"   -- full match (anchored)
TAKE email FROM contacts IF email NOT MATCH "..."
TAKE name  FROM users    IF name  SEARCH "^A"               -- partial (unanchored)
TAKE name  FROM users    IF name  NOT SEARCH "bot"
```

### Expression functions

| Function                           | Returns                        |
|------------------------------------|--------------------------------|
| `regex.match(field, "pat")`        | bool — full match              |
| `regex.test(field, "pat")`         | bool — partial match           |
| `regex.search(field, "pat")`       | first match string or `""`     |
| `regex.replace(field, "pat", "r")` | replace first occurrence       |
| `regex.replace_all(f, "pat", "r")` | replace all occurrences        |
| `regex.count(field, "pat")`        | number of matches              |
| `regex.escape(field)`              | escape regex special chars     |
| `regex.find_all(field, "pat")`     | comma-joined list of matches   |
| `regex.groups(field, "pat")`       | comma-joined capture groups    |
| `regex.split(field, "pat")`        | comma-joined split result      |

```sql
TAKE regex.replace(email, "@.+", "@***") AS masked FROM contacts
UPDATE users SET email = regex.replace(email, "\\.com$", ".org") IF id = 1
```

---

## datac Format

JaSQL's native file format. Schema headers appear before each data block.

```
class int property: id
string sub.id: name
int    sub.id: age
set inclusion id.age to nil

users {
    id: 1, name: "Alice", age: 30
    id: 2, name: "Bob",   age: 25
    id: 3, name: "Carol"          ; age is nil — omitted
}
```

- `;` begins a comment
- Multi-table files are supported — each table is a separate named block
- `nil` fields are omitted (absent = null)

---

## Errors

All errors use the `Preposterous:` format with caret pointing to the offending token:

```
Preposterous: TableNotFound found ln:3 Char:12
  TAKE * FROM ghost
              ^
```

| Error kind        | Cause                                       |
|-------------------|---------------------------------------------|
| `TableNotFound`   | Table name doesn't exist                    |
| `TableExists`     | CREATE TABLE on an existing name            |
| `MissingFields`   | INSERT missing a required (non-nullable) field |
| `TypeError`       | Value doesn't match schema type             |
| `SyntaxError`     | Unrecognised clause or malformed statement  |
| `AlterError`      | ALTER TABLE on missing/protected field      |
| `TransactionError`| Nested BEGIN or COMMIT outside transaction  |
| `IOError`         | File write failure in SAVE                  |
| `NoTables`        | ALLSAVE with no tables defined              |

---

## SCT Rating

Rated using the **Shariff Complexity Theorem** (SCT, Abu Shariff, 2026):

| Dimension          | Value | Notes                                              |
|--------------------|-------|----------------------------------------------------|
| Coordinate (x, y)  | (8, 85) | Very far from machine semantics; near English    |
| Esotericosity E    | 8     | min(8, 85) — low, DSL reads like English           |
| Work Distribution  | 12    | Runtime handles joins, validation, lazy load; programmer learns relational model |
| Barrier tax        | +0    | Runs on any Python install, free                   |
| **SCT**            | **9.2** | 0.7(8) + 0.3(12) = 5.6 + 3.6                   |

For reference: Python = 10.0, ASM = 30, Lambda Calculus = 35.
JaSQL scores below Python because it is a DSL with a single domain — the runtime absorbs far more complexity than a general-purpose language runtime can.

---

## File Overview

| File              | Purpose                                       |
|-------------------|-----------------------------------------------|
| `abudb.py`        | In-memory database engine (Table, Database)   |
| `jasql.py`        | JaSQL parser, evaluator, dispatcher           |
| `datac_parser.py` | datac file format parser (eager + streaming)  |
| `repl.py`         | Interactive REPL                              |
| `test_jasql.py`   | pytest suite (59 tests)                       |
| `*.datac`         | datac data files                              |
| `*.jasql`         | JaSQL script files                            |

---

Part of the **Abu Ecosystem** — AC · AI · AC+ · AbuDB · JaSQL  
Abu Shariff · 2026 · abu.shariffaiml@gmail.com
