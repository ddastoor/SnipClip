# SnipClip — Local Continuation Tracker & Engineering Roadmap (`todo.md`)

> **Canonical Task & Continuation Ledger**: Read this file alongside [`SPEC.md`](./SPEC.md) and [`AGENTS.md`](./AGENTS.md) at the start of every Google Antigravity session. Update checkbox states (`- [ ]` → `- [x]`) and Handoff Snapshot metadata whenever tasks are completed or reprioritized.

---

## 1. Handoff Snapshot ("Where We Left Off")

- **Snapshot Date**: `2026-11-25` (Completed Full `P0-1` → `P2-10` Roadmap + Audit Hardening on Local Linux Host)
- **Active Git Branch**: `main` (clean working tree; all `P0`, `P1`, and `P2` slices landed, verified, and synchronized)
- **Canonical Version**: `2.1.0` (`v2.1.0`)
- **In-Repo Synchronized `.deb` Artifact**: `public/releases/snipclip_2.1.0_amd64.deb` (`59,992` bytes, `installedSizeKb: 304`, `sha256: 87b2b61881d43014bc293e64d20b1a5fd43aad6836a507b9cd8b1bb424592eb4`, `builtAtIso: 2026-11-25T11:40:18Z`, containing `129,240`-byte stripped ELF64 `/usr/bin/snipclip` + `SHA256SUMS` + `src/data/releaseInfo.json`)
- **Published GitHub Release Baseline**: [`v2.1.0`](https://github.com/ddastoor/SnipClip/releases/tag/v2.1.0) (`debian:bookworm` container build capping ELF dynamic symbols at `<= GLIBC_2.36` for Debian 12 / Ubuntu 22.04+ cross-distro portability)

### Narrative of Recent Cloud & Local Engineering Work

1. **Cloud IDE Foundation (`v2.1.0` Handoff)**:
   - Configured `.agents/environment.json`, fixed the compact **`460×200` px** Microsoft Snipping Tool startup launcher (`SC_LAUNCHER_WIDTH=460`, `SC_LAUNCHER_HEIGHT=200`) with scrollable `annot_scroll` editor chrome, hardened 7-tier Wayland/X11 startup detection + `GdkDisplay` runtime reconciliation, added `--print-ui-metrics`, published `v2.1.0`, and authored ground-truth `SPEC.md`, `todo.md`, `AGENTS.md`, and `.env.example`.
2. **Local Continuation & Full `P0-1` → `P2-10` Execution (`2026-11-25`)**:
   - **Repo Hygiene & Canonical Packaging (`P0-1`, `P0-2`)**: Committed `100755` executable modes on `scripts/*.sh` and `scripts/generate_icons.py`, prefixed `package.json` script entries with `bash`, added `.env*` rules to `.gitignore` and `xvfb xauth` to `.agents/environment.json`, removed legacy AI Studio remnants (`public/assets/aistudio/image_53746529.png` and `GEMINI_API` capability in `metadata.json`), and updated all Debian/AppStream/copyright/changelog/UI metadata to canonical `https://github.com/ddastoor/SnipClip` (`io.github.ddastoor`).
   - **Capture Geometry, Overlay Window Mode, Persistence, Telemetry, HiDPI & Global Hotkeys (`P0-3`, `P1-4`–`P1-7`)**: Fixed half-pixel `sc_normalize_rect` / `normalizeRect` far-edge rounding clamps in C and TypeScript; enabled unconditional X11 window enumeration before overlay launch plus Wayland XDG Portal interactive window recapture (`on_window_recapture`); added pure-C11 `$XDG_CONFIG_HOME/snipclip/config.ini` atomic persistence (`native/src/config_store.c`); aligned Wayland `capture_driver` telemetry 1:1 with `sc_wayland_capture_plan()` (`wlroots+grim` priority, `xsel` PNG claim removed, `\u00XX` JSON escaping, `SIGPIPE` guard, `.png` auto-append + modal `GtkMessageDialog` file error dialogs); added HiDPI logical-to-physical coordinate scaling (`sc_scale_rect_to_pixbuf`), `GDK_FULLSCREEN_ON_ALL_MONITORS`, and multi-monitor root union capture; and implemented resident `--background` mode + system-wide hotkeys (`native/src/global_hotkey.c` via X11 `XGrabKey` behind an X11 `BadAccess` error trap and async Wayland `org.freedesktop.portal.GlobalShortcuts`).
   - **Automated GTK/Cairo & Vitest UI Suites + Web Workbench Parity (`P2-8`–`P2-10`)**: Added `native/tests/test_editor_gtk.c` (`309` headless GTK 3 / Cairo checks under `xvfb-run` covering all 10 tools, ARGB alpha-preserving freeform pixelate, 40-step undo eviction, ruler snap, and crop); added `tests/web-workbench.test.tsx` (`18` Vitest + React Testing Library UI tests across all `data-testid` hooks with zero `console.error` tolerance); and closed all Web Workbench parity gaps (`400%` max zoom + `Ctrl+Wheel`, wheel-over-ruler `±5°` rotation, PNG/JPEG export toggle, honest clipboard fallback feedback, verbatim `?raw` `release-deb.yml` preview, 16-file `debFileTree`, and live GitHub Releases links), then regenerated the 5-way synchronized `v2.1.0` `.deb` bundle.

### Completed Milestones Ledger

- [x] **Cloud workspace bootstrap & `xvfb`/`xauth` parity** (`.agents/environment.json:1-3`).
- [x] **Compact `460×200` Microsoft Snipping Tool startup launcher & scrollable editor toolbar** (`native/include/geometry.h`, `native/src/geometry.c`, `native/src/main.c`, `src/utils/snipEngine.ts`, `src/components/SnipStudio.tsx`).
- [x] **Hardened 7-tier X11 & Wayland startup auto-detection + 1:1 `sc_wayland_capture_plan` telemetry** (`native/src/display_detect.c`, `native/src/capture_backend.c`, `src/utils/snipEngine.ts`).
- [x] **Pure-C11 `$XDG_CONFIG_HOME/snipclip/config.ini` atomic preference store** (`native/include/config_store.h`, `native/src/config_store.c`, `native/src/main.c`).
- [x] **HiDPI coordinate scaling (`sc_scale_rect_to_pixbuf`), multi-monitor fullscreen overlay & Wayland window recapture** (`native/src/geometry.c`, `native/src/overlay.c`, `native/src/capture_backend.c`).
- [x] **System-wide global hotkeys (`XGrabKey` + `org.freedesktop.portal.GlobalShortcuts`) & `--background` resident mode** (`native/include/global_hotkey.h`, `native/src/global_hotkey.c`, `native/src/main.c`).
- [x] **Headless GTK 3 / Cairo canvas suite (`309` checks) & Vitest + Testing Library Web UI suite (`18` tests)** (`native/tests/test_editor_gtk.c`, `tests/setup.ts`, `tests/web-workbench.test.tsx`).
- [x] **Canonical `ddastoor/SnipClip` Debian/AppStream metadata, Web Workbench `400%` zoom + PNG/JPEG parity & 5-way `.deb` sync** (`scripts/build-deb.sh`, `src/components/*`, `public/releases/*`, `src/data/releaseInfo.json`).

### Current Build & Test Health Summary (6-Stage Unified Suite)

| Verification Layer | Command | Current Result | Details |
| :--- | :--- | :--- | :--- |
| **Full Unified Suite** | `npm test` / `./scripts/run-tests.sh` | **PASS (100% — 6 / 6 stages)** | Executes all 6 verification stages sequentially (`scripts/run-tests.sh:1-39`, tracked in Git as `100755` executable and invoked via `bash ./scripts/run-tests.sh` in `package.json:12`). |
| **Stage 1: Native C11 Unit Suite** | `make -C native test` | **PASS (7 suites / 139 `assert()` statements)** | Runs GTK/GLib-free `native/build/test_snipclip_native` (`native/tests/test_native.c:1-512`) across display cascade, `460×200` window sizing, HiDPI `sc_scale_rect_to_pixbuf`, `%f`/`file://` launcher paths, geometry/ruler (`sc_normalize_rect` clamp), Wayland driver priority + JSON escaping, and `config.ini` persistence. |
| **Stage 2: Headless GTK 3 / Cairo Canvas Suite** | `xvfb-run -a -s "-screen 0 1920x1080x24" make -C native test-gtk` | **PASS (7 suites / 309 checks)** | Runs `native/build/test_editor_gtk` (`native/tests/test_editor_gtk.c:1-487`) testing all 10 `ScToolKind` tools, opaque & ARGB alpha-preserving freeform `Pixelate`, 40-step (`SC_MAX_HISTORY`) undo/redo eviction, ruler edge snap, and crop flattening. |
| **Stage 3: Python ELF64 / `.deb` / `Xvfb` / `XGrabKey` / Metadata Suite** | `python3 -m unittest discover -s linux/tests -p "test_*.py" -v` | **PASS (10 / 10 tests)** | Verifies 64-bit ELF magic, zero `.py` in `.deb`, `ldd`/`GLIBC` symbols, CLI flags, Wayland/X11 socket detection, live `Xvfb` `460×200` GTK geometry, `xz` archive members, `t64 \| pre-t64` `Depends`, `md5sums`/maintainer scripts/AppStream/icons, canonical `ddastoor/SnipClip` metadata, and X11 `XGrabKey` `BadAccess` collision handling (`linux/tests/test_native_elf_and_deb.py:127-490`). |
| **Stage 4: TypeScript Engine & 5-Way Parity Suite** | `npx --yes tsx --test src/utils/snipEngine.test.ts` | **PASS (18 / 18 tests)** | Verifies launcher/editor sizing parity, Wayland/X11 capture & clipboard priority parity, `normalizeRect` rounding clamp, `PrintScreen` & custom shortcut parsing, geometry/ruler math, and 5-way `.deb` / `SHA256SUMS` / `releaseInfo.json` sync (`src/utils/snipEngine.test.ts:44-572`). |
| **Stage 5: Web Workbench Vitest + Testing Library UI Suite** | `npm run test:ui` (`vitest run`) | **PASS (18 / 18 tests)** | Exercises `tests/web-workbench.test.tsx:42-406` (`jsdom` + `tests/setup.ts` 2D canvas stub + strict zero-`console.error` guard) across all 9 core `data-testid` hooks, `400%` zoom + `Ctrl+Wheel`, wheel-over-ruler `±5°` rotation, PNG/JPEG export, honest clipboard fallback, `?raw` workflow YAML, and 16-file `.deb` tree. |
| **Stage 6: TypeScript Strict Typecheck & Production Web Build** | `npm run lint && npm run build` | **PASS (0 errors / 0 warnings)** | `tsc --noEmit` (`strict: true`) + `vite build` bundling React 19, Tailwind CSS v4, and verified `releaseInfo.json` into `dist/`. |

---

## 2. Immediate Post-Clone Local Bootstrap Checklist (Step 0)

All Step 0 local onboarding & verification steps have been completed on the local host (reference commands retained below for fresh machine clones):

- [x] **Step 0.1 — Install native Debian/Ubuntu C11, GTK 3, X11, packaging & headless `Xvfb` packages**:
  ```bash
  sudo apt-get update
  sudo apt-get install -y \
    build-essential pkg-config libgtk-3-dev libx11-dev \
    dpkg-dev binutils python3 xvfb xauth
  ```
  *(Completed: Native GTK 3, Cairo, GdkPixbuf, GIO/GDBus, libX11, `dpkg-deb`, and `xvfb-run` toolchains active and verified.)*
- [x] **Step 0.2 — Install recommended desktop runtime helpers (for live local X11/Wayland capture & clipboard persistence)**:
  ```bash
  sudo apt-get install -y \
    xdg-desktop-portal xdg-desktop-portal-gtk \
    wl-clipboard xclip grim desktop-file-utils shared-mime-info
  ```
  *(Completed: `xdg-desktop-portal`, `wl-clipboard`, `xclip`, and `grim` verified on host.)*
- [x] **Step 0.3 — Executable permissions on build & test scripts (`100755` committed in Git)**:
  ```bash
  git ls-files -s scripts/build-deb.sh scripts/run-tests.sh scripts/generate_icons.py
  ```
  *(Completed in Task `P0-1`: All three scripts now carry mode `100755` in Git index and `package.json` invokes `bash ./scripts/...`, so manual `chmod +x` is no longer needed on fresh clones.)*
- [x] **Step 0.4 — Verify Node.js 22+ and install npm packages**:
  ```bash
  node -v   # v22.x LTS
  npm install --no-package-lock
  ```
  *(Completed: Node.js v22 LTS and all runtime + Vitest/Testing Library `devDependencies` installed cleanly.)*
- [x] **Step 0.5 — Compile the native C11 GTK 3 binary and run C + GTK/Cairo unit suites**:
  ```bash
  make -C native clean all test
  xvfb-run -a -s "-screen 0 1920x1080x24" make -C native test-gtk
  ```
  *(Completed: Zero compiler warnings; `139` pure-C assertions and `309` headless GTK 3 / Cairo checks pass.)*
- [x] **Step 0.6 — Run the full 6-stage baseline verification suite & production web bundle**:
  ```bash
  npm test && npm run build
  ```
  *(Completed: All 6 stages in `./scripts/run-tests.sh` plus `vite build` pass with `0` errors.)*
- [x] **Step 0.7 — Optional `.env` setup (only if disabling Vite HMR)**:
  - SnipClip requires **zero** API keys, databases, Docker containers, or external cloud credentials.
  - `.gitignore` (`lines 17-19`) now ignores `.env` and `.env.*` while keeping `!.env.example` tracked.
- [x] **Step 0.8 — Local Google Antigravity `/swarm` & repository `AGENTS.md` setup**:
  - Repository-scoped rules in [`AGENTS.md`](./AGENTS.md) and global `/swarm` coordinator skills are active and verified.
- [x] **Step 0.9 — Choose the right `.deb` artifact when testing local package installation**:
  - **Cross-distro ABI note**: The in-repo `public/releases/snipclip_2.1.0_amd64.deb` (`59,992` B) was rebuilt on a `glibc 2.39` host (`GLIBC_2.38` symbol ceiling) and installs directly on Ubuntu 24.04+ / Debian 13+ / Zorin OS / Mint 22. For Debian 12 Bookworm (`glibc 2.36`) and Ubuntu 22.04 (`glibc 2.35`) release binaries, build inside `.github/workflows/release-deb.yml` (`debian:bookworm` container) or `docker run --rm -v "$PWD:/work" -w /work debian:bookworm bash ./scripts/build-deb.sh`.

---

## 3. Completed Resume Queue (Previously In-Flight Items)

- [x] **3.1 — Script Executable Bits & Shell-Safe `npm` / Python Test Invocations**
  - **Completed (`0ce535f`, `00b8ee7`)**: Committed `100755` executable modes on `scripts/build-deb.sh`, `scripts/run-tests.sh`, and `scripts/generate_icons.py`; prefixed `package.json` `"build:deb"` and `"test"` scripts with `bash `; and updated `linux/tests/test_native_elf_and_deb.py:110` to invoke `["bash", str(ROOT_DIR / "scripts" / "build-deb.sh"), cls.version]`.

- [x] **3.2 — Native GTK Custom Shortcut, Snip Mode & Delay Persistence on Disk**
  - **Completed (`c9e4c63`)**: Added GTK/GLib-free `native/include/config_store.h` and `native/src/config_store.c` persisting `[snipclip]` (`snip_mode`, `delay_seconds`, `custom_shortcut`) at `$XDG_CONFIG_HOME/snipclip/config.ini` (fallback `~/.config/snipclip/config.ini`) with `0700` parent directory creation and `0600` temp-file + `fsync` + `rename` atomic writes. `main.c` loads disk defaults prior to CLI parsing (so CLI flags override for one run without clobbering disk config) and saves silently on interactive GUI changes; verified by `test_config_persistence` in `native/tests/test_native.c`.

- [x] **3.3 — Switching to `"Window"` Mode Inside the Active X11 & Wayland Capture Overlay**
  - **Completed (`c9e4c63`)**: `perform_delayed_desktop_grab` (`native/src/main.c`) now calls `sc_enumerate_x11_windows` unconditionally whenever `app->session.protocol == SC_PROTO_X11` **before** mapping SnipClip's overlay window, so toggling into `Window` mode mid-capture on X11 immediately highlights and crops individual windows. On Wayland (`window_count == 0`), clicking `Window` on the overlay bar destroys the overlay and invokes `on_window_recapture` -> `sc_capture_wayland_window_interactive()` (XDG Desktop Portal window picker) instead of capturing the full desktop.

---

## 4. Resolved Bugs, Caveats & Codebase Audit Ledger

> **Codebase Comment Marker Audit**: `0` occurrences of `TODO`, `FIXME`, `HACK`, `XXX`, `BUG`, `WARN`, or `DEPRECATED` across source files. Every Phase 1 audit finding below has been fixed and covered by automated regression tests:

- [x] **4.1 — Non-executable Git script permissions (`100644`) broke `npm test` & `npm run build:deb`**:
  - **Resolved (`P0-1`, `0ce535f`, `00b8ee7`)**: All three scripts in `scripts/` are now `100755` in Git and `package.json` uses `bash ./scripts/...`. `npm test` and `npm run build:deb` exit `0` on fresh clones.
- [x] **4.2 — `.gitignore` omitted `.env` file patterns**:
  - **Resolved (`P0-1`, `0ce535f`)**: Added `.env`, `.env.*`, and `!.env.example` to `.gitignore:16-19`.
- [x] **4.3 — `.agents/environment.json` omitted `xvfb` and `xauth`**:
  - **Resolved (`P0-1`, `0ce535f`)**: Added `xvfb xauth` to `.agents/environment.json:2`, matching `.github/workflows/ci.yml`.
- [x] **4.4 — `sc_normalize_rect` / `normalizeRect` independent rounding could exceed `max_w` / `max_h` by 1 px**:
  - **Resolved (`P0-3`, `c9e4c63`, `00b8ee7`)**: Both `native/src/geometry.c` and `src/utils/snipEngine.ts:254-259` now clamp `width <= max_w - x` and `height <= max_h - y` after `llround`/`Math.round` (and `overlay.c` / `editor_canvas.c` guard `gdk_pixbuf_new_subpixbuf` returns); verified in `test_native.c` and `snipEngine.test.ts`.
- [x] **4.5 — Overlay `"Window"` mode button had empty window list when capture began in another mode**:
  - **Resolved (`P0-3`, `c9e4c63`)**: X11 windows are pre-enumerated on every X11 capture regardless of initial `snip_mode`, and Wayland overlays route `"Window"` clicks to `ScSnipWindowRecaptureFn` (`sc_capture_wayland_window_interactive`).
- [x] **4.6 — `xsel` advertised as PNG clipboard driver in telemetry but never executed**:
  - **Resolved (`P1-5`, `c9e4c63`, `00b8ee7`)**: Removed the misleading `xsel` PNG persistence branch in `native/src/display_detect.c` and `src/utils/snipEngine.ts:208` (reporting `GtkClipboard (X11)` / `GtkClipboard` unless `xclip` is installed), and added `signal(SIGPIPE, SIG_IGN)` in `pipe_png_to_daemon` (`capture_backend.c:525`).
- [x] **4.7 — Wayland `capture_driver` telemetry label diverged from actual execution order**:
  - **Resolved (`P1-5`, `c9e4c63`, `00b8ee7`)**: Added `sc_wayland_capture_plan()` (`display_detect.h:77-83`) shared 1:1 between `display_detect.c` telemetry, `capture_backend.c:347-368` runtime execution, and `snipEngine.ts:173-185` (`wlroots+grim` first on Sway/Hyprland/River/Wayfire; otherwise `XDG Portal -> grim -> spectacle -> gnome-screenshot -> GDK root`). Also hardened `sc_format_display_session_json` to `\u00XX`-escape all control characters and quotes.
- [x] **4.8 — Silent `GError` discard on image open/save failures & missing `.png` extension auto-append**:
  - **Resolved (`P1-5`, `c9e4c63`)**: `action_save_file` in `native/src/main.c` automatically appends `.png` when the chosen filename lacks `.png`/`.jpg`/`.jpeg`, and both file open and file save surface `GError` details safely (`"%s"`) via `app_show_message_dialog` (`GtkMessageDialog`) or `stderr` on CLI startup paths.
- [x] **4.9 — Wayland HiDPI coordinate scale mismatch & single-monitor overlay fullscreen**:
  - **Resolved (`P1-6`, `c9e4c63`)**: Added `sc_scale_rect_to_pixbuf()` (`geometry.c`), logical-to-physical Cairo surface scaling and crop mapping in `overlay.c`, `GDK_FULLSCREEN_ON_ALL_MONITORS` on overlay realization, and full multi-monitor geometry union in `capture_gdk_root_window` (`capture_backend.c:272-315`).
- [x] **4.10 — Legacy Google AI Studio template leftovers in repository**:
  - **Resolved (`P0-2`, `0ce535f`)**: Deleted unreferenced `public/assets/aistudio/image_53746529.png` and cleared `majorCapabilities` to `[]` in `metadata.json:5`.

---

## 5. Metadata Synchronization & Web Workbench Parity Status

### 5.1 Canonical Upstream Metadata & Live UI Previews (All Resolved)

- [x] **AppStream developer ID**: Updated `scripts/build-deb.sh:92-95` to `<developer id="io.github.ddastoor"><name>Darayus Dastoor</name></developer>`.
- [x] **Canonical GitHub repository URL (`https://github.com/ddastoor/SnipClip`)**: Updated across `scripts/build-deb.sh:119,132,175` and `src/components/DebReleaseSection.tsx:11,59`; enforced by `test_package_metadata_uses_canonical_ddastoor_snipclip_upstream` (`linux/tests/test_native_elf_and_deb.py:400-430`).
- [x] **Canonical maintainer identity (`Darayus Dastoor <ddastoor@users.noreply.github.com>`)**: Updated in `scripts/build-deb.sh:158,174` and `src/components/DebReleaseSection.tsx:58`.
- [x] **Zero-drift `.github/workflows/release-deb.yml` preview & 16-file `.deb` tree**: `src/components/DebReleaseSection.tsx:9,66,68-83` now imports `../../.github/workflows/release-deb.yml?raw` verbatim (typed via `src/vite-env.d.ts`) and lists all 16 packaged `/usr/...` files including `/usr/share/snipclip/snipclip.png`.
- [x] **Outbound GitHub Releases & Repository links**: Added verified `target="_blank" rel="noopener noreferrer"` links to `https://github.com/ddastoor/SnipClip/releases` (and repo root) in `src/components/Header.tsx:54-63`, `src/components/DebReleaseSection.tsx:120-129`, and `src/App.tsx:86-102`; verified in `tests/web-workbench.test.tsx:335-350`.

### 5.2 Web Workbench Browser Simulations & Resolved Parity Items

- **Intentional Browser Sandbox Simulations (By Design)**:
  - `src/App.tsx:19-25` & `src/components/DisplayDetectionPanel.tsx:15-111`: Initializes with `SESSION_PRESETS[0]` (`Debian 12 GNOME (Wayland)`) so browser visitors can interactively test `detectLinuxDisplaySession` across 5 Linux desktop presets without host OS socket access.
  - `src/utils/desktopScene.ts:15-40,115-279`: Renders a deterministic `1120×620` synthetic Debian 12 desktop (`SIMULATED_WINDOWS`) onto an HTML5 `<canvas>` so browser users can test `Rectangle`, `Window`, `Fullscreen`, and `Freeform` snips without OS screen-capture permissions.
- [x] **Removed dead `xprop` & `xwininfo` fields on `LinuxBinarySet`**: Cleaned up `src/utils/snipEngine.ts:63-71`, `src/components/DisplayDetectionPanel.tsx:25-110`, and `src/utils/snipEngine.test.ts` (`P2-10`, `00b8ee7`).
- [x] **Honest clipboard copy fallback in restricted browser contexts**: `handleCopySnip` (`src/components/SnipStudio.tsx:534-562`) and `copyCommand` (`src/components/DebReleaseSection.tsx:28-45`) now show `"Copied"` checkmarks **only** after `navigator.clipboard.write` / `writeText` resolves, surfacing clear manual-copy / `Save PNG/JPEG` fallback guidance when blocked (`P2-10`, `00b8ee7`; verified in `tests/web-workbench.test.tsx:202-236,374-405`).
- [x] **Web vs. Native `400%` zoom, wheel interactions & PNG/JPEG export parity**: Raised `MAX_EDITOR_ZOOM` to `4` (`400%`), added `Ctrl + Wheel` canvas zoom and wheel-over-ruler `±5°` rotation, sampled `Pixelate` export from pristine `baseCanvas`, exited `Crop` mode on new image load, and added a `PNG` / `JPEG` export format toggle (`src/components/SnipStudio.tsx:97-101,521-593`; verified in `tests/web-workbench.test.tsx:122-200,238-255`).

---

## 6. Completed Engineering Roadmap (`P0 Critical` → `P1 Next` → `P2 Polish & Scale`)

### `P0 Critical` — Repo Hygiene, Canonical Metadata & Capture Correctness

- [x] **Task P0-1: Fix Git executable script modes (`100755`), `package.json` `bash` invocations, `.gitignore` `.env*` rules, and `.agents/environment.json` `xvfb xauth` parity**
  - **Completion Note**: Committed `100755` modes on `scripts/build-deb.sh`, `scripts/run-tests.sh`, and `scripts/generate_icons.py`; updated `package.json` (`"build:deb"`, `"test"`) and `linux/tests/test_native_elf_and_deb.py` to invoke `bash`; added `.env`, `.env.*`, `!.env.example` to `.gitignore`; and added `xvfb xauth` to `.agents/environment.json` (`0ce535f`, `00b8ee7`).

- [x] **Task P0-2: Replace placeholder `snipclip/snipclip` & `maintainers@snipclip.org` metadata with `ddastoor/SnipClip` and remove legacy AI Studio artifacts**
  - **Completion Note**: Updated `scripts/build-deb.sh` and `src/components/DebReleaseSection.tsx` to `https://github.com/ddastoor/SnipClip`, `io.github.ddastoor`, and `Darayus Dastoor <ddastoor@users.noreply.github.com>`; deleted `public/assets/aistudio/image_53746529.png`; cleared `metadata.json` `majorCapabilities` to `[]`; regenerated synchronized `.deb` artifacts (`4e50592`); and enforced via `test_package_metadata_uses_canonical_ddastoor_snipclip_upstream` (`0ce535f`).

- [x] **Task P0-3: Fix `sc_normalize_rect` / `normalizeRect` `max_w + 1` rounding clamp and enable X11 window enumeration across all overlay modes**
  - **Completion Note**: Clamped post-rounding `width` / `height` to `max_w - x` / `max_h - y` in `native/src/geometry.c` and `src/utils/snipEngine.ts` with regression assertions in `test_native.c` and `snipEngine.test.ts`; enabled unconditional X11 window enumeration in `perform_delayed_desktop_grab` (`main.c`); added Wayland interactive portal window recapture (`ScSnipWindowRecaptureFn` in `overlay.h`/`overlay.c`); guarded `gdk_pixbuf_new_subpixbuf` `NULL` returns; and preserved alpha on ARGB freeform snips in `draw_pixelate_mosaic` (`c9e4c63`, `00b8ee7`).

### `P1 Next` — Persistence, Backend Telemetry Accuracy, Error UX, HiDPI & Global Hotkeys

- [x] **Task P1-4: Persist native GTK custom shortcut, default snip mode, and delay timer via `$XDG_CONFIG_HOME/snipclip/config.ini`**
  - **Completion Note**: Implemented pure-C11/POSIX `native/include/config_store.h` and `native/src/config_store.c` (`0700` dir creation + `0600` temp/fsync/rename atomic save), wired pre-CLI loading and interactive saving in `native/src/main.c` alongside `Ctrl++`/`Ctrl+Plus` parsing, reserved editor chord guards, `Escape` countdown/crop cancellation, and destroy timer cleanup; verified by `test_config_persistence` in `native/tests/test_native.c` (`c9e4c63`).

- [x] **Task P1-5: Align Wayland/X11 driver telemetry with `capture_backend.c` and surface GTK file I/O errors & `.png` default extension**
  - **Completion Note**: Shared `sc_wayland_capture_plan()` (`wlroots+grim -> portal -> grim -> spectacle -> gnome-screenshot -> GDK root`) across `display_detect.c`, `capture_backend.c`, and `snipEngine.ts`; dropped misleading `xsel` PNG claims; escaped control chars (`\u00XX`) in `--detect-display` JSON; ignored `SIGPIPE` on clipboard helper pipes; auto-appended `.png` on extensionless saves; and surfaced `GError` dialogs via `app_show_message_dialog` (`c9e4c63`, `00b8ee7`).

- [x] **Task P1-6: Add Wayland HiDPI scale-factor compensation & multi-monitor fullscreen overlay support**
  - **Completion Note**: Added `sc_scale_rect_to_pixbuf()` in `geometry.c` (tested at `1x`, `1.5x`, and `2x` in `test_native.c`), logical-to-physical Cairo rendering & crop scaling plus `GDK_FULLSCREEN_ON_ALL_MONITORS` in `overlay.c`, multi-monitor geometry union in `capture_gdk_root_window` (`capture_backend.c`), and compact `APP_CSS` toolbar metrics (`editor_min_w <= 560`) (`c9e4c63`).

- [x] **Task P1-7: Add global desktop hotkey listener (`XGrabKey` on X11 + `org.freedesktop.portal.GlobalShortcuts` on Wayland)**
  - **Completion Note**: Added `native/include/global_hotkey.h` and `native/src/global_hotkey.c` (`XGrabKey` across `0`, `Mod2Mask`, `LockMask`, `Mod2Mask|LockMask` behind a scoped X11 error trap + `gdk_window_add_filter` root filter on X11; non-blocking async GDBus `org.freedesktop.portal.GlobalShortcuts` with ref-counted `HkBox` weak pointer on Wayland), wired `--background` resident mode and live shortcut rebinding in `main.c`, and verified X11 grab-collision resilience in `test_x11_global_hotkey_registration_handles_grab_collision_cleanly` (`linux/tests/test_native_elf_and_deb.py:431-490`) (`0ce535f`, `c9e4c63`).

### `P2 Polish & Scale` — Expanded Automated Test Coverage & Web Workbench Parity

- [x] **Task P2-8: Add headless GTK 3 / Cairo C tests for `editor_canvas.c` and `overlay.c`**
  - **Completion Note**: Authored `native/tests/test_editor_gtk.c` (`309` pixel & state checks across all 10 `ScToolKind` tools, opaque + ARGB freeform `Pixelate`, eraser hit-testing, `SC_MAX_HISTORY=40` undo/redo eviction, ruler edge snap, and crop flattening), added `make -C native test-gtk`, and wired Stage 2 of `scripts/run-tests.sh` (`0ce535f`, `c9e4c63`).

- [x] **Task P2-9: Add automated React UI browser/DOM tests exercising all 9 `data-testid` hooks**
  - **Completion Note**: Configured `vitest` (`^5.1.1`), `@testing-library/react` (`^16.3.3`), `@testing-library/dom`, and `jsdom` in `package.json` + `vite.config.ts`, authored deterministic 2D canvas & pointer-capture polyfills with a strict zero-`console.error` guard in `tests/setup.ts`, and implemented `18` UI integration tests in `tests/web-workbench.test.tsx` (`npm run test:ui`, Stage 5 of `scripts/run-tests.sh`) (`0ce535f`, `00b8ee7`).

- [x] **Task P2-10: Sync Web Workbench parity (`400%` zoom, PNG/JPEG export, live GitHub Release link, dynamic workflow preview & `LinuxBinarySet` cleanup)**
  - **Completion Note**: Raised Web Workbench zoom ceiling to `400%` + `Ctrl+Wheel`, added wheel-over-ruler `±5°` rotation, added `PNG`/`JPEG` export format toggle, enforced honest clipboard copy feedback in `SnipStudio.tsx` and `DebReleaseSection.tsx`, imported `.github/workflows/release-deb.yml?raw` verbatim, listed all 16 packaged files in `debFileTree`, added outbound `https://github.com/ddastoor/SnipClip/releases` links across header/section/footer, and removed dead `xprop`/`xwininfo` fields from `LinuxBinarySet` (`00b8ee7`).

---

## 7. Architectural Decisions, Constraints & "Do Not Repeat" Log

> **Mandatory Guardrails for Agents**: Every constraint below exists because an alternative failed in real Debian/Ubuntu desktop environments or CI. Do **not** regress these invariants.

1. **100% Native Compiled C11 + GTK 3 ELF Binary — Zero Python in `.deb`**
   - **Rule**: `/usr/bin/snipclip` must be a compiled, stripped 64-bit C11 ELF binary (`ELFCLASS64`), and the staged `.deb` archive must contain **zero** `.py` files and **zero** `python3-*` runtime `Depends`. Python (`scripts/generate_icons.py`) is permitted strictly at build/test time.
   - **Why**: Earlier Python/PyQt6 packaging suffered broken dependencies and slow startup across minimal Debian/XFCE/Mint setups. Enforced by `test_usr_bin_snipclip_is_compiled_elf64_and_contains_zero_python` (`linux/tests/test_native_elf_and_deb.py:127-143`).

2. **Always Pass `-Zxz --uniform-compression --root-owner-group` to `dpkg-deb` (Never `zstd`)**
   - **Rule**: Keep `dpkg-deb -Zxz --uniform-compression --root-owner-group --build` in `scripts/build-deb.sh:249`.
   - **Why**: Ubuntu 22.04/24.04's `dpkg-deb` defaults to `zstd` (`control.tar.zst` / `data.tar.zst`), which causes Debian 12 Bookworm `dpkg`, KDE Discover, GNOME Software, and GDebi to reject or hang on the package. Enforced by `test_deb_archive_uses_xz_compression_for_cross_distro_gui_installers` (`linux/tests/test_native_elf_and_deb.py:314-327`).

3. **Preserve Dual `t64 | pre-t64` Shared Library `Depends` in `DEBIAN/control`**
   - **Rule**: Keep `libc6 (>= 2.35), libgtk-3-0t64 | libgtk-3-0, libglib2.0-0t64 | libglib2.0-0, libcairo2, libx11-6` (`scripts/build-deb.sh:30`).
   - **Why**: Ubuntu 24.04+ and Debian 13 renamed 64-bit `time_t` packages with a `t64` suffix (`libgtk-3-0t64`), while Debian 12 and Ubuntu 22.04 only provide `libgtk-3-0`. Dual `|` alternatives allow one `.deb` to install cleanly across both generations. Enforced by `linux/tests/test_native_elf_and_deb.py:328-338`.

4. **Compile Official Release `.deb` Packages on `debian:bookworm` (`GLIBC <= 2.36`), Never Ubuntu 24.04+**
   - **Rule**: Official distributable `.deb` binaries must be built inside `debian:bookworm` (`.github/workflows/release-deb.yml:33-34`) so ELF dynamic symbol versions stay `<= GLIBC_2.36`.
   - **Why**: Compiling on `glibc 2.39` hosts silently links C23 symbols (`__isoc23_strtol@GLIBC_2.38`), causing dynamic linker failures (`version 'GLIBC_2.38' not found`) on Debian 12 Bookworm and Ubuntu 22.04. Enforced on Bookworm by `linux/tests/test_native_elf_and_deb.py:159-173`.

5. **Close `stdin` (`</dev/null`) in Every `DEBIAN/postinst` & `DEBIAN/postrm` Command**
   - **Rule**: Every desktop/icon cache refresh command in `postinst` and `postrm` (`scripts/build-deb.sh:187-231`) must redirect `</dev/null >/dev/null 2>&1 || true`.
   - **Why**: GUI software centers (PackageKit, KDE Discover, GNOME Software) invoke maintainer scripts with open IPC pipes and hang indefinitely at 99% installation if child utilities inherit open `stdin`/`stdout` descriptors. Enforced by `linux/tests/test_native_elf_and_deb.py:353-359`.

6. **Never Regress the Compact `460×200` Startup Launcher or Unwrap `GtkScrolledWindow` on the Annotation Strip**
   - **Rule**: Keep `SC_LAUNCHER_WIDTH = 460` and `SC_LAUNCHER_HEIGHT = 200` (`native/include/geometry.h:87-88`, `src/utils/snipEngine.ts:95-96`), keep `min-width: 0; min-height: 0;` in `APP_CSS` (`native/src/main.c:72-88`), keep editor-only controls hidden via `hide_with_no_show_all` when no image is loaded, and keep the annotation buttons inside `app->annot_scroll` (`GtkScrolledWindow` with `GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER`).
   - **Why**: GTK 3 `GtkWindow` strictly enforces the sum of visible children's minimum widths. Packing all 26 capture, history, and annotation widgets into an unscrollable `GtkBox` forced the startup window to span ~1,855–1,920 px horizontally. Enforced under live `Xvfb` by `test_startup_window_matches_microsoft_snipping_tool_dimensions` (`linux/tests/test_native_elf_and_deb.py:279-313`).

7. **Maintain the 5-Artifact Version & Checksum Synchronization Invariant**
   - **Rule**: Never manually edit `VERSION`, `native/include/snipclip_version.h`, `public/releases/snipclip_<ver>_<arch>.deb`, `public/releases/SHA256SUMS`, or `src/data/releaseInfo.json` independently. Always regenerate them together via `bash ./scripts/build-deb.sh` (and keep `package.json:4` `"version"` aligned on version bumps).
   - **Why**: Both `linux/tests/test_native_elf_and_deb.py:393-399` and `src/utils/snipEngine.test.ts:525-572` verify exact version strings, `.deb` byte sizes, base64 `!<arch>\n` headers, and SHA-256 checksums across all 5 files.

8. **Probe Wayland/X11 Sockets via `stat()` + `S_ISSOCK()` Only — Never `connect()` Pre-`gtk_init`**
   - **Rule**: In `discover_wayland_socket_in_dir` and `discover_x11_socket` (`native/src/display_detect.c`), check socket existence with `stat()` + `S_ISSOCK(st.st_mode)` without calling `connect()`. Disambiguate stale Wayland sockets via explicit `DISPLAY` + `loginctl` and post-`gtk_init_check` `sc_reconcile_with_gdk_backend_name`.
   - **Why**: `test_binary_auto_detects_wayland_x11_and_stripped_gui_socket_sessions` (`linux/tests/test_native_elf_and_deb.py:238-278`) binds an `AF_UNIX` socket without calling `listen()`, where `connect()` fails with `ECONNREFUSED`/`EINVAL`; furthermore, opening and dropping raw client connections to live Wayland compositors before `gtk_init` causes noisy compositor disconnect logs.

9. **Keep `native/build/test_snipclip_native` Display-Server-Free (`-lm` Only)**
   - **Rule**: `native/Makefile:26-30,49-50` links `test_snipclip_native` (`test_native.c`, `display_detect.c`, `geometry.c`, `config_store.c`) with `-lm` only—zero GTK/GLib/X11 headers or libraries. All GTK/Cairo widget tests belong in `native/tests/test_editor_gtk.c` (`make -C native test-gtk`, guarded by `gtk_init_check` and executed under `xvfb-run`).
   - **Why**: `make -C native test` runs inside minimal `debian:bookworm` release containers (`.github/workflows/release-deb.yml:77`) where neither Xorg nor Wayland nor `Xvfb` is running.
