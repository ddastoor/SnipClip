# SnipClip — Local Continuation Tracker & Engineering Roadmap (`todo.md`)

> **Canonical Task & Continuation Ledger**: Read this file alongside [`SPEC.md`](./SPEC.md) and [`AGENTS.md`](./AGENTS.md) at the start of every local Google Antigravity session. Update checkbox states (`- [ ]` → `- [x]`) and Handoff Snapshot metadata whenever tasks are completed or reprioritized.

---

## 1. Handoff Snapshot ("Where We Left Off")

- **Handoff Date**: `2026-11-24` (Cloud-to-Local Workspace Handoff from Google Antigravity Cloud IDE, Ubuntu 24.04 container)
- **Active Git Branch**: `main` (synced 1:1 with `origin/main`; zero unmerged local/remote branches; zero `git stash` entries)
- **Latest Merged Commit on `main`**: `fccf817abb700bf9b03f814f4aa5a73be40c1eaf`
- **Latest Published Release**: [`v2.1.0`](https://github.com/ddastoor/SnipClip/releases/tag/v2.1.0) — ships natively compiled `snipclip_2.1.0_amd64.deb` (`49,296` bytes, `xz`-compressed, `debian:bookworm` `glibc <= 2.36` ABI baseline) + `SHA256SUMS`

### Narrative of Recent Cloud IDE Work

In the Google Antigravity Cloud IDE sessions leading up to this handoff, we:
1. Configured reproducible cloud workspace provisioning in `.agents/environment.json` so fresh Linux containers install native GTK 3 / X11 / Debian packaging headers, npm dependencies, and build the C11 binary automatically.
2. Resolved a critical UI layout regression where packing all 26 capture, history, and annotation controls into an unscrollable GTK 3 toolbar stretched the startup window across the full monitor width (~1,855–1,920 px). SnipClip now launches as a compact **`460×200` px** Microsoft Snipping Tool bar (`SC_LAUNCHER_WIDTH=460`, `SC_LAUNCHER_HEIGHT=200`), hides editor-only chrome until an image is captured or opened, dynamically resizes to fit the captured snip (`560×380` min up to 80% monitor workarea), and wraps the annotation strip inside a horizontally scrollable `GtkScrolledWindow`.
3. Hardened zero-CLI GUI startup X11/Wayland auto-detection (`native/src/display_detect.c`) to overwrite empty-string environment variables, query `loginctl` when `XDG_SESSION_TYPE` is `tty`/`unspecified`, prioritize explicit X11 `DISPLAY` over unreferenced stale `wayland-0` sockets, and reconcile session metadata against the live `GdkDisplay` runtime backend (`GdkX11Display` / `GdkWaylandDisplay`) after `gtk_init_check()`.
4. Added `--print-ui-metrics` to the native binary and an automated headless `Xvfb` GTK 3 integration test verifying compact `460×200` startup geometry in CI, fixed lockfile-independent CI workflow steps and `build-deb.sh` branch-ref version resolution, bumped the project to **`2.1.0` (`v2.1.0`)**, verified publication of `snipclip_2.1.0_amd64.deb` on GitHub Releases, and generated the complete Cloud-to-Local handoff specification (`SPEC.md`, `todo.md`, `AGENTS.md`, `.env.example`).

### Completed Cloud IDE Milestones

- [x] **Cloud workspace bootstrap config** (`.agents/environment.json:1-3`) provisioning `build-essential`, `pkg-config`, `libgtk-3-dev`, `libx11-dev`, `dpkg-dev`, `binutils`, npm packages, and `make -C native all` (PR #1, merged as `fccf817`).
- [x] **Compact `460×200` Microsoft Snipping Tool startup launcher & scrollable editor toolbar** across `native/include/geometry.h:81-86`, `native/src/geometry.c:245-286`, `native/src/main.c:55-75,122-154,762-766,802-987,1058-1072`, `native/src/editor_canvas.c:548-578,713-740`, `src/utils/snipEngine.ts:97-142`, and `src/components/SnipStudio.tsx:335-346,1509-1775` (PR #2, merged as `5631403`).
- [x] **Hardened X11 & Wayland startup auto-detection** in `native/src/display_detect.c`: empty-env overwrite in `sc_bootstrap_display_env` (`native/src/display_detect.c:388-414`), `loginctl` session `Type` fallback for `tty`/`unspecified`/blank `XDG_SESSION_TYPE` (`native/src/display_detect.c:146-196,371-376`), explicit `DISPLAY` priority over stale Wayland sockets (`native/src/display_detect.c:310-314`), and post-`gtk_init_check` `GdkX11Display` / `GdkWaylandDisplay` runtime reconciliation via `sc_reconcile_with_gdk_backend_name` (`native/src/display_detect.c:416-432`, `native/src/main.c:1188-1191`).
- [x] **Live `Xvfb` GTK 3 geometry integration test**: added `--print-ui-metrics` (`native/src/main.c:1130-1131,1214-1246`) and `test_startup_window_matches_microsoft_snipping_tool_dimensions` (`linux/tests/test_native_elf_and_deb.py:207-236`) asserting `launcher_default_w == 460`, `launcher_default_h == 200`, `launcher_min_w <= 460`, and `editor_min_w <= 600`.
- [x] **CI & packaging script hardening**: added `xvfb` + `xauth` and lockfile-optional `npm install` fallback in `.github/workflows/ci.yml:29-30,38-43`; added strict semver regex guard on `GITHUB_REF_NAME` in `scripts/build-deb.sh:7-9,17-20` so non-tag branch runs (`GITHUB_REF_NAME=main`) fall back cleanly to `VERSION`.
- [x] **Release `v2.1.0` synchronization & publication**: synced `2.1.0` across `VERSION`, `package.json:4`, `native/include/snipclip_version.h:6-9`, `public/releases/snipclip_2.1.0_amd64.deb`, `public/releases/SHA256SUMS`, and `src/data/releaseInfo.json`, and verified live assets on [`v2.1.0` GitHub Release](https://github.com/ddastoor/SnipClip/releases/tag/v2.1.0).
- [x] **Cloud-to-Local handoff deliverables**: authored ground-truth `SPEC.md`, `todo.md`, `AGENTS.md`, and `.env.example` in the repository root.

### Current Build & Test Health Summary

| Verification Layer | Command | Current Result | Details |
| :--- | :--- | :--- | :--- |
| **Full Unified Suite** | `bash ./scripts/run-tests.sh` | **PASS (100%)** | Executes all 4 verification stages below sequentially (`scripts/run-tests.sh:7-23`). |
| **1. Native C11 Unit Suite** | `make -C native test` | **PASS (4 suites / 54 assertions)** | Runs `native/build/test_snipclip_native` (`native/tests/test_native.c:46-239`): display cascade, window sizing math, `%f`/`file://` path normalization, geometry & ruler snap. |
| **2. ELF / `.deb` / `Xvfb` Suite** | `python3 -m unittest discover -s linux/tests -p "test_*.py" -v` | **PASS (8 / 8 tests)** | Verifies 64-bit ELF magic, zero `.py` in `.deb`, `ldd`/`GLIBC` symbols, CLI flags, Wayland/X11 socket detection, live `Xvfb` `460×200` GTK geometry, `xz` archive members, `t64 \| pre-t64` `Depends`, `md5sums`, `</dev/null` `postinst`/`postrm`, AppStream XML, icons, and SHA-256 parity (`linux/tests/test_native_elf_and_deb.py:56-323`). |
| **3. TypeScript Parity Suite** | `npx --yes tsx --test src/utils/snipEngine.test.ts` | **PASS (17 / 17 tests)** | Verifies launcher/editor sizing parity, Wayland/X11 detection parity, `PrintScreen` & custom shortcut parsing, geometry/ruler math, and `.deb` `!<arch>\n` / `releaseInfo.json` sync. |
| **4. TypeScript Typecheck** | `npm run lint` (`tsc --noEmit`) | **PASS (0 errors)** | Strict TypeScript ~5.8.2 verification across `src/` and `vite.config.ts`. |
| **5. Production Web Build** | `npm run build` (`vite build`) | **PASS (0 errors)** | Bundles React 19 + Tailwind CSS v4 Web Workbench & `.deb` portal into `dist/`. |
| **Known `npm test` Caveat** | `npm test` (`./scripts/run-tests.sh`) | **EXIT `126` on fresh clone until `chmod +x`** | `scripts/run-tests.sh` and `scripts/build-deb.sh` are currently tracked as `100644` in Git (`package.json:7,12`). Invoke via `bash ./scripts/run-tests.sh` or run `chmod +x` (Step 0.3 below; permanent fix in Task `P0-1`). |

---

## 2. Immediate Post-Clone Local Bootstrap Checklist (Step 0)

Run these steps once on your local Debian/Ubuntu machine immediately after `git clone https://github.com/ddastoor/SnipClip.git && cd SnipClip`:

- [ ] **Step 0.1 — Install native Debian/Ubuntu C11, GTK 3, X11, packaging & headless `Xvfb` packages**:
  ```bash
  sudo apt-get update
  sudo apt-get install -y \
    build-essential pkg-config libgtk-3-dev libx11-dev \
    dpkg-dev binutils python3 xvfb xauth
  ```
- [ ] **Step 0.2 — Install recommended desktop runtime helpers (for live local X11/Wayland capture & clipboard persistence)**:
  ```bash
  sudo apt-get install -y \
    xdg-desktop-portal xdg-desktop-portal-gtk \
    wl-clipboard xclip grim desktop-file-utils shared-mime-info
  ```
  *(On KDE Plasma, also install `xdg-desktop-portal-kde`; on GNOME, install `xdg-desktop-portal-gnome`.)*
- [ ] **Step 0.3 — Restore executable permissions on build & test scripts** (currently `100644` in Git index until Task `P0-1` lands):
  ```bash
  chmod +x scripts/build-deb.sh scripts/run-tests.sh scripts/generate_icons.py
  ```
- [ ] **Step 0.4 — Verify Node.js 22+ and install npm packages**:
  ```bash
  node -v   # Recommended: v22.x LTS (matches .github/workflows/ci.yml:35)
  npm install
  ```
  *(Note: `package-lock.json` is intentionally not tracked in Git; use `npm install`, not `npm ci`.)*
- [ ] **Step 0.5 — Compile the native C11 GTK 3 binary and run C unit tests**:
  ```bash
  make -C native clean all test
  ```
  *(Outputs stripped ELF binary `native/build/snipclip` and test runner `native/build/test_snipclip_native`.)*
- [ ] **Step 0.6 — Run the full baseline verification suite & production web bundle**:
  ```bash
  bash ./scripts/run-tests.sh && npm run build
  ```
  *(Confirm 4 C suites, 8 Python ELF/`.deb`/`Xvfb` tests including `test_startup_window_matches_microsoft_snipping_tool_dimensions`, 17 TypeScript tests, `tsc --noEmit`, and `vite build` all pass with 0 errors.)*
- [ ] **Step 0.7 — Optional `.env` setup (only if disabling Vite HMR)**:
  - SnipClip requires **zero** API keys, databases, Docker containers, or external cloud credentials.
  - Only run `cp .env.example .env` if you want to set `DISABLE_HMR=true` (`vite.config.ts:17`). Until Task `P0-1` adds `.env*` to `.gitignore`, keep `.env` untracked when staging commits.
  - Note on local dev server binding: `npm run dev` (`package.json:8`, `vite.config.ts:14-15`) binds `0.0.0.0:3000` (originally configured for Cloud IDE port forwarding). Open `http://localhost:3000` locally, or run `npx vite --port=3000 --host=127.0.0.1` on shared networks.
- [ ] **Step 0.8 — Optional local Google Antigravity global `/swarm` skill setup**:
  - In the Cloud IDE VM, user-global Antigravity rules lived outside the repository at `~/.gemini/GEMINI.md` and `~/.gemini/skills/swarm/SKILL.md` and therefore do **not** transfer via `git clone`.
  - Repository-scoped rules in [`AGENTS.md`](./AGENTS.md) load automatically on local startup; recreate `~/.gemini/skills/swarm/SKILL.md` locally only if you want universal background `/swarm` subagent orchestration across all local projects.
- [ ] **Step 0.9 — Choose the right `.deb` artifact when testing local package installation**:
  - **Recommended cross-distro test**: On Ubuntu 24.04+ / Debian 13+, install the in-repo package via `sudo apt install ./public/releases/snipclip_2.1.0_amd64.deb` (`48,908` B, compiled on Ubuntu 24.04 with `GLIBC_2.38` symbol ceiling); on Debian 12 Bookworm / Ubuntu 22.04 (`glibc <= 2.36`), download the `debian:bookworm`-compiled `snipclip_2.1.0_amd64.deb` (`49,296` B, `GLIBC <= 2.36`) from [`v2.1.0` GitHub Releases](https://github.com/ddastoor/SnipClip/releases/tag/v2.1.0) (or rebuild locally via `bash ./scripts/build-deb.sh`).
  - **Local `build-deb.sh` caveat**: Running `bash ./scripts/build-deb.sh` on Ubuntu 24.04+ (`glibc 2.39`) links `native/build/snipclip` against host `GLIBC_2.38`/`2.39` symbols and rewrites `public/releases/snipclip_2.1.0_amd64.deb`, `public/releases/SHA256SUMS`, and `src/data/releaseInfo.json`. That locally built `.deb` works on your host OS, **but must not be committed as a cross-distro release artifact** because Debian 12 Bookworm (`glibc 2.36`) and Ubuntu 22.04 (`glibc 2.35`) will reject `GLIBC_2.38+` symbols. Use `.github/workflows/release-deb.yml` (`debian:bookworm` container) or `docker run --rm -v "$PWD:/work" -w /work debian:bookworm` for distributable `.deb` builds.

---

## 3. In-Flight / Partially Completed Work (Immediate Resume Queue)

> **Git Working Tree State**: Clean — zero unmerged branches, zero stashes, and zero half-written uncommitted code changes remain from the Cloud IDE. The three items below are complete in one layer of the stack and ready to finish wiring across the remaining layer.

- [ ] **3.1 — Script Executable Bits & Shell-Safe `npm` / Python Test Invocations**
  - **What is already done**:
    - `scripts/build-deb.sh:1`, `scripts/run-tests.sh:1`, and `scripts/generate_icons.py:1` all start with portable `#!/usr/bin/env bash` / `#!/usr/bin/env python3` shebangs.
    - `.github/workflows/ci.yml:47` and `.github/workflows/release-deb.yml:72` explicitly run `chmod +x` in CI before calling the scripts.
  - **What remains**:
    - Commit `100755` executable file modes in Git via `git update-index --chmod=+x scripts/build-deb.sh scripts/run-tests.sh scripts/generate_icons.py`.
    - Prefix `package.json:7` (`"build:deb": "bash ./scripts/build-deb.sh"`) and `package.json:12` (`"test": "bash ./scripts/run-tests.sh"`) with `bash`, and update `linux/tests/test_native_elf_and_deb.py:38-42` to invoke `["bash", str(ROOT_DIR / "scripts" / "build-deb.sh"), cls.version]`.
  - **Subtleties / Gotchas**:
    - `linux/tests/test_native_elf_and_deb.py:37-42` only triggers `scripts/build-deb.sh` when `native/build/snipclip` or `public/releases/snipclip_<ver>_<arch>.deb` is absent; testing after `make -C native clean` without `bash` or `+x` raises `PermissionError: [Errno 13] Permission denied`.

- [ ] **3.2 — Native GTK Custom Shortcut, Snip Mode & Delay Persistence on Disk**
  - **What is already done**:
    - Web Workbench (`src/components/SnipStudio.tsx:249-263,431-445`) persists `snipclip.printScreenEnabled` and `snipclip.customShortcut` in `window.localStorage`.
    - Native GTK app (`native/src/main.c:50-52,188-233,546-639,691-695,1144-1148,1198-1212`) implements full accelerator parsing (`parse_shortcut_accelerator`), interactive modal key recording (`on_shortcut_btn_clicked` / `on_record_shortcut_key`), `--shortcut KEYS` CLI override, and live capture triggering.
  - **What remains**:
    - `ScApp` fields `custom_keyval`, `custom_mods`, `custom_shortcut_label`, `snip_mode`, and `delay_seconds` (`native/src/main.c:44-52`) exist only in process memory and reset to `Ctrl+Shift+S` / `rectangle` / `0s` on exit.
    - Add GLib `GKeyFile` load/save at `$XDG_CONFIG_HOME/snipclip/config.ini` (`g_get_user_config_dir()`), loading defaults prior to CLI flag parsing in `main()` (`native/src/main.c:1109-1153`) and saving when the user updates settings in the GUI (`on_shortcut_btn_clicked` at `native/src/main.c:619-637`, `on_mode_changed` at `native/src/main.c:360-367`, `on_delay_changed` at `native/src/main.c:369-375`).
  - **Subtleties / Gotchas**:
    - Explicit CLI flags (`--mode`, `--delay`, `--shortcut`) should override `config.ini` for that invocation without overwriting `config.ini` on disk unless changed interactively.
    - Missing or unwritable `$HOME` / `$XDG_CONFIG_HOME` (such as `HOME=/tmp/...` in `linux/tests/test_native_elf_and_deb.py:174`) must degrade silently without warnings on `stdout` that would corrupt JSON output from `--detect-display` or `--print-ui-metrics`.

- [ ] **3.3 — Switching to `"Window"` Mode Inside the Active X11 Capture Overlay**
  - **What is already done**:
    - `sc_enumerate_x11_windows` (`native/src/capture_backend.c:337-427`) enumerates visible top-level X11 client windows in reverse `_NET_CLIENT_LIST_STACKING` Z-order using `libX11`.
    - `overlay.c` (`native/src/overlay.c:97-111,139-165,186-202,286-307,429-444`) renders the floating Windows 11 mode switcher bar (`Rectangle`, `Window`, `Fullscreen`, `Freeform`) and highlights/crops hovered windows from `ctx->windows[0..window_count-1]`.
  - **What remains**:
    - `perform_delayed_desktop_grab` (`native/src/main.c:292-296`) only calls `sc_enumerate_x11_windows` when `app->snip_mode == SC_MODE_WINDOW` **at capture start**. Starting a snip in `Rectangle` or `Freeform` mode passes `win_count = 0` into `sc_launch_capture_overlay` (`native/src/main.c:298-308`), so clicking `"Window"` on the floating overlay bar (`native/src/overlay.c:97-111`) leaves `ctx->window_count == 0`—no window hover borders appear and clicking anywhere falls back to capturing the entire desktop (`native/src/overlay.c:162-164`).
  - **Subtleties / Gotchas**:
    - Enumerate X11 windows **before** mapping SnipClip's fullscreen overlay window in `perform_delayed_desktop_grab` (`if (app->session.protocol == SC_PROTO_X11)` regardless of initial `app->snip_mode` at `native/src/main.c:294`). Enumerating lazily *after* `ctx->window` is mapped on X11 places SnipClip's own fullscreen overlay at the top of `_NET_CLIENT_LIST_STACKING`, occluding every underlying desktop window unless its XID is explicitly filtered out.

---

## 4. Bugs, Failing Tests & Codebase `TODO`/`FIXME` Triage

> **Codebase Comment Marker Audit**: Exhaustive search across `native/`, `src/`, `scripts/`, `linux/`, `packaging/`, and `.github/` confirmed **`0` occurrences** of `TODO`, `FIXME`, `HACK`, `XXX`, `BUG`, `WARN`, or `DEPRECATED` markers in source comments. All items below were uncovered via direct static & runtime audit in Phase 1.

- [ ] **4.1 — Non-executable Git script permissions (`100644`) break `npm test` & `npm run build:deb`** (`package.json:7,12`, `scripts/run-tests.sh:1`, `scripts/build-deb.sh:1`, `scripts/generate_icons.py:1`, `linux/tests/test_native_elf_and_deb.py:39`):
  - Fresh clones have mode `0644` on `scripts/*.sh`, causing `npm test` and `npm run build:deb` to fail with `/bin/sh: 1: ./scripts/run-tests.sh: Permission denied` (exit code `126`). Resolve in Task `P0-1`.
- [ ] **4.2 — `.gitignore` omits `.env` file patterns** (`.gitignore:1-14`):
  - `.gitignore` excludes `node_modules/`, `dist/`, `native/build/`, `__pycache__/`, `.DS_Store`, and `*.swp`, but lacks `.env`, `.env.*`, and `!.env.example`. Resolve in Task `P0-1`.
- [ ] **4.3 — `.agents/environment.json` omits `xvfb` and `xauth`** (`.agents/environment.json:2` vs `.github/workflows/ci.yml:29-30`):
  - Without `xvfb` and `xauth` in `.agents/environment.json`, `test_startup_window_matches_microsoft_snipping_tool_dimensions` (`linux/tests/test_native_elf_and_deb.py:209-211`) hits `self.skipTest("xvfb-run not installed in minimal container")` in freshly provisioned cloud environments instead of enforcing the `460×200` GTK geometry invariant. Resolve in Task `P0-1`.
- [ ] **4.4 — `sc_normalize_rect` / `normalizeRect` independent rounding can exceed `max_w` / `max_h` by 1 px** (`native/src/geometry.c:37-40`, `src/utils/snipEngine.ts:250-253`):
  - Because `rx = (int)llround(min_x)` and `rw = (int)llround(max_x - min_x)` are rounded independently after clamping `min_x` and `max_x` to `[0, max_w]`, inputs like `min_x = 99.5, max_x = 100.0, max_w = 100` produce `rx = 100, rw = 1` (`rx + rw = 101 > 100`). Passing `r` into `gdk_pixbuf_new_subpixbuf` (`native/src/overlay.c:153,229` and `native/src/editor_canvas.c:886`) triggers a GLib `src_x + width <= src_pixbuf->width` assertion failure and returns `NULL`. Clamp `rw` to `max_w - rx` and `rh` to `max_h - ry` (or round clamped endpoints `x1`/`x2` prior to subtraction) in both C and TypeScript. Resolve in Task `P0-3`.
- [ ] **4.5 — Overlay `"Window"` mode button has empty window list when capture began in another mode** (`native/src/main.c:294`, `native/src/overlay.c:97-111,139-165`):
  - Toggling into `SC_MODE_WINDOW` from the overlay bar on X11 fails to highlight or crop individual windows because `sc_enumerate_x11_windows` was skipped at `main.c:294`. On Wayland, toggling into `SC_MODE_WINDOW` inside `overlay.c` also cannot invoke the interactive XDG Portal window picker (`capture_via_xdg_portal_gdbus(true)` only runs before overlay launch in `main.c:264-270`). Resolve in Task `P0-3`.
- [ ] **4.6 — `xsel` advertised as PNG clipboard driver in telemetry but never executed** (`native/src/display_detect.c:236-238`, `src/utils/snipEngine.ts:207-209` vs `native/src/capture_backend.c:482-489`):
  - `populate_protocol_drivers` reports `"GtkClipboard + xsel"` when `xsel` is present without `xclip`, yet `sc_copy_pixbuf_to_clipboard` only spawns `xclip` (`capture_backend.c:483-488`). Because `xsel` truncates binary `image/png` streams on NUL bytes, remove the misleading `xsel` PNG persistence claim in `display_detect.c:236-238` and `snipEngine.ts:207-209` (reporting `"GtkClipboard (X11)"` unless `xclip` is installed). Resolve in Task `P1-5`.
- [ ] **4.7 — Wayland `capture_driver` telemetry label diverges from actual execution order** (`native/src/display_detect.c:205-219` vs `native/src/capture_backend.c:290-296`):
  - `populate_protocol_drivers` checks `has_bin_fn("dbus-daemon") || has_bin_fn("gdbus")` first (`display_detect.c:205`), labeling almost every Wayland desktop (including `sway`/`Hyprland` with `grim` installed) as `"Native GDBus XDG Portal (org.freedesktop.portal.Screenshot)"`. At runtime, `sc_capture_desktop_pixbuf` (`capture_backend.c:291-295`) runs `capture_wayland_fallbacks()` (`grim` → `spectacle` → `gnome-screenshot`) **before** `capture_via_xdg_portal_gdbus(false)` for full-desktop captures. Align telemetry and/or Desktop-Env-aware priority so reported `capture_driver` matches the driver actually invoked. Resolve in Task `P1-5`.
- [ ] **4.8 — Silent `GError` discard on image open/save failures & missing `.png` extension auto-append** (`native/src/main.c:482-496,528,1051`):
  - `action_save_file` (`native/src/main.c:493-495`) frees `err` without notifying the user if `gdk_pixbuf_save` fails (e.g., read-only target directory or full disk), and saving an extensionless filename like `capture` writes PNG bytes without appending `.png`.
  - `action_open_file` (`native/src/main.c:528`) and CLI `initial_image_path` (`native/src/main.c:1051`) pass `NULL` for `GError **` to `gdk_pixbuf_new_from_file`, failing silently on corrupt or unreadable files. Resolve in Task `P1-5`.
- [ ] **4.9 — Wayland HiDPI coordinate scale mismatch & single-monitor overlay fullscreen** (`native/src/overlay.c:275,407-415`, `native/src/capture_backend.c:261-273`):
  - On 2x / fractional HiDPI Wayland sessions, `grim` and XDG Desktop Portal return physical-pixel `GdkPixbuf` buffers while `GtkDrawingArea` Cairo drawing (`overlay.c:275`) and `GdkEventButton`/`GdkEventMotion` coordinates (`overlay.c:168-171,220-221`) use logical GDK pixels, causing zoomed-in overlay rendering and offset crop rectangles unless scaled by `gdk_window_get_scale_factor`.
  - `gtk_window_fullscreen` (`overlay.c:415`) restricts the capture overlay to a single monitor on multi-head X11 setups instead of setting `GDK_FULLSCREEN_ON_ALL_MONITORS`. Resolve in Task `P1-6`.
- [ ] **4.10 — Legacy Google AI Studio template leftovers in repository** (`public/assets/aistudio/image_53746529.png`, `metadata.json:5-7`, `.env.example:10-13`):
  - `public/assets/aistudio/image_53746529.png` (~450 KB) is completely unreferenced in code and copied into `dist/` on every Vite build.
  - `metadata.json:5-7` lists `"MAJOR_CAPABILITY_SERVER_SIDE_GEMINI_API"` despite SnipClip having zero Gemini or server-side AI dependencies (note: the legacy `GEMINI_API_KEY=` stub previously in `.env.example` has already been removed as part of this handoff; see `.env.example:10-13`). Resolve the remaining two files in Task `P0-2`.

---

## 5. Stubbed, Mocked, or Placeholder Functionality

### 5.1 Actionable Metadata Placeholders & Drifted UI Previews (Must Replace for Production)

- [ ] **AppStream developer ID placeholder**: `scripts/build-deb.sh:92` hardcodes `<developer id="io.github.snipclip">` inside generated `usr/share/metainfo/snipclip.metainfo.xml` instead of `io.github.ddastoor`.
- [ ] **Placeholder GitHub repository URL (`https://github.com/snipclip/snipclip`)**: hardcoded in `scripts/build-deb.sh:119` (AppStream `<url type="homepage">`), `scripts/build-deb.sh:132` (`usr/share/doc/snipclip/copyright` `Source:`), `scripts/build-deb.sh:175` (`DEBIAN/control` `Homepage:`), and `src/components/DebReleaseSection.tsx:45` (`controlPreview`). Canonical URL is `https://github.com/ddastoor/SnipClip`.
- [ ] **Placeholder maintainer email (`maintainers@snipclip.org`)**: hardcoded in `scripts/build-deb.sh:158` (`changelog.Debian.gz`), `scripts/build-deb.sh:174` (`DEBIAN/control` `Maintainer:`), and `src/components/DebReleaseSection.tsx:44` (`controlPreview`).
- [ ] **Drifted `.github/workflows/release-deb.yml` code preview tab**: `src/components/DebReleaseSection.tsx:51-72` (`workflowPreview`) renders an abbreviated static YAML snippet that differs from `.github/workflows/release-deb.yml:1-108` (missing `paths` triggers, `workflow_dispatch`, `steps.meta` version resolution, `make -C native test` + Python `.deb` verification step, and `actions/upload-artifact@v4`).
- [ ] **Missing live GitHub Releases outbound link in Web UI**: `src/components/Header.tsx:44-49` (`href="#github-release"`), `src/components/DebReleaseSection.tsx:101-120`, and `src/App.tsx:77-88` only scroll within the SPA or download the embedded `.deb`; none provide an `<a href="https://github.com/ddastoor/SnipClip/releases">` link to the live GitHub Releases page.

### 5.2 Intentional Web Workbench Browser Mocks & Parity Gaps

- [ ] **Simulated Linux environment & helper binary state** (`src/App.tsx:19-25`, `src/components/DisplayDetectionPanel.tsx:15-121`): Intentional browser sandbox simulation — initializes with `SESSION_PRESETS[0]` (`Debian 12 GNOME (Wayland)`) so browser visitors can test `detectLinuxDisplaySession` across 5 desktop presets without host OS socket access.
- [ ] **Simulated Debian 12 desktop & 3 mock windows** (`src/utils/desktopScene.ts:15-40,115-262`): Intentional browser sandbox simulation — `renderSimulatedDebianDesktop` draws a `1120×620` synthetic Debian desktop (`SIMULATED_WINDOWS`) onto an HTML5 `<canvas>` so browser users can test `Rectangle`, `Window`, `Fullscreen`, and `Freeform` snips without OS capture permissions.
- [ ] **Unread `xprop` & `xwininfo` fields on `LinuxBinarySet`** (`src/utils/snipEngine.ts:71-72`, `src/components/DisplayDetectionPanel.tsx:33-34,54-55,75-76,96-97,117-118`): Legacy fields from before native X11 window enumeration moved to direct `libX11` (`_NET_CLIENT_LIST_STACKING`); never rendered as UI checkboxes (`DisplayDetectionPanel.tsx:268-276`) and never read in `detectLinuxDisplaySession` (`snipEngine.ts:145-237`). Remove in Task `P2-10`.
- [ ] **Optimistic clipboard copy checkmark in restricted browser frames** (`src/components/SnipStudio.tsx:500-517`, `src/components/DebReleaseSection.tsx:23-31`): Both `handleCopySnip` (`SnipStudio.tsx:511-512`) and `copyCommand` (`DebReleaseSection.tsx:29-30`) trigger visual `"Copied"` checkmarks even when `navigator.clipboard` rejects in non-secure/iframe contexts (`SnipStudio.tsx:513-517` adjusts the status message to `"Prepared PNG buffer..."`, while `DebReleaseSection.tsx:26-28` swallows the error silently). Surface honest fallback feedback in Task `P2-10`.
- [ ] **Web vs. Native zoom & export format parity gap** (`src/components/SnipStudio.tsx:520-531,1951` vs `native/src/editor_canvas.c:809`, `native/src/main.c:482-487`): Web Workbench caps zoom at `300%` (`Math.min(3, ...)`) and exports PNG only, whereas native GTK supports `400%` zoom (`4.0`) and both PNG and JPEG export. Align in Task `P2-10`.

---

## 6. Prioritized Engineering Roadmap (`P0 Critical` → `P1 Next` → `P2 Polish & Scale`)

### `P0 Critical` — Repo Hygiene, Canonical Metadata & Capture Correctness

- [ ] **Task P0-1: Fix Git executable script modes (`100755`), `package.json` `bash` invocations, `.gitignore` `.env*` rules, and `.agents/environment.json` `xvfb xauth` parity**
  - **Objective**: Ensure `npm test` and `npm run build:deb` succeed out-of-the-box on any fresh clone without manual `chmod +x`, protect local `.env*` files from accidental commits, and install `xvfb` + `xauth` in `.agents/environment.json` so live GTK 3 geometry tests never skip in cloud containers.
  - **Target Files**:
    - `scripts/build-deb.sh` (git mode `100644` → `100755`)
    - `scripts/run-tests.sh` (git mode `100644` → `100755`)
    - `scripts/generate_icons.py` (git mode `100644` → `100755`)
    - `package.json` (lines `7`, `12`: prefix `./scripts/...` with `bash `)
    - `linux/tests/test_native_elf_and_deb.py` (line `39`: invoke `["bash", str(ROOT_DIR / "scripts" / "build-deb.sh"), cls.version]`)
    - `.gitignore` (append `.env`, `.env.*`, `!.env.example`)
    - `.agents/environment.json` (line `2`: add `xvfb xauth` to `apt-get install`)
  - **Dependencies**: None.
  - **Verification Criteria**:
    - `git ls-files -s scripts/build-deb.sh scripts/run-tests.sh scripts/generate_icons.py` reports mode `100755` for all three files.
    - `npm test` exits `0` directly (running all 4 suites including `test_startup_window_matches_microsoft_snipping_tool_dimensions` without skipping).
    - Creating a dummy `.env` file confirms `git check-ignore -v .env` matches `.gitignore` while `git check-ignore -v .env.example` exits `1` (not ignored); remove dummy `.env` afterward.

- [ ] **Task P0-2: Replace placeholder `snipclip/snipclip` & `maintainers@snipclip.org` metadata with `ddastoor/SnipClip` and remove legacy AI Studio artifacts**
  - **Objective**: Update all Debian control, AppStream MetaInfo, copyright, changelog, and Web UI preview metadata to canonical `https://github.com/ddastoor/SnipClip` / `io.github.ddastoor` identity, delete the unused 450 KB `public/assets/aistudio/image_53746529.png` asset, and remove the unused `MAJOR_CAPABILITY_SERVER_SIDE_GEMINI_API` declaration from `metadata.json`.
  - **Target Files**:
    - `scripts/build-deb.sh` (lines `92, 119, 132, 158, 174, 175`)
    - `src/components/DebReleaseSection.tsx` (lines `44-45`)
    - `metadata.json` (lines `5-7`: clear `majorCapabilities` to `[]`)
    - `public/assets/aistudio/image_53746529.png` (delete file)
    - `linux/tests/test_native_elf_and_deb.py` (add assertion checking `https://github.com/ddastoor/SnipClip` in extracted `DEBIAN/control` and `snipclip.metainfo.xml`)
    - Regenerate synchronized `.deb` artifacts (`public/releases/snipclip_2.1.0_amd64.deb`, `public/releases/SHA256SUMS`, `src/data/releaseInfo.json`) inside `debian:bookworm` (or during next version bump) so embedded `DEBIAN/control` matches `scripts/build-deb.sh`.
  - **Dependencies**: Task `P0-1` (for `npm run build:deb` / Docker `debian:bookworm` rebuild).
  - **Verification Criteria**:
    - `rg -i "snipclip/snipclip|maintainers@snipclip\.org|GEMINI_API" scripts/ src/ metadata.json` returns `0` matches.
    - `test ! -e public/assets/aistudio/image_53746529.png` exits `0`.
    - `npm test && npm run build` exits `0` and `dist/assets/aistudio` no longer exists.

- [ ] **Task P0-3: Fix `sc_normalize_rect` / `normalizeRect` `max_w + 1` rounding clamp and enable X11 window enumeration across all overlay modes**
  - **Objective**: Prevent `rx + rw > max_w` / `ry + rh > max_h` off-by-one `GdkPixbuf` subpixbuf assertion failures on half-pixel boundary drags, and populate X11 window bounds unconditionally before launching the capture overlay so switching from `Rectangle`/`Freeform` to `Window` mode mid-capture highlights and crops individual X11 windows cleanly (while disabling/hiding unavailable `Window` toggle on Wayland overlays when `window_count == 0` or triggering interactive portal re-capture).
  - **Target Files**:
    - `native/src/geometry.c` (lines `22-44`: clamp `rw` to `max_w - rx` and `rh` to `max_h - ry` when `max_w > 0` / `max_h > 0`)
    - `src/utils/snipEngine.ts` (lines `239-255`: mirror identical endpoint/width clamp in `normalizeRect`)
    - `native/tests/test_native.c` (add boundary rounding regression assertions in `test_geometry_and_ruler_math`)
    - `src/utils/snipEngine.test.ts` (add matching boundary rounding test)
    - `native/src/main.c` (lines `292-296`: call `sc_enumerate_x11_windows` whenever `app->session.protocol == SC_PROTO_X11` regardless of initial `app->snip_mode`)
    - `native/src/overlay.c` (lines `97-111, 435-443`: support `Window` mode hover immediately using pre-enumerated `ctx->windows` on X11; on Wayland where `window_count == 0`, re-dispatch interactive portal window capture via callback or disable unsupported in-overlay window hover)
  - **Dependencies**: Task `P0-1`.
  - **Verification Criteria**:
    - `make -C native clean all test` passes new `sc_normalize_rect(99.5, 99.5, 100.0, 100.0, 100, 100)` assertions (`x + width <= 100`, `y + height <= 100`).
    - `npx --yes tsx --test src/utils/snipEngine.test.ts` passes matching TypeScript `normalizeRect` boundary assertions.
    - `npm test` exits `0`.

### `P1 Next` — Persistence, Backend Telemetry Accuracy, Error UX, HiDPI & Global Hotkeys

- [ ] **Task P1-4: Persist native GTK custom shortcut, default snip mode, and delay timer via `$XDG_CONFIG_HOME/snipclip/config.ini`**
  - **Objective**: Save and restore user-customized capture accelerator (`custom_shortcut`), last-selected `snip_mode`, and `delay_seconds` across native application restarts using GLib `GKeyFile`, matching Web Workbench `localStorage` persistence while preserving CLI flag overrides.
  - **Target Files**:
    - `native/include/config_store.h` (new header declaring `ScUserConfig`, `sc_config_load_from_path`, `sc_config_save_to_path`, `sc_config_default_path`)
    - `native/src/config_store.c` (new module using `GKeyFile` / pure C INI parser with atomic write via `g_file_set_contents` and `0700` directory creation on `$XDG_CONFIG_HOME/snipclip`)
    - `native/src/main.c` (load config before CLI loop at lines `1109-1153`; save config in `on_mode_changed`, `on_delay_changed`, and `on_shortcut_btn_clicked`)
    - `native/Makefile` (add `src/config_store.c` to `APP_SRCS` and `TEST_SRCS`)
    - `native/tests/test_native.c` (add unit tests verifying round-trip save/load, malformed file resilience, and read-only path fallback)
  - **Dependencies**: Task `P0-1`.
  - **Verification Criteria**:
    - `make -C native clean all test` passes new `test_config_persistence` C unit tests using a temporary directory.
    - Running `XDG_CONFIG_HOME=/tmp/sc_cfg_test ./native/build/snipclip --print-ui-metrics` and `--detect-display` outputs valid JSON and exits `0` even when `/tmp/sc_cfg_test` does not pre-exist.
    - `npm test` exits `0`.

- [ ] **Task P1-5: Align Wayland/X11 driver telemetry with `capture_backend.c` and surface GTK file I/O errors & `.png` default extension**
  - **Objective**: Eliminate telemetry drift between `display_detect.c` / `snipEngine.ts` and `capture_backend.c` (removing unimplemented `xsel` PNG claims and matching Wayland capture driver priority to actual runtime helper vs. XDG Portal execution order), append `.png` when saving extensionless filenames, and present modal `GtkMessageDialog` errors on failed image open/save operations.
  - **Target Files**:
    - `native/src/display_detect.c` (lines `205-240`: align Wayland `capture_driver` selection order with `capture_backend.c` or desktop-specific portal/grim order; remove `xsel` branch at lines `236-238`)
    - `native/src/capture_backend.c` (lines `284-296, 482-489`: keep runtime driver order 1:1 with `display_detect.c`)
    - `src/utils/snipEngine.ts` (lines `171-210`: keep TypeScript `detectLinuxDisplaySession` parity with `display_detect.c`)
    - `native/src/main.c` (lines `478-499`: append `.png` if filename lacks `.png`/`.jpg`/`.jpeg` suffix and display `GtkMessageDialog` with `err->message` on `gdk_pixbuf_save` error; lines `526-536, 1050-1056`: capture `GError *err` from `gdk_pixbuf_new_from_file` and report failure via dialog/stderr)
    - `native/tests/test_native.c` & `src/utils/snipEngine.test.ts` (update/add driver parity unit tests)
  - **Dependencies**: Task `P0-1`.
  - **Verification Criteria**:
    - `make -C native clean all test` and `npx --yes tsx --test src/utils/snipEngine.test.ts` pass with 100% C ↔ TypeScript telemetry parity.
    - `./native/build/snipclip /nonexistent/missing.png` under `xvfb-run` logs/surfaces a clean error without crashing.
    - `npm test` exits `0`.

- [ ] **Task P1-6: Add Wayland HiDPI scale-factor compensation & multi-monitor fullscreen overlay support**
  - **Objective**: Ensure crisp 1:1 overlay rendering and pixel-accurate `Rectangle`, `Window`, and `Freeform` crops on 2x / fractional HiDPI monitors, and span all connected monitors on multi-head X11 setups.
  - **Target Files**:
    - `native/src/overlay.c` (lines `139-165, 223-265, 270-362, 407-416`: compute buffer-to-logical scale ratio `scale_x = (double)pixbuf_w / alloc_w`, `scale_y = (double)pixbuf_h / alloc_h`, apply `cairo_scale` / `cairo_surface_set_device_scale` during `on_overlay_draw`, map logical event coordinates to physical `desktop_pixbuf` pixels on crop, and set `gdk_window_set_fullscreen_mode(gdk_win, GDK_FULLSCREEN_ON_ALL_MONITORS)` on realization)
    - `native/src/capture_backend.c` (lines `261-273`: query full `GdkScreen` / combined monitor geometry for root window capture origin & dimensions)
    - `native/include/geometry.h` & `native/src/geometry.c` (optional pure helper `sc_scale_rect_to_pixbuf` for unit-testable coordinate scaling)
    - `native/tests/test_native.c` (unit tests for 1x, 1.5x, and 2x logical-to-physical crop coordinate mapping)
  - **Dependencies**: Task `P0-3`.
  - **Verification Criteria**:
    - `make -C native clean all test` passes 1x / 1.5x / 2x HiDPI coordinate scaling assertions.
    - `xvfb-run -a -s "-screen 0 3840x2160x24" ./native/build/snipclip --print-ui-metrics` exits `0` with compact launcher dimensions preserved.
    - `npm test` exits `0`.

- [ ] **Task P1-7: Add global desktop hotkey listener (`XGrabKey` on X11 + `org.freedesktop.portal.GlobalShortcuts` on Wayland)**
  - **Objective**: Allow `PrintScreen` and the user's custom shortcut (`Ctrl+Shift+S`) to trigger SnipClip capture even when the SnipClip GTK window is unfocused or minimized in the background.
  - **Target Files**:
    - `native/include/global_hotkey.h` (new header for session-aware global hotkey registration/unregistration)
    - `native/src/global_hotkey.c` (new module implementing `XGrabKey` + `gdk_window_add_filter` root event filter with NumLock/CapsLock `Mod2Mask`/`LockMask` combinations on X11, and GDBus `org.freedesktop.portal.GlobalShortcuts` session binding on Wayland with graceful fallback when unsupported by the compositor)
    - `native/src/main.c` (register global shortcuts after `build_main_window`, re-bind when user records a new shortcut in `on_shortcut_btn_clicked`, and add `--background` / minimized resident flag)
    - `native/Makefile` (add `src/global_hotkey.c` to `APP_SRCS`)
    - `linux/tests/test_native_elf_and_deb.py` (add `Xvfb` test verifying X11 `XGrabKey` registration does not crash when key is already grabbed by another WM client)
  - **Dependencies**: Task `P1-4`.
  - **Verification Criteria**:
    - Under `xvfb-run`, launching `snipclip` registers X11 `XGrabKey` cleanly (handling `BadAccess` via X11 error trap without aborting if another client already holds `PrintScreen`).
    - `npm test` exits `0`.

### `P2 Polish & Scale` — Expanded Automated Test Coverage & Web Workbench Parity

- [ ] **Task P2-8: Add headless GTK 3 / Cairo C tests for `editor_canvas.c` and `overlay.c`**
  - **Objective**: Bring `native/src/editor_canvas.c` (937 lines) and `native/src/overlay.c` (469 lines) under automated regression testing covering all 10 `ScToolKind` tools, 40-step undo/redo ring buffer eviction (`SC_MAX_HISTORY`), eraser hit-testing, ruler snap, crop flattening, and `sc_editor_render_pixbuf` pixel output.
  - **Target Files**:
    - `native/tests/test_editor_gtk.c` (new GTK/Cairo test binary exercising `ScEditorCanvas` public API offscreen under `xvfb-run` / `gtk_init_check`)
    - `native/Makefile` (add `test-gtk` target or conditional `xvfb-run` execution in `test`)
    - `scripts/run-tests.sh` (invoke headless GTK C canvas test suite when `xvfb-run` is available)
  - **Dependencies**: Task `P0-1`, Task `P0-3`.
  - **Verification Criteria**:
    - `bash ./scripts/run-tests.sh` executes the new GTK C canvas suite under `xvfb-run`, verifying pixel color deltas after drawing Pen, Highlighter, Rect, Ellipse, Line, Arrow, Redact, Pixelate, Eraser, Undo/Redo, and Crop on a synthetic `GdkPixbuf`.

- [ ] **Task P2-9: Add automated React UI / Playwright browser tests exercising all 9 existing `data-testid` hooks**
  - **Objective**: Add end-to-end DOM/canvas verification for the React 19 Web Workbench using the 9 `data-testid` hooks already wired in the components (`snip-launcher-window`, `capture-overlay-canvas`, `snip-editor-window`, `snip-editor-canvas`, `detected-protocol-value`, `detected-source-value`, `detected-capture-driver`, `detected-clipboard-driver`, `synced-deb-filename`).
  - **Target Files**:
    - `package.json` (add `@playwright/test` or `vitest` + `@testing-library/react` devDependencies and `"test:ui"` script)
    - `tests/web-workbench.spec.ts` (new UI test suite verifying `460×200` launcher state, sample window snip loading, annotation stroke count increment on `data-annotations`, preset switching in `DisplayDetectionPanel.tsx:308-350`, and tag sync preview in `DebReleaseSection.tsx:267`)
    - `.github/workflows/ci.yml` (add UI test step after `npm run build`)
  - **Dependencies**: Task `P0-1`.
  - **Verification Criteria**:
    - `npm run test:ui` passes all 9 `data-testid` component workflows headlessly with `0` console errors.

- [ ] **Task P2-10: Sync Web Workbench parity (`400%` zoom, PNG/JPEG export, live GitHub Release link, dynamic workflow preview & `LinuxBinarySet` cleanup)**
  - **Objective**: Close all remaining UI/UX parity gaps between the React Web Workbench and the native GTK 3 app + live GitHub repository.
  - **Target Files**:
    - `src/components/SnipStudio.tsx` (line `1951`: raise max zoom from `3` to `4` (`400%`); lines `500-531`: add PNG/JPEG export option and accurate clipboard fallback indicator when `navigator.clipboard` is unavailable)
    - `src/utils/snipEngine.ts` (lines `63-73`: remove dead `xprop` and `xwininfo` properties from `LinuxBinarySet`)
    - `src/components/DisplayDetectionPanel.tsx` (lines `25-120`: remove dead `xprop`/`xwininfo` preset fields)
    - `src/components/DebReleaseSection.tsx` (lines `23-31, 51-72, 101-120`: import or sync real `.github/workflows/release-deb.yml` via Vite `?raw`, surface honest copy fallback state, and add outbound `<a href="https://github.com/ddastoor/SnipClip/releases">` button)
    - `src/components/Header.tsx` & `src/App.tsx` (add external GitHub repo / Releases link in footer/header)
    - `src/utils/snipEngine.test.ts` (update `LinuxBinarySet` fixtures)
  - **Dependencies**: Task `P0-2`, Task `P1-5`.
  - **Verification Criteria**:
    - `npm test && npm run build` exits `0` with `0` TypeScript errors and verified outbound `https://github.com/ddastoor/SnipClip/releases` link in rendered markup.

---

## 7. Architectural Decisions, Constraints & "Do Not Repeat" Log

> **Mandatory Guardrails for Local Agents**: Every constraint below exists because an alternative failed in real Debian/Ubuntu desktop environments or CI. Do **not** regress these invariants.

1. **100% Native Compiled C11 + GTK 3 ELF Binary — Zero Python in `.deb`**
   - **Rule**: `/usr/bin/snipclip` must be a compiled, stripped 64-bit C11 ELF binary (`ELFCLASS64`), and the staged `.deb` archive must contain **zero** `.py` files and **zero** `python3-*` runtime `Depends`. Python (`scripts/generate_icons.py`) is permitted strictly at build/test time.
   - **Why**: Earlier Python/PyQt6 packaging suffered broken dependencies and slow startup across minimal Debian/XFCE/Mint setups. Enforced by `test_usr_bin_snipclip_is_compiled_elf64_and_contains_zero_python` (`linux/tests/test_native_elf_and_deb.py:56-72`).

2. **Always Pass `-Zxz --uniform-compression --root-owner-group` to `dpkg-deb` (Never `zstd`)**
   - **Rule**: Keep `dpkg-deb -Zxz --uniform-compression --root-owner-group --build` in `scripts/build-deb.sh:249`.
   - **Why**: Ubuntu 22.04/24.04's `dpkg-deb` defaults to `zstd` (`control.tar.zst` / `data.tar.zst`), which causes Debian 12 Bookworm `dpkg`, KDE Discover, GNOME Software, and GDebi to reject or hang on the package. Enforced by `test_deb_archive_uses_xz_compression_for_cross_distro_gui_installers` (`linux/tests/test_native_elf_and_deb.py:238-251`).

3. **Preserve Dual `t64 | pre-t64` Shared Library `Depends` in `DEBIAN/control`**
   - **Rule**: Keep `libc6 (>= 2.35), libgtk-3-0t64 | libgtk-3-0, libglib2.0-0t64 | libglib2.0-0, libcairo2, libx11-6` (`scripts/build-deb.sh:30`).
   - **Why**: Ubuntu 24.04+ and Debian 13 renamed 64-bit time_t packages with a `t64` suffix (`libgtk-3-0t64`), while Debian 12 and Ubuntu 22.04 only provide `libgtk-3-0`. Dual `|` alternatives allow one `.deb` to install cleanly across both generations. Enforced by `linux/tests/test_native_elf_and_deb.py:252-262`.

4. **Compile Official Release `.deb` Packages on `debian:bookworm` (`GLIBC <= 2.36`), Never Ubuntu 24.04+**
   - **Rule**: Official distributable `.deb` binaries must be built inside `debian:bookworm` (`.github/workflows/release-deb.yml:33-34`) so ELF dynamic symbol versions stay `<= GLIBC_2.36`.
   - **Why**: Compiling on Ubuntu 24.04 (`glibc 2.39`) silently links C23 symbols (`__isoc23_strtol@GLIBC_2.38`), causing immediate dynamic linker failures (`version 'GLIBC_2.38' not found`) on Debian 12 Bookworm and Ubuntu 22.04. Enforced when running on Bookworm by `linux/tests/test_native_elf_and_deb.py:88-102`.

5. **Close `stdin` (`</dev/null`) in Every `DEBIAN/postinst` & `DEBIAN/postrm` Command**
   - **Rule**: Every desktop/icon cache refresh command in `postinst` and `postrm` (`scripts/build-deb.sh:187-231`) must redirect `</dev/null >/dev/null 2>&1 || true`.
   - **Why**: GUI software centers (PackageKit, KDE Discover, GNOME Software) invoke maintainer scripts with open IPC pipes and hang indefinitely at 99% installation if child utilities inherit open `stdin`/`stdout` descriptors. Enforced by `linux/tests/test_native_elf_and_deb.py:277-283`.

6. **Never Regress the Compact `460×200` Startup Launcher or Unwrap `GtkScrolledWindow` on the Annotation Strip**
   - **Rule**: Keep `SC_LAUNCHER_WIDTH = 460` and `SC_LAUNCHER_HEIGHT = 200` (`native/include/geometry.h:81-82`, `src/utils/snipEngine.ts:97-98`), keep `min-width: 0; min-height: 0;` in `APP_CSS` (`native/src/main.c:58,63,65,68,72`), keep editor-only controls hidden via `hide_with_no_show_all` (`native/src/main.c:762-766`) when no image is loaded, and keep the annotation buttons inside `app->annot_scroll` (`GtkScrolledWindow` with `GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER` at `native/src/main.c:922-986`).
   - **Why**: GTK 3 `GtkWindow` strictly enforces the sum of visible children's natural/minimum widths. Packing all 26 capture, history, and annotation widgets into a single unscrollable `GtkBox` forced the startup window to span ~1,855–1,920 px horizontally even when `gtk_window_set_default_size` requested a smaller width. Enforced under live `Xvfb` by `test_startup_window_matches_microsoft_snipping_tool_dimensions` (`linux/tests/test_native_elf_and_deb.py:207-236`).

7. **Maintain the 5-Artifact Version & Checksum Synchronization Invariant**
   - **Rule**: Never manually edit `VERSION`, `native/include/snipclip_version.h`, `public/releases/snipclip_<ver>_<arch>.deb`, `public/releases/SHA256SUMS`, or `src/data/releaseInfo.json` independently. Always regenerate them together via `scripts/build-deb.sh` (and keep `package.json:4` `"version"` aligned on version bumps).
   - **Why**: Both `linux/tests/test_native_elf_and_deb.py:317-322` and `src/utils/snipEngine.test.ts` verify exact version strings, `.deb` byte sizes, base64 `!<arch>\n` headers, and SHA-256 checksums across all 5 files.

8. **Probe Wayland/X11 Sockets via `stat()` + `S_ISSOCK()` Only — Never `connect()` Pre-`gtk_init`**
   - **Rule**: In `discover_wayland_socket_in_dir` (`native/src/display_detect.c:68-97`) and `discover_x11_socket` (`native/src/display_detect.c:99-133`), check socket existence with `stat()` + `S_ISSOCK(st.st_mode)` without calling `connect()`. Disambiguate stale Wayland sockets via explicit `DISPLAY` + `loginctl` (`native/src/display_detect.c:310-314`) and post-`gtk_init_check` `sc_reconcile_with_gdk_backend_name` (`native/src/display_detect.c:416-432`).
   - **Why**: `test_binary_auto_detects_wayland_x11_and_stripped_gui_socket_sessions` (`linux/tests/test_native_elf_and_deb.py:169-171`) binds an `AF_UNIX` socket without calling `listen()`, where `connect()` fails with `ECONNREFUSED`/`EINVAL`; furthermore, opening and immediately dropping raw client connections to live Wayland compositors before `gtk_init` causes noisy compositor disconnect logs.

9. **Keep `native/build/test_snipclip_native` Display-Server-Free**
   - **Rule**: `native/Makefile:20-24,36-38` links `test_snipclip_native` with `-lm` only, using callback injection (`sc_detect_display_session_custom`) rather than requiring a live X11/Wayland socket.
   - **Why**: `make -C native test` runs inside minimal `debian:bookworm` release containers (`.github/workflows/release-deb.yml:77`) where neither Xorg nor Wayland nor `Xvfb` is running. Any live GTK widget tests must live in a separate target or guard on `gtk_init_check` / `xvfb-run`.
