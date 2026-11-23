# Repository Agent Directives (`esp32_touchscreen`)

## 1. MANDATORY: Spec-Driven Development (`SPEC.md` -> `specs/` -> Implementation)

**Hard Directive:** This repository follows strict spec-driven development. Behavioral, architectural, and protocol specifications live in `SPEC.md` and `specs/*.md`, complemented by hardware/setup documentation in `README.md` and `docs/`.

### Workflow Rules

1. **Source of Truth:**
   - Treat `SPEC.md` and `specs/*.md` (alongside hardware pinouts/datasheets in `README.md` and `docs/`) as the canonical specification for architecture, NVS schemas, UI flows, and observable behavior.
2. **Discuss & Confirm Before Editing:**
   - Any proposed modification to behavior, architecture, NVS schemas, hardware configuration, or contracts **MUST** first be discussed with concrete suggestions and explicitly confirmed by the user before modifying `SPEC.md`, `specs/*.md`, or source files.
3. **Propagate in Strict Order (`SPEC.md` / `specs/` -> Project):**
   - Once confirmed, **first** integrate the change into `SPEC.md` and the affected `specs/*.md` files (and `README.md` / `platformio.ini` when hardware or build config changes), and **then** replicate those specifications into `include/`, `src/`, and `test/`.
   - Do not implement un-specced behavior ahead of `specs/*.md`.

---

## 2. MANDATORY: Regression Testing & Coverage Strategy (ESP32 / PlatformIO)

### Toolchain & Environment
- **PlatformIO CLI:** `C:\Users\marco\.platformio\penv\Scripts\pio.exe` (with `platforms/`, `tools/`, and `penv/` under `C:\Users\marco\.platformio`).
- **Host Native Toolchain:** `C:\cygwin64\bin` (install `mingw64-x86_64-gcc-g++` or `gcc-g++` + `gcov` for `[env:native]`, and `gcovr` in `penv` via `C:\Users\marco\.platformio\penv\Scripts\python.exe -m pip install gcovr`).

### 3-Tier Test & Coverage Rules
1. **Tier 1 — Host `native` Unit & Regression Tests (`pio test -e native`):**
   - Keep pure logic (Prometheus metrics parsing, affine touch calibration math, Wi-Fi list ordering/NVS serialization, UI state machines) decoupled from `<Arduino.h>`, `<WiFi.h>`, `<HTTPClient.h>`, `Preferences.h`, and `LovyanGFX` so it compiles on the host using PlatformIO's Unity framework.
   - Every bug fix or new pure-logic feature **MUST** include a corresponding regression test under `test/`.
2. **Tier 2 — Line & Branch Coverage (`gcov` + `gcovr` on `native`):**
   - Measure coverage on `[env:native]` (compiled with `--coverage`) using `gcovr`, filtering to `src/` and excluding `.pio/`, `test/`, and `lib/` (LVGL/LovyanGFX).
3. **Tier 3 — Firmware Build & On-Device Smoke Gate (`upesy_wroom`):**
   - Every firmware code or `lv_conf.h` / `platformio.ini` change **MUST** pass `pio run -e upesy_wroom` to verify ESP32 SDK compilation and `huge_app.csv` flash budget.
   - Reserve `pio test -e upesy_wroom` for hardware-bound integration checks (NVS, I2C CST820 touch, LGFX display) when a target board is connected.

---

## 3. MANDATORY: Git Commit at End of Every Prompt Execution

**Hard Directive:** Whenever a prompt execution creates, modifies, or deletes any file in this repository, you **MUST** stage those changes and create a descriptive git commit **before finishing your final response**.

### Commit Rules & Hygiene

1. **One Logical Commit Per Prompt Round:**
   - Do not leave uncommitted changes in the working tree at the end of a prompt execution.
   - Stage only the files intended for the repository (`git add ...`) and verify nothing stray remains (`git status --short` must be clean for tracked/intended files before concluding).
2. **Conventional Commits Format:**
   - Use Conventional Commits headers:
     - `feat(<scope>): ...` — new features or capabilities
     - `fix(<scope>): ...` — bug fixes
     - `docs(<scope>): ...` — documentation or `specs/` updates
     - `refactor(<scope>): ...` — non-behavioral restructuring
     - `test(<scope>): ...` — tests and fixtures
     - `chore(<scope>): ...` — build/tooling/maintenance updates
   - Common scopes in this repository: `ui`, `display`, `touch`, `network`, `metrics`, `build`, `tests`, `docs`, `specs`, `agent`.
3. **Never Commit Secrets or Generated Artifacts:**
   - Never commit hardcoded Wi-Fi SSIDs/passwords, API keys, `.env`, or NVS dumps.
   - Never commit PlatformIO build/test artifacts (`.pio/`, `build/`, `*.bin`, `*.elf`, `*.o`, `*.a`), coverage outputs (`*.gcda`, `*.gcno`, `coverage/`), or Python/IDE caches.
4. **Keep `specs/` in Sync:**
   - Include updated `SPEC.md`, `specs/*.md`, `README.md`, code, and regression tests together cleanly within their logical commit(s).

---

## 4. Two-Phase Turn Protocol (Bridging Directives #1 & #3)

1. **Turn Phase A — Proposal / Discussion (Read-Only):**
   - When a user prompt proposes a new feature or behavior change not yet confirmed in `specs/`, **do not modify any repository files**.
   - Inspect the codebase, present concrete `SPEC.md` / `specs/*.md` suggestions and regression test plans, and wait for explicit user confirmation. Because the working tree remains untouched, no git commit is created in Phase A.
2. **Turn Phase B — Confirmed Execution:**
   - Once confirmed, execute strictly in order: update `SPEC.md` + affected `specs/*.md` -> implement changes in `include/` / `src/` + regression tests in `test/` -> run `pio test -e native` (with `gcovr` when host toolchain is installed) and `pio run -e upesy_wroom` -> stage and create the git commit(s) -> verify `git status --short` before replying.
