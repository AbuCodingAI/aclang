## The ACX binary — AC+'s native executable format

Unlike AC's `.acb` (which wraps ELF/PE), `.acx` is its own format designed for bare-metal.

---

### Layout

```
Offset  Size  Field
──────  ────  ──────────────────────────────────────────────────────────────
0       4     Magic: 0x41 0x43 0x58 0x21  ("ACX!")
4       1     Architecture: 0x01 = x86-64 | 0x02 = ARM
5       8     Entry point — 64-bit little-endian offset into code section

[13+]         Import section (only present if program has imports):
              0x70              — import section start
              [0x7N][name\0]…   — one entry per import:
                                    type byte + null-terminated name
                                    0x71 ilib  0x72 elib  0x73 clib  0x74 flib
              0x00              — end of import section

[after]       Code section — flat machine code; entry offset 0x0 = __ai_main__
```

### Minimal example (no imports)

```
41 43 58 21 01 00 00 00 00 00 00 00 00  [code...]
│           │  └─ Entry = 0x0 ────────┘
└─ "ACX!"   └─ x86-64
```

### Example with one flib import

```
41 43 58 21 01 00 00 00 00 00 00 00 00
70                                   ← import section start
74 6C6F67 69632E 616370 00           ← 0x74(flib) + "logic.acp\0"
00                                   ← end of imports
[code...]
```

---

### Import type bytes

| Byte | Keyword | Meaning                                                       |
|------|---------|---------------------------------------------------------------|
| 0x71 | `ilib`  | AC built-in / standard libraries — resolved from the `ilib/` folder              |
| 0x72 | `elib`  | Packages installed via **atar** (AC's package manager, `.tar` bundles) — `elib/` folder |
| 0x73 | `clib`  | Custom libraries you made yourself — placed as a subfolder inside `clib/`         |
| 0x74 | `flib`  | A single `.ac`/`.acp` file imported by path — can live anywhere on disk           |

`flib` is the only library type that is not folder-based — it is always a single file referenced by path. `flib` imports are inlined into the code section at compile time; the import entry is metadata for tooling.

---

### Entry point

Always `0x0` for programs compiled by `acp` — `__ai_main__` is emitted first in the generated assembly and linked at `-Ttext 0x0`.
