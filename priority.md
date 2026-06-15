# AC 0.3 + AI 0.2 — Priority Chart

## Scoring System

Each item is rated 1–10 on four axes. **Lower = better / easier / do first.**

| Axis | Meaning | Weight |
|------|---------|--------|
| **Size** | Implementation size (1=tiny, 10=huge) | 25% |
| **Bugs** | Bug proneness (1=safe, 10=landmine) | 20% |
| **Quality** | How much the improvement improves the language (1=high, 10=low) | 15% |
| **Impact** | Impact on AC/AI legacy (1=high, 10=low) | 40% |

**Weighted score = Size×0.25 + Bugs×0.20 + Quality×0.15 + Impact×0.40**  
Sort ascending — lower score = do this first.

---

## Ratings

| # | Item | Size | Bugs | Quality | Impact | **Score** |
|---|------|------|------|---------|--------|-----------|
| 1 | Wire `libacmath.so` into AI | 1 | 1 | 2 | 1 | **1.25** |
| 2 | Wire `libaccamera.so` into AI | 2 | 2 | 2 | 1 | **1.65** |
| 3 | Automatic binary execution after compile | 2 | 2 | 3 | 2 | **2.15** |
| 4 | C-based AI VM codegen (replace ASM) | 5 | 4 | 2 | 1 | **3.05** |
| 5 | Better `.acb` files | 3 | 3 | 3 | 2 | **2.70** |
| 6 | Cache system (`.lir` / `.irc` / `.acc`) | 4 | 4 | 2 | 2 | **2.80** |
| 7 | Improve IR | 5 | 4 | 2 | 2 | **3.25** |
| 8 | Tag system improvements | 4 | 5 | 3 | 2 | **3.35** |
| 9 | Faster compiler | 5 | 3 | 2 | 2 | **3.20** |
| 10 | AI terminal IDE | 5 | 3 | 3 | 2 | **3.45** |
| 11 | Every backend same features | 7 | 6 | 1 | 2 (Dev here, I'd say WAY more, each backend has a specialty and if AC can target them all the same, it'd be crazy good) | **4.35** |
| 12 | `AC->LIB` / `AC LIB` headers (no `<mainloop>`) | 6 | 5 | 2 | 3 | **4.30**(Dev again, very important for multi-file projects) |
| 13 | Speed tests | 3 | 1 | 5 | 5 | **3.80** |
| 14 | machine-audio library (C++ `.so`) | 8 | 7 | 2 | 3 | **5.05** |
| 15 | Experimental binary linker | 9 | 9 | 3 | 4(BNY becomes the fastest backend by a big margin and complete as well(Dynamic linking) so I mean, is it really a 4?) | **6.85** |

---

## Priority Order (sorted by score)

| Priority | Item | Score | Notes |
|----------|------|-------|-------|
| 1 | Wire `libacmath.so` into AI | 1.25 | `.so` already exists — just wire the AI compiler to load it |
| 2 | Wire `libaccamera.so` into AI | 1.65 | `.so` already exists — same pattern as math |
| 3 | Automatic binary execution | 2.15 | Small change in compiler driver; `execv` after compile |
| 4 | Better `.acb` files | 2.70 | Format improvement; isolated change |
| 5 | Cache system (`.lir` / `.irc` / `.acc`) | 2.80 | High leverage — speeds up all downstream work |
| 6 | C-based AI VM codegen | 3.05 | Uses AC's C relic as template; cross-platform payoff is massive |
| 7 | Faster compiler | 3.20 | Profiling + hot-path work; low bug risk |
| 8 | Improve IR | 3.25 | Foundational; do after cache so you can iterate fast |
| 9 | Tag system improvements | 3.35 | Medium scope; depends on IR being clean |
| 10 | AI terminal IDE | 3.45 | Standalone tool; can be built in parallel |
| 11 | Speed tests | 3.80 | Benchmark harness; do after compiler + IR work so numbers mean something |
| 12 | `AC->LIB` / `AC LIB` headers | 4.30 | Design work needed; affects entry-point rules |
| 13 | Every backend same features | 4.35 | Large surface area; do after IR is stable |
| 14 | machine-audio library (C++ `.so`) | 5.05 | Significant C++ effort; external lib integration (portaudio/etc.) |
| 15 | Experimental binary linker | 6.85 | High complexity + high risk; last |

---

## Rationale for Weights

- **Impact (40%)** dominates because the goal is AC's long-term legacy. A tiny fix with massive user-facing payoff beats a hard feature nobody notices.
- **Size (25%)** second — smaller tasks ship faster and unblock the rest.
- **Bugs (20%)** third — bug-prone work on a shaky base multiplies debt.
- **Quality (15%)** last — quality improvements matter but are less urgent than stability and reach.
