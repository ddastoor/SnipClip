# SnipClip

Windows 11 Snipping Tool clone for Debian-based Linux distributions (Debian 12+, Ubuntu 22.04+, Linux Mint, Pop!_OS, Kali) with automatic **X11** and **Wayland** display server detection.

## Features

- **Automatic Display Server Detection**: Inspects `XDG_SESSION_TYPE`, `WAYLAND_DISPLAY`, and `DISPLAY` at launch to select XDG Desktop Portal / `grim` + `wl-copy` on Wayland or `QScreen.grabWindow` + `_NET_CLIENT_LIST` window enumeration + `xclip` on X11.
- **All Windows Snipping Tool Capture Modes**:
  - **Rectangle Snip**: Drag to capture any rectangular region with live pixel dimensions.
  - **Window Snip**: Click an open window to capture its exact frame.
  - **Fullscreen Snip**: Instant full-desktop capture across single or multi-monitor setups.
  - **Freeform Snip**: Draw a custom lasso path around irregular regions.
- **Delay Timer**: Capture immediately (`No delay`) or after `3s`, `5s`, or `10s` to capture open menus and tooltips.
- **Rich Annotation & Editing Suite**:
  - Ballpoint Pen & translucent Highlighter
  - Stroke Eraser & Clear All Ink
  - Shapes (Rectangle, Ellipse, Line, Arrow)
  - Sensitive Data Redaction (Solid Blackout & Mosaic Pixelate/Blur)
  - Draggable, rotatable **Ruler** (`0°–180°` straight-edge snapping)
  - Interactive **Crop**, Zoom (`25%–400%`), Undo (`Ctrl+Z`), Redo (`Ctrl+Y`)
  - Automatic & one-click Copy to Clipboard (`Ctrl+C`) and Save As PNG/JPEG/WebP (`Ctrl+S`)
- **Release-Synced Version Number**: `scripts/build-deb.sh` and `.github/workflows/release-deb.yml` synchronize `VERSION`, `snipclip --version`, `DEBIAN/control`, and the GitHub Release `.deb` filename (`snipclip_<version>_all.deb`).

## Install `.deb` Package (Debian / Ubuntu)

Download the latest `snipclip_<version>_all.deb` from the **GitHub Releases** page (or build locally), then install with `apt`:

```bash
sudo apt install ./snipclip_1.0.0_all.deb
```

Verify version parity with the GitHub release:

```bash
snipclip --version
# SnipClip 1.0.0 (snipclip_1.0.0_all.deb, release v1.0.0)

snipclip --detect-display
```

## Build `.deb`, Run Repository Test Suite & Cut a GitHub Release

```bash
# 1. Build snipclip_<version>_all.deb into public/releases/
./scripts/build-deb.sh 1.0.0

# 2. Run the full repository test suite (Python Linux/dpkg tests + TypeScript engine/parity tests + tsc)
chmod +x scripts/run-tests.sh
./scripts/run-tests.sh
# or: npm test

# 3. Trigger automated GitHub Release (.github/workflows/release-deb.yml)
git tag v1.0.0
git push origin v1.0.0
```

## Interactive Web Preview & Release Workbench

```bash
npm install
npm run dev
```