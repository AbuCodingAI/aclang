# atar: AC Package Manager

**Very simple. Just works.**

---

## Philosophy

```
npm → bloated, 1000 dependencies for hello world
pip → virtual env hell
cargo → 5 minute compile times
Atar → paste URL, download, use. Done.
```

---

## Installation

```bash
npm install -g atar
```

---

## Quick Start

### **1. Create Package**

```bash
cd my_ac_lib
atar init
```

Creates `atar.yaml`:
```yaml
name: my_ac_lib
version: 1.0.0
author: Your Name
description: My awesome AC library
main: lib.ac
license: MIT
```

### **2. Publish Package**

```bash
atar publish
```

Uploads to central registry. Done.

### **3. Use Package**

```bash
atar add github.com/user/my_ac_lib
```

Adds to local `atar.lock`:
```yaml
my_ac_lib: github.com/user/my_ac_lib@1.0.0
```

Code automatically fetches and caches.

### **4. Use in Code**

```ac
use my_ac_lib

result = my_ac_lib.my_function(42)
```

Done.

---

## Directory Structure

```
my_project/
  atar.yaml          /* Package metadata */
  atar.lock          /* Locked versions */
  atar_modules/      /* Downloaded packages (cached) */
    my_ac_lib/
      lib.ac
      atar.yaml
  src/
    main.ac          /* Your code */
```

---

## atar.yaml Format

```yaml
# Required
name: my_ac_lib
version: 1.0.0
main: lib.ac

# Optional
author: Your Name
description: Description
license: MIT
repository: github.com/user/my_ac_lib
homepage: https://example.com
tags: [utils, math, string]

# Dependencies
dependencies:
  other_lib: github.com/user/other_lib@2.0.0
  another_lib: github.com/user/another_lib@*  /* Any version */

# Optional: what to include in package
include:
  - lib.ac
  - lib/
  - README.md
  - LICENSE
```

---

## Commands

### **Init**
```bash
atar init
```
Creates `atar.yaml` interactively.

### **Add**
```bash
atar add github.com/user/my_lib
atar add github.com/user/my_lib@1.2.3   /* Specific version */
atar add github.com/user/my_lib@^1.0.0  /* Caret (compatible) */
```

Adds to `atar.yaml` + downloads to `atar_modules/`

### **Remove**
```bash
atar remove my_lib
```

### **Update**
```bash
atar update                    /* Update all */
atar update my_lib             /* Update specific */
atar update my_lib@2.0.0       /* Update to version */
```

### **List**
```bash
atar list                      /* Local packages */
atar list -g                   /* Global packages */
atar list --outdated          /* Show updates available */
```

### **Publish**
```bash
atar publish
```

Uploads to registry. Requires:
- `atar.yaml` with name + version
- Git repo with valid origin
- Not already published at that version

### **Publish (Private)**
```bash
atar publish --registry https://my-registry.com
```

Uses custom registry instead of central.

### **Search**
```bash
atar search math              /* Search for "math" packages */
atar search "matrix lib"
```

Shows top results from registry.

### **Info**
```bash
atar info github.com/user/my_lib
```

Shows version history, downloads, etc.

### **Cache**
```bash
atar cache clean             /* Clear cache */
atar cache list              /* Show cached packages */
```

### **Install Dependencies**
```bash
atar install                 /* Read atar.lock, fetch all */
```

---

## Version Scheme

**Semantic Versioning:** `MAJOR.MINOR.PATCH`

```
1.0.0  → stable
1.0.1  → bug fix
1.1.0  → new feature, backwards compatible
2.0.0  → breaking change
```

### **Version Ranges**

```
^1.0.0  → >=1.0.0, <2.0.0 (caret: minor+patch allowed)
~1.0.0  → >=1.0.0, <1.1.0 (tilde: patch only)
1.0.0   → exactly 1.0.0
*       → any version
1.x.x   → any 1.y.z
```

---

## Registry

### **Central Registry** (Default)

```
registry.aclang.dev
```

Stores all public packages.

### **Private Registries**

```bash
/* In atar.yaml */
registry:
  - name: central
    url: https://registry.aclang.dev
    
  - name: private
    url: https://my-company-registry.com
    auth: token
```

```bash
/* Or CLI */
atar add my_company/my_lib --registry private
```

### **GitHub as Registry** (Simplest)

```bash
atar add github.com/user/my_lib
/* Automatically uses GitHub as registry */
/* Clones from GitHub, caches locally */
```

---

## Lock File (atar.lock)

```yaml
# Auto-generated, should be committed

my_lib: github.com/user/my_lib@1.0.0 (hash: abc123def456)
other_lib: github.com/other/lib@2.1.0 (hash: xyz789uvw012)
nested_dep: github.com/dep/dep@0.5.0 (hash: 456def789abc)
```

**Why:** Ensures exact same versions on all machines.

```bash
git add atar.lock
git commit -m "Lock dependencies"
```

---

## Conflict Resolution

### **Diamond Dependency**

```
my_project
├── lib_a@1.0.0
│   └── common@2.0.0
└── lib_b@1.0.0
    └── common@1.0.0    /* Conflict! */
```

**Solution:** Atar's smart resolver

1. Check if `2.0.0` is compatible with `1.0.0` (semver)
2. If yes: use `2.0.0` (newer version compatible)
3. If no: error + suggestion

```
Error: Dependency conflict
  lib_a requires common@2.0.0
  lib_b requires common@1.0.0
  
Solution: Update lib_b to a version that accepts common@2.0.0
  atar update lib_b
```

---

## How It Works Under the Hood

```
1. User: atar add github.com/user/my_lib
   ↓
2. Parse URL, fetch atar.yaml from GitHub
   ↓
3. Check version compatibility
   ↓
4. Clone repo to atar_modules/my_lib/
   ↓
5. Recursively fetch dependencies
   ↓
6. Update atar.yaml + atar.lock
   ↓
7. AC compiler finds modules in atar_modules/
   ↓
8. Done! Use with: use my_lib
```

---

## Example: Complete Workflow

### **Step 1: Create Package**

```bash
mkdir my_string_lib
cd my_string_lib
atar init
```

Prompts:
```
Name: my_string_lib
Version: 1.0.0
Description: String utilities
Author: You
Main file: lib.ac
License: MIT
```

### **Step 2: Write Code**

```ac
/* lib.ac */
Make reverse func(s: str) -> str
    /* String reversal logic */
    RETURN reversed_string
Make

Make uppercase func(s: str) -> str
    RETURN s.to_upper()
Make
```

### **Step 3: Publish**

```bash
git init
git add .
git commit -m "Initial release"
git push origin main

atar publish
```

```
✅ Published my_string_lib@1.0.0
Uploaded to registry
Available at: github.com/you/my_string_lib
```

### **Step 4: Use in Project**

```bash
mkdir my_project
cd my_project
atar init

atar add github.com/you/my_string_lib
```

### **Step 5: Use in Code**

```ac
AC->PY

use my_string_lib

text = $hello world$
reversed = my_string_lib.reverse(text)
Term.display reversed  /* dlrow olleh */
```

### **Step 6: Run**

```bash
ac main.ac
```

Atar automatically:
1. Fetches my_string_lib from cache or GitHub
2. Compiles dependencies
3. Links your code
4. Runs result

---

## Special Features

### **Monorepo Support**

```
my_monorepo/
  atar.yaml
  packages/
    utils/
      lib.ac
      atar.yaml
    math/
      lib.ac
      atar.yaml
```

### **Local Path Dependencies**

```yaml
# In atar.yaml
dependencies:
  local_lib: ./packages/utils
  remote_lib: github.com/user/lib@1.0.0
```

### **Git Branch/Tag Dependencies**

```yaml
dependencies:
  unstable: github.com/user/lib@main        /* Latest from main branch */
  latest: github.com/user/lib@v2.0.0        /* Git tag */
```

### **Custom Registries**

```bash
atar config set registry https://my-registry.com
atar add my_lib  /* Uses custom registry */
```

---

## Security

### **Checksum Verification**

```yaml
# atar.lock includes checksums
my_lib: github.com/user/my_lib@1.0.0 
  hash: sha256:abc123...
```

Verified on every install.

### **Audit**

```bash
atar audit          /* Check for known vulnerabilities */
atar audit --fix    /* Auto-upgrade vulnerable deps */
```

### **Sandboxing** (Future)

Packages run in sandbox by default (no file access, no os.bash).

---

## CLI Summary

```bash
atar init                               # Create new package
atar add PACKAGE[@VERSION]              # Add dependency
atar remove PACKAGE                     # Remove dependency
atar update [PACKAGE]                   # Update to latest
atar install                            # Install from lock file
atar publish [--registry URL]           # Publish package
atar search QUERY                       # Search packages
atar info PACKAGE                       # Package info
atar list [-g|--outdated]              # List packages
atar cache clean                        # Clear cache
atar audit [--fix]                     # Security check
atar config set KEY VALUE              # Configure
```

---

## Philosophy Summary

| Feature | npm | pip | Atar |
|---------|-----|-----|------|
| **Simplicity** | ❌ 100MB node_modules | ❌ venv hell | ✅ atar_modules/ |
| **Speed** | ❌ Slow | ❌ Slow | ✅ Fast |
| **Clarity** | ❌ Confusing | ❌ Complex | ✅ Simple |
| **Git native** | ❌ No | ❌ No | ✅ Yes |
| **Caching** | ❌ Bad | ❌ Meh | ✅ Good |
| **Conflicts** | ❌ Nested deps | ❌ Frozen deps | ✅ Smart resolver |

---

## Status

- ✅ Design complete
- ⏳ CLI implementation (Phase 1)
- ⏳ Registry backend (Phase 2)
- ⏳ Security audit (Phase 3)

---

## One-Liner

> **Atar: Paste GitHub URL, get code, move on with your life.**

