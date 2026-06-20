# atar: AC Package Manager

**Very simple. Independent. Cross-platform.**

---

## Philosophy

```
apt/npm/pip → platform-specific, dependency hell
atar → self-contained .tar files, works everywhere
```

atar is **NOT** a wrapper around system package managers. It's its own standalone PM.

---

## Installation

```bash
npm install -g atar
```

Or: download prebuilt binary for your platform.

---

## Quick Start

### **1. Create Package in clib/**

```bash
cd my_project/clib
mkdir my_string_lib
cd my_string_lib
```

Create `lib.ac`:
```ac
Make reverse func(s: str) -> str
    RETURN reversed
Make

Make uppercase func(s: str) -> str
    RETURN s.to_upper()
Make
```

### **2. Make Package Ready**

```bash
cd my_string_lib
atar ready
```

**What `atar ready` does:**
1. Checks for `LICENSE` file
2. Creates `atar.yaml` with license info (or "NO LICENSE")
3. Packages everything into `my_string_lib.tar`
4. Ready to publish!

Output:
```
✅ Created my_string_lib.tar
License: MIT (auto-detected)
Ready to publish: atar publish my_string_lib.tar
```

### **3. Publish Package**

```bash
atar publish my_string_lib.tar
```

Uploads to central registry.

### **4. Use in Another Project**

```bash
cd other_project
atar install my_string_lib
```

Downloads to `elib/my_string_lib/`

### **5. Use in Code**

```ac
use my_string_lib

text = $hello world$
reversed = my_string_lib.reverse(text)
```

---

## Directory Structure

```
my_project/
  clib/                      /* Computer Libraries (internal) */
    my_string_lib/
      lib.ac
      LICENSE
      atar.yaml              /* Auto-created by atar ready */
      my_string_lib.tar      /* Auto-created by atar ready */
    another_lib/
      lib.ac
      atar.yaml
  
  elib/                       /* External Libraries (downloaded) */
    installed_lib/
      lib.ac
      atar.yaml
    another_installed/
      lib.ac
      atar.yaml
  
  src/
    main.ac                   /* Your code uses: use installed_lib */
```

---

## atar.yaml Format

**User-created version:**

```yaml
# Required
name: my_string_lib
version: 1.0.0
main: lib.ac

# Optional (auto-filled by atar ready)
author: Your Name
description: String utilities library
license: MIT                    # or "NO LICENSE"
repository: github.com/you/my_string_lib
homepage: https://example.com
tags: [strings, utils]

# Optional: what to include in .tar
include:
  - lib.ac
  - LICENSE
  - README.md
```

**Auto-created by `atar ready`:**

```bash
atar ready
# Reads LICENSE file
# Creates/updates atar.yaml with license field
# Packages into .tar
```

---

## Commands

### **atar ready**
```bash
atar ready
```

Auto-generates `atar.yaml` + creates `.tar` file.

**What it does:**
1. Reads `LICENSE` file if present
2. Creates/updates `atar.yaml` with detected license
3. Packages everything into `{name}.tar`
4. Prints: "Ready to publish: `atar publish {name}.tar`"

**Output:**
```
✅ atar.yaml created
📦 Package: my_string_lib.tar
🔐 License: MIT
📝 Ready: atar publish my_string_lib.tar
```

### **atar publish**
```bash
atar publish my_string_lib.tar
```

Uploads `.tar` to central registry.

**Requirements:**
- `.tar` file exists
- `atar.yaml` inside it
- Name + version not already published

### **atar publish (Local)**
```bash
atar publish --local my_string_lib.tar
```

Publishes locally (no upload to registry).

### **atar get**
```bash
atar get my_string_lib
```

Shows package info:
```
Name: my_string_lib
Version: 1.0.0
Author: You
License: MIT
Description: String utilities library
Repository: github.com/you/my_string_lib
Size: 2.3KB
```

### **atar install**
```bash
atar install my_string_lib          /* Global installation */
atar install my_string_lib --local  /* Local to elib/ */
```

**Global:**
- Downloads to system atar directory
- Accessible from any AC project

**Local:**
- Downloads to `./elib/` 
- Only accessible from current project

### **atar list**
```bash
atar list              /* List installed packages */
atar list -g           /* List globally installed */
atar list --local      /* List in local elib/ */
```

### **atar search**
```bash
atar search string     /* Search registry for "string" */
atar search "utils"
```

Returns matching packages with versions.

### **atar remove**
```bash
atar remove my_lib             /* Remove globally */
atar remove my_lib --local     /* Remove from elib/ */
```

### **atar cache**
```bash
atar cache clean       /* Clear downloaded .tar files */
atar cache list        /* Show cached packages */
```

---

---

## How It Works

### **Creating a Package**

```
1. Write code in clib/my_lib/
2. Add LICENSE file (MIT, Apache, etc.)
3. atar ready → creates atar.yaml + my_lib.tar
4. atar publish my_lib.tar → uploads to registry
```

### **Using a Package**

```
1. atar install my_lib → downloads my_lib.tar
2. Extracts to elib/my_lib/
3. use my_lib in code
4. Compiler finds it in elib/ and links it
```

### **Package Distribution**

**Why .tar files?**

```
✅ Works everywhere (Linux, Windows, macOS)
✅ No platform-specific dependencies
✅ Self-contained (no apt/brew/choco needed)
✅ Can be version-controlled (git)
✅ Portable (download once, use anywhere)
❌ Not dependent on system package managers
```

---

## Registry

### **Central Registry**

```
registry.aclang.dev
```

Default location for all published packages.

### **Local Registry** (`--local`)

```bash
atar publish --local my_lib.tar
```

Publishes locally without uploading to central registry.

---

## Folder Structure

**clib/ - Computer Libraries (Development)**

```
clib/
  math_lib/
    lib.ac
    LICENSE
    atar.yaml
    math_lib.tar
  string_lib/
    lib.ac
    LICENSE
    atar.yaml
    string_lib.tar
```

Use for:
- ✅ Internal testing
- ✅ Package development
- ✅ Before publishing

**elib/ - External Libraries (Installation)**

```
elib/
  math_lib/
    lib.ac
    atar.yaml
  string_lib/
    lib.ac
    atar.yaml
  networking_lib/
    lib.ac
    atar.yaml
```

Use for:
- ✅ Downloaded packages from atar
- ✅ External dependencies
- ✅ Installed globally or locally

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

### **Step 1: Create Package in clib/**

```bash
cd my_project/clib
mkdir my_string_lib
cd my_string_lib
```

Create `lib.ac`:
```ac
Make reverse func(s: str) -> str
    RETURN reversed_string
Make

Make uppercase func(s: str) -> str
    RETURN s.to_upper()
Make
```

Create `LICENSE` (MIT):
```
MIT License
Copyright (c) 2026 You
...
```

### **Step 2: Make Package Ready**

```bash
atar ready
```

Output:
```
✅ atar.yaml created
📦 Package: my_string_lib.tar
🔐 License: MIT
📝 Ready: atar publish my_string_lib.tar
```

What happened:
- Read LICENSE file → auto-detected MIT
- Created atar.yaml with license
- Packaged everything into my_string_lib.tar
- Ready to publish!

### **Step 3: Publish**

```bash
atar publish my_string_lib.tar
```

Output:
```
✅ Published my_string_lib@1.0.0
📍 Registry: registry.aclang.dev
🔗 Available globally
```

### **Step 4: Use in Another Project**

```bash
cd other_project
atar install my_string_lib
```

Output:
```
📥 Downloading: my_string_lib.tar
✅ Installed to elib/my_string_lib/
```

Folder structure:
```
other_project/
  elib/
    my_string_lib/
      lib.ac
      atar.yaml
  src/
    main.ac
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
ac src/main.ac
```

Compiler:
1. Sees `use my_string_lib`
2. Finds it in `elib/my_string_lib/`
3. Links it
4. Runs

---

## Special Features

### **License Auto-Detection**

```bash
atar ready
```

Automatically reads LICENSE file and includes in atar.yaml:

```yaml
license: MIT   # auto-detected
```

Or marks as "NO LICENSE" if missing.

### **Multiple Installations**

**Global:**
```bash
atar install my_lib
```
Available to all AC projects.

**Local:**
```bash
atar install my_lib --local
```
Only in current project's elib/

### **Package Management**

```bash
atar list              /* Show installed */
atar get my_lib        /* Show package info */
atar remove my_lib     /* Uninstall */
atar search string     /* Search registry */
```

---

## Security

### **License Tracking**

Every package includes license in atar.yaml.

```bash
atar get my_lib
# Output includes: License: MIT
```

### **Checksums** (Future)

Verify downloaded .tar integrity:
```bash
atar install my_lib --verify
```

### **Audit** (Future)

Check for known vulnerabilities:
```bash
atar audit
```

---

## CLI Summary

```bash
atar ready [path]                       # Auto-create atar.yaml + .tar
atar publish PACKAGE.tar                # Publish to registry
atar publish PACKAGE.tar --local        # Publish locally
atar install PACKAGE                    # Install globally to system
atar install PACKAGE --local            # Install to local elib/
atar get PACKAGE                        # Show package info
atar list                               # List installed packages
atar list --local                       # List in local elib/
atar search QUERY                       # Search registry
atar remove PACKAGE                     # Uninstall
atar remove PACKAGE --local             # Uninstall from elib/
atar cache clean                        # Clear cache
```

---

## Architecture Summary

| Aspect | Design |
|--------|--------|
| **Package format** | .tar (self-contained, cross-platform) |
| **Development** | clib/ (Computer Libraries) |
| **Installation** | elib/ (External Libraries) |
| **Metadata** | atar.yaml (user or auto-created) |
| **License** | Auto-detected from LICENSE file |
| **Registry** | Central (registry.aclang.dev) + local |
| **Independence** | NOT dependent on apt/brew/choco |
| **Platform support** | Windows, Linux, macOS (all equal) |

---

## vs npm/pip

| | npm | pip | **atar** |
|---|-----|-----|----------|
| **Bloat** | 🤦 100MB | 🤦 venv hell | ✅ Clean |
| **Speed** | ❌ Slow | ❌ Slow | ✅ Fast |
| **Cross-platform** | ⚠️ Meh | ❌ No | ✅ Perfect |
| **WSL/Windows** | ⚠️ Meh | ❌ Bad | ✅ Works |
| **Simplicity** | ❌ Complex | ❌ Complex | ✅ Simple |
| **Independence** | ❌ Requires npm | ❌ Requires pip | ✅ Self-contained |
| **License tracking** | ❌ Optional | ❌ Optional | ✅ Automatic |

---

---

## Recommended Practices

### **1. Create Your Own atar.yaml**

**Best practice:** Write atar.yaml manually, don't rely only on `atar ready`.

```yaml
name: my_string_lib
version: 1.0.0
author: Your Name
description: String utilities and helpers
license: MIT
repository: github.com/you/my_string_lib
homepage: https://example.com
main: lib.ac
tags: [strings, utils, text-processing]
```

**Why:**
- ✅ Clear intent and metadata
- ✅ Repository + homepage for users
- ✅ Tags help with discovery
- ✅ Version control (check in git)

**atar ready supplements this:**
- Auto-detects license from LICENSE file
- Updates atar.yaml if missing license
- Ensures all required fields present

---

### **2. Use a License**

**Always include a LICENSE file.**

```bash
cd clib/my_lib
echo "MIT License" > LICENSE
# Or copy full license text
```

**Why:**
- ✅ Users know what they can do with your code
- ✅ atar auto-detects and includes in atar.yaml
- ✅ Legal clarity (you're protected)
- ✅ Increases adoption (people trust licensed code)

**Common licenses for AC libraries:**

```
MIT              → Permissive, most popular
Apache 2.0       → Permissive with patent protection
GPL 3.0          → Share-alike (code sharing required)
Unlicense        → Public domain
```

**No license?**
```bash
atar ready
# Output: 🚨 Warning: NO LICENSE detected
# atar.yaml shows: license: NO LICENSE
```

Users will hesitate to use unlicensed code.

---

### **3. Naming Conventions**

**Package names should be:**

#### **Clear & Descriptive**
```
✅ string_utils
✅ math_lib
✅ networking_tools
❌ stuff
❌ lib
❌ util
```

#### **Lowercase with underscores**
```
✅ my_string_lib
✅ http_client
✅ json_parser
❌ MyStringLib
❌ httpClient
❌ JSON-Parser
```

#### **Avoid conflicts**
```
✅ my_project_math        (prefix to avoid collision)
✅ awesome_http_client    (descriptive + prefix)
❌ math                   (too generic, likely taken)
❌ http_client            (common name, collisions)
```

#### **Length guideline**
```
Good: 2-4 words
  ✅ string_utils
  ✅ http_client_lib
  ✅ json_parser
  
Avoid: Too short or too long
  ❌ str              (too short, unclear)
  ❌ my_awesome_super_string_utilities_library  (too long)
```

#### **Examples of Good Names**

```
crypto_lib           → cryptography library
regex_engine         → regex utilities
file_io_utils        → file operations
concurrent_tools     → concurrency primitives
data_struct          → data structures
ui_components        → UI building blocks
ml_inference         → machine learning
web_framework        → web server framework
```

#### **Avoid These Patterns**

```
❌ ac_*              (redundant, already in AC)
❌ lib_*             (redundant, it's a library)
❌ *_lib             (can use just the name)
❌ test_*            (not for public packages)
❌ tmp_*             (temporary, unprofessional)
```

**Better:**
```
❌ ac_crypto        →  ✅ crypto_utils
❌ lib_http         →  ✅ http_client
❌ string_lib       →  ✅ string_utils
```

---

## Example: Complete Best Practices

### **clib/awesome_parser/atar.yaml**

```yaml
# User-written, not auto-generated
name: awesome_parser
version: 1.2.0
author: Jane Doe
description: Fast JSON and XML parser for AC

# License is auto-detected from LICENSE file
license: MIT

# User provides details
repository: github.com/janedoe/awesome_parser
homepage: https://awesome-parser.example.com
main: lib.ac

# Tags help with discovery
tags: [json, xml, parsing, utilities]
```

### **clib/awesome_parser/LICENSE**

```
MIT License

Copyright (c) 2026 Jane Doe

Permission is hereby granted, free of charge, to any person obtaining a copy...
```

### **Publish**

```bash
cd clib/awesome_parser
atar ready
# ✅ Reads LICENSE → MIT detected
# ✅ atar.yaml verified
# ✅ awesome_parser.tar created

atar publish awesome_parser.tar
# ✅ Published to registry
# Users can now: atar install awesome_parser
```

---

## Status

- ✅ Architecture complete
- ✅ Best practices documented
- ⏳ CLI implementation (Phase 1)
- ⏳ Registry backend (Phase 2)
- ⏳ License verification (Phase 3)

---

## Summary

**Three principles:**

1. **Write atar.yaml manually** → Clear metadata + intent
2. **Always use a License** → Legal clarity + user trust
3. **Follow naming conventions** → Discoverability + professionalism

**One-Liner:**

> **atar: .tar files + clear names + proper licenses = trusted ecosystem.**

🎯

