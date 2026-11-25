# SnipClip — Google Antigravity Agent Rules

SnipClip is a dual-stack repository containing:
1. **Native Linux Desktop Application (`native/`, `packaging/`, `scripts/`)**: A compiled C11 + GTK 3 + Cairo + libX11 + GDBus Windows 11 Snipping Tool clone (`/usr/bin/snipclip`) packaged as a cross-distro Debian installer (`public/releases/snipclip_<version>_<arch>.deb`) with automatic Wayland and X11 startup detection.
2. **Interactive Web Workbench & Verified Release Portal (`src/`, `public/`)**: A React 19 + TypeScript 5.8 + Tailwind CSS v4 + Vite 6 application simulating the SnipClip studio, X11/Wayland session-detection matrix, and SHA-256/magic-byte-verified `.deb` package downloads backed by `src/data/releaseInfo.json`.

Google Antigravity loads this root `AGENTS.md` automatically as `always_on` plain Markdown context. Keep line 1 as a Markdown heading (never add YAML `---` frontmatter here; only files under `.agents/rules/*.md` use YAML `trigger` frontmatter, and `.agents/rules/` is currently unused in this repository).

---

## 1. Context Bootstrapping Protocol

At the start of **every** cold-started session—before editing code, running builds, or proposing architectural changes—read these two ground-truth documents at the repository root:

1. **`SPEC.md` (Authoritative System Specification)**:
   - Consult `SPEC.md` first for as-is system architecture, annotated directory/file ownership, C11 and React/TypeScript module breakdowns, CLI flags, `--detect-display` and `--print-ui-metrics` JSON schemas, `src/data/releaseInfo.json` schema, feature verification status, and Cloud-to-Local environment deltas.
2. **`todo.md` (Active Continuation Tracker & Roadmap)**:
   - Consult `todo.md` for the latest handoff snapshot, Step 0 post-clone bootstrap checks, in-flight tasks, bug/caveat triage, prioritized `P0 -> P1 -> P2` engineering backlog, and the **"Do Not Repeat"** architectural constraints log.

### Mandatory Documentation Synchronization Rule
Whenever a change modifies features, UI behavior, C/TS shared logic, CLI flags, JSON contracts, Debian packaging metadata, environment variables, or task progress, you **must** update both `SPEC.md` and `todo.md` **in the same turn / pull request** before declaring work complete. Never leave `SPEC.md` or `todo.md` lagging behind the code.

### Workspace Environment Metadata (`.agents/environment.json` & `.env.example`)
- **`.agents/environment.json`**: Automated Google Antigravity Cloud VM bootstrap hook. It installs Debian C/GTK/X11 build packages (`build-essential`, `pkg-config`, `libgtk-3-dev`, `libx11-dev`, `dpkg-dev`, `binutils`), runs `npm ci` (if `package-lock.json` exists) or `npm install --no-package-lock`, and compiles `make -C native all`. On local Linux machines or fresh CI/VM setups, also install `xvfb` and `xauth` (see command below) so headless GTK 3 window-sizing tests execute instead of skipping.
- **`.env.example`**: Documents all build, Vite (`DISABLE_HMR`), and Linux display-detection environment variables (`XDG_SESSION_TYPE`, `WAYLAND_DISPLAY`, `DISPLAY`, `XDG_RUNTIME_DIR`, `XDG_CURRENT_DESKTOP`, `DESKTOP_SESSION`, `XDG_SESSION_ID`, `GDK_BACKEND`). SnipClip requires **zero API keys, databases, or external secrets**. Note: `.gitignore` does not ignore `.env*` files until the tracked `todo.md` hardening item lands—never write real credentials into `.env*`.

---

## 2. Canonical Workspace Commands

Run all commands from the repository root (`SnipClip/`).

### System & Node Dependency Setup
```bash
# Install Debian/Ubuntu C11, GTK 3, X11, .deb packaging, and headless Xvfb test dependencies
sudo apt-get update && sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  build-essential pkg-config libgtk-3-dev libx11-dev dpkg-dev binutils xvfb xauth

# Install Node.js dependencies (repo currently has no package-lock.json; use npm ci if lockfile is added)
npm install --no-package-lock
```

### Native Build, Synchronized `.deb` Packaging & Web Build
```bash
# Compile stripped C11/GTK 3 binary (native/build/snipclip) + C test runner (native/build/test_snipclip_native)
make -C native clean all

# Rebuild native binary, run C tests, and atomically regenerate .deb + SHA256SUMS + src/data/releaseInfo.json
bash ./scripts/build-deb.sh
# Or bump/pin an explicit semantic version across VERSION, C header, .deb, SHA256SUMS, and releaseInfo.json:
bash ./scripts/build-deb.sh <semver>

# Start local Vite dev server on http://localhost:3000 (bound to 0.0.0.0:3000)
npm run dev

# Typecheck React/TypeScript sources (runs tsc --noEmit with strict: true)
npm run lint

# Build production static web bundle into dist/
npm run build
```

> **Why `bash ./scripts/...` instead of `npm test` / `npm run build:deb`**: `scripts/run-tests.sh` and `scripts/build-deb.sh` are currently tracked in Git with file mode `100644` rather than `100755`. On a fresh `git clone`, `npm test` (`./scripts/run-tests.sh`) and `npm run build:deb` (`./scripts/build-deb.sh`) fail with `Permission denied` until executable bits (`100755`) are committed. Always invoke `bash ./scripts/run-tests.sh` and `bash ./scripts/build-deb.sh` (or `chmod +x scripts/*.sh` first).

### Full & Targeted Test Commands
```bash
# Run the full 4-stage verification suite (C unit -> Python ELF/.deb -> TS parity -> tsc --noEmit)
bash ./scripts/run-tests.sh

# 1. Native C11 unit tests only (headless; tests display_detect.c & geometry.c)
make -C native test

# 2. Live GTK 3 460x200 startup window & compact editor geometry check under Xvfb
xvfb-run -a -s "-screen 0 1920x1080x24" ./native/build/snipclip --print-ui-metrics

# 3. Python compiled ELF64 & multi-distro .deb integration tests (all 8 tests)
python3 -m unittest discover -s linux/tests -p "test_*.py" -v

# 3b. Single Python integration test method
python3 -m unittest linux.tests.test_native_elf_and_deb.NativeElfAndDebPackageTests.<test_name>

# 4. TypeScript engine, shortcut, geometry, and 5-way release parity tests
npx tsx --test src/utils/snipEngine.test.ts

# 4b. Single filtered TypeScript test by name
npx tsx --test --test-name-pattern="<pattern>" src/utils/snipEngine.test.ts
```

---

## 3. Codebase Architecture & Non-Negotiable Invariants

### Directory Ownership Boundaries
- **`native/`**: Pure C11 native Linux desktop app (`Makefile`, headers in `native/include/`, sources in `native/src/`, headless unit suite in `native/tests/test_native.c`, gitignored build outputs in `native/build/`).
  - `main.c`: CLI parser, GTK 3 compact launcher/editor window lifecycle, shortcut recorder, CSS styling, and `--print-ui-metrics`.
  - `display_detect.c`: Startup Wayland vs. X11 detection cascade (`XDG_SESSION_TYPE`, `WAYLAND_DISPLAY`, UNIX sockets in `XDG_RUNTIME_DIR` / `/tmp/.X11-unix`, `loginctl`, `DISPLAY`), `GDK_BACKEND` bootstrap, post-`gtk_init` reconciliation, and `%f`/`%u`/`file://` launcher argument normalization.
  - `geometry.c`: Pure math for rectangle normalization, freeform polygon bounds/hit-testing, arrow wings, graduated ruler fitting/snapping, stroke hit-testing, and `sc_compute_window_size`.
  - `capture_backend.c`: GDBus `org.freedesktop.portal.Screenshot`, CLI fallbacks (`grim`, `gnome-screenshot`, `spectacle`), X11 root-window capture, `_NET_CLIENT_LIST_STACKING` window enumeration, and clipboard persistence (`GtkClipboard` + `wl-copy` / `xclip`).
  - `overlay.c` & `editor_canvas.c`: Fullscreen interactive capture overlay and Cairo zoomable annotation/ruler/redact/pixelate/crop canvas with undo/redo stacks.
- **`packaging/`**: Static Debian desktop assets (`snipclip.desktop`, `snipclip.menu`, `snipclip.svg`).
- **`scripts/`**: `build-deb.sh` (master C build + `.deb` packager + metadata synchronizer), `generate_icons.py` (build-time pure-Python PNG rasterizer for `hicolor` `16x16`–`256x256` and `pixmaps`), and `run-tests.sh`.
- **`linux/tests/`**: `test_native_elf_and_deb.py` verifying ELF64 headers, `ldd`/`GLIBC_` linkage, CLI flags, Wayland/X11 socket probing, Xvfb GTK 3 window metrics, `xz` archive members, `DEBIAN/control`, `md5sums`, `postinst`/`postrm`, and AppStream XML.
- **`src/`**: React 19 web workbench (`App.tsx`, `components/Header.tsx`, `components/SnipStudio.tsx`, `components/DisplayDetectionPanel.tsx`, `components/DebReleaseSection.tsx`), shared pure TypeScript mirror (`utils/snipEngine.ts` + `utils/snipEngine.test.ts`), verified `.deb` downloader (`utils/debDownload.ts`), synthetic desktop canvas (`utils/desktopScene.ts`), and auto-generated `data/releaseInfo.json`.
- **`public/releases/`**: Committed `snipclip_<ver>_<arch>.deb` binary package and `SHA256SUMS` manifest served by Vite.
- **`.github/workflows/`**: `ci.yml` (Ubuntu + Node 22 + Xvfb full test/build gate) and `release-deb.yml` (`debian:bookworm` container build + GitHub Releases publisher).

### Invariant 1: 5-Way Release Artifact Parity
The following five artifacts **must** remain 100% synchronized at all times:
1. `VERSION`
2. `native/include/snipclip_version.h` (`SNIPCLIP_VERSION`, `SNIPCLIP_DEB_FILENAME`, `SNIPCLIP_RELEASE_TAG`)
3. `native/build/snipclip` (compiled ELF64 binary installed into `/usr/bin/snipclip` inside the `.deb`)
4. `public/releases/snipclip_<ver>_<arch>.deb` and `public/releases/SHA256SUMS`
5. `src/data/releaseInfo.json` (`version`, `debFilename`, `sha256`, `sizeBytes`, `debBase64`)

**Rule**: Whenever `VERSION` or **any** file under `native/`, `packaging/`, or `scripts/generate_icons.py` changes, run `bash ./scripts/build-deb.sh` before running tests or committing. Never hand-edit `native/include/snipclip_version.h`, `public/releases/SHA256SUMS`, or `src/data/releaseInfo.json`; both `src/utils/snipEngine.test.ts` and `linux/tests/test_native_elf_and_deb.py` enforce exact version, byte-size, and SHA-256 equality across all five locations.

### Invariant 2: Compact Microsoft Snipping Tool Window Sizing (`460 × 200`)
- On launch with no image loaded, the native GTK 3 window **must** open as a compact toolbar frame of exactly **`460 × 200 px`** (`SC_LAUNCHER_WIDTH × SC_LAUNCHER_HEIGHT` in `native/include/geometry.h`, mirrored as `LAUNCHER_WINDOW_WIDTH` / `LAUNCHER_WINDOW_HEIGHT` in `src/utils/snipEngine.ts`).
- Live GTK preferred minimum widths reported by `./native/build/snipclip --print-ui-metrics` **must** satisfy **`launcher_min_w <= 460`** and **`editor_min_w <= 600`**.
- **Mandatory GTK layout rules (`native/src/main.c`)**:
  - Keep the annotation tool strip inside `GtkScrolledWindow` (`app->annot_scroll`) with `GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER`—never pack all editor tool buttons into an unwrapped top-level horizontal `GtkBox`.
  - Preserve `min-width: 0; min-height: 0;` in `APP_CSS` for toolbar buttons and combo boxes.
  - Guard all editor-only widgets (`editor_actions_box`, `annot_scroll`, `crop_banner`, `status_bar`) with `hide_with_no_show_all()` (`gtk_widget_set_no_show_all(..., TRUE)`). Never call an unguarded `gtk_widget_show_all(app->window)` that exposes hidden editor rows on startup.

### Invariant 3: Cross-Distro `.deb` Packaging Rules
- **Pure Compiled ELF64 Payload**: `/usr/bin/snipclip` inside the `.deb` must be a stripped 64-bit ELF binary (`\x7fELF`). Ship **zero** `.py` files, **zero** `/usr/lib/snipclip` directories, and **zero** Python runtime dependencies in `DEBIAN/control` (`scripts/generate_icons.py` is strictly a build-time host script).
- **Mandatory `xz` Archive Compression**: Always build `.deb` packages with `dpkg-deb -Zxz --uniform-compression --root-owner-group`. **Never** omit `-Zxz` on Ubuntu 24.04+, which defaults to `zstd` and breaks Debian 12, KDE Discover, GNOME Software, and GDebi.
- **Dual `t64` / Pre-`t64` Dependency Compatibility**: Preserve dual-alternative `Depends` in `scripts/build-deb.sh` (`libc6 (>= 2.35), libgtk-3-0t64 | libgtk-3-0, libglib2.0-0t64 | libglib2.0-0, libcairo2, libx11-6`) so one `.deb` installs cleanly on both Debian 12 / Ubuntu 22.04 / Mint 21 and Ubuntu 24.04+ / Debian 13 / Mint 22.
- **PackageKit-Safe Maintainer Scripts**: Every command in `DEBIAN/postinst` and `DEBIAN/postrm` must redirect `</dev/null >/dev/null 2>&1 || true` so GUI package installers never hang on open descriptors.
- **ABI Baseline Awareness**: Official GitHub Release `.deb` binaries are compiled inside `debian:bookworm` (`glibc <= 2.36`) via `.github/workflows/release-deb.yml`. Local rebuilds on Ubuntu 24.04 link against host `glibc 2.39`; use the `debian:bookworm` container workflow when producing portable cross-distro release binaries.

### Invariant 4: C11 & TypeScript Coding Standards
- **C11 (`native/`)**: Must compile with **zero warnings** under `-std=c11 -O2 -Wall -Wextra -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2`. Keep pure geometry and display-session logic GTK-free and deterministic in `geometry.c` and `display_detect.c` (using injectable callbacks `sc_detect_display_session_custom`) so `make -C native test` runs in headless environments. Always use bounded buffers (`snprintf`), pair every GLib/GDK/Cairo/X11 allocation with its destructor, and never pass unescaped strings to printf/GTK markup formatters.
- **TypeScript & React (`src/`)**: Maintain `strict: true` (`tsconfig.json`) with **zero `any` types** and clean `npm run lint` (`tsc --noEmit`). Mirror any change to C display detection, window sizing, shortcuts, or annotation math in `src/utils/snipEngine.ts` and `src/utils/snipEngine.test.ts`. Preserve `isValidDebianArchiveBytes` (`!<arch>\n` + `debian-binary` header verification) in `src/utils/debDownload.ts`.

---

## 4. Execution & Verification Loop

Follow this exact six-step workflow for every task before marking it complete:

1. **Explore**: Read `SPEC.md`, `todo.md`, and the exact C, shell, Python, or TypeScript source files involved—never rely on assumptions or stale `README.md` prose.
2. **Plan**: Map all affected files, cross-stack C/TypeScript parity touchpoints, packaging implications, and verification tests.
3. **Implement**: Apply minimal, high-precision edits honoring C11 `-Wall -Wextra`, TypeScript `strict: true`, compact `460×200` GTK geometry, and `.deb` packaging invariants.
4. **Rebuild `.deb` (Conditional Mandatory Step)**: If `VERSION` or any file in `native/`, `packaging/`, or `scripts/` was modified, run `bash ./scripts/build-deb.sh` to re-sync the binary, `.deb`, `SHA256SUMS`, and `src/data/releaseInfo.json`.
5. **Verify**: Run all verification gates and confirm zero errors or warnings:
   - `make -C native test`
   - `xvfb-run -a -s "-screen 0 1920x1080x24" ./native/build/snipclip --print-ui-metrics`
   - `python3 -m unittest discover -s linux/tests -p "test_*.py" -v`
   - `npx tsx --test src/utils/snipEngine.test.ts`
   - `npm run lint`
   - `npm run build`
6. **Sync Docs**: Update `todo.md` (check off completed items, log new findings/tasks) and `SPEC.md` (update architecture, contracts, feature matrix, or baseline counts) in the same turn.
