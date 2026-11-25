# SnipClip

Natively compiled **C11 + GTK 3 + Cairo** Windows 11 Snipping Tool clone for Debian-based Linux distributions (**Debian 12/13**, **Ubuntu 22.04 / 24.04 / 24.10 / 25.04**, **Kubuntu**, **Linux Mint 21/22**, **Pop!_OS**, **Kali**, **Zorin OS**, **MX Linux**) with automatic **X11** and **Wayland** startup detection.

## Architecture & Multi-Distro Debian Portability

- **100% Native Compiled ELF Executable (`/usr/bin/snipclip`)**: Written in C11 with GTK 3, Cairo, GdkPixbuf, GIO/GDBus, and `libX11` — zero Python or interpreted runtime dependencies.
- **Cross-Distro Debian & Ubuntu `.deb` Compatibility**:
  - Compiled against the Debian 12 Bookworm ABI baseline (`libc6 >= 2.35`) so a single `amd64` binary runs natively across Ubuntu 22.04+, Debian 12+, Linux Mint 21/22, MX Linux 23, Zorin OS 17, Pop!_OS, Kali, and Ubuntu 24.04/24.10/25.04.
  - `DEBIAN/control` declares dual `t64` / pre-`t64` package alternatives (`libgtk-3-0t64 | libgtk-3-0`, `libglib2.0-0t64 | libglib2.0-0`) and uses `dpkg-deb -Zxz --uniform-compression` (never `zstd`) so every Debian/Ubuntu GUI package manager installs cleanly without hanging.
- **Automatic GUI Startup X11 & Wayland Detection**: Inspects `XDG_SESSION_TYPE`, `WAYLAND_DISPLAY`, `$XDG_RUNTIME_DIR/wayland-*` & `/run/user/<uid>/wayland-*` UNIX sockets, `loginctl`, and `DISPLAY` / `/tmp/.X11-unix/X*` sockets before `gtk_init()` and reconciles with the live `GdkDisplay`:
  - **Wayland**: Native `GDBusConnection` call to `org.freedesktop.portal.Screenshot` (with `grim` / `spectacle` / `gnome-screenshot` fallbacks) and `GtkClipboard` + `wl-copy`.
  - **X11**: Direct `gdk_pixbuf_get_from_window` root capture, built-in `libX11` `_NET_CLIENT_LIST_STACKING` + `XTranslateCoordinates` window hover snapping, and `GtkClipboard` + `xclip`.
- **All Windows Snipping Tool Capture Modes & Annotation Tools**:
  - **Rectangle**, **Window**, **Fullscreen**, and **Freeform** lasso snips
  - **Delay Timer** (`No delay`, `3s`, `5s`, `10s`) and **PrintScreen** + customizable keyboard shortcuts (interactive GTK Shortcut Recorder)
  - **Pen**, **Highlighter**, **Stroke Eraser**, **Shapes** (Rectangle, Ellipse, Line, Arrow), **Redact** & **Pixelate**, draggable/rotatable **Ruler** (`0°–180°` straight-edge snapping), **Crop**, **Zoom** (`25%–400%`), **Undo/Redo**, **Copy to Clipboard**, and **Save PNG/JPEG**.

## Graphical Install & Launch (Zero Command Line Required)

1. **Download `snipclip_2.1.0_amd64.deb`** from the GitHub Releases page (or directly from the web workbench).
2. **Double-click `snipclip_2.1.0_amd64.deb`** in your file manager (Dolphin, GNOME Files, Nemo, Thunar) to open **KDE Discover**, **GNOME Software**, or **GDebi**, then click **Install** (or **Update**).
3. **Click "Launch" right inside the GUI installer** or open **SnipClip** from your desktop's **Application Launcher** (`Utilities`, `Graphics`, or `All Applications`).
4. **Automatic X11 & Wayland Startup Detection**: SnipClip automatically detects **Wayland** or **X11** on GUI launch — zero terminal commands or flags needed.

## Build Native `.deb`, Run Repository Test Suite & Automated GitHub Releases

Pushing changes to `main` (or pushing a `v*.*.*` tag) automatically triggers `.github/workflows/release-deb.yml` inside a `debian:bookworm` container to compile the C11/GTK3 binary, run all test suites, build `snipclip_<version>_amd64.deb`, and publish it to the GitHub Releases page.

```bash
# 1. Compile native C11 + GTK 3 ELF binary & package snipclip_2.1.0_amd64.deb into public/releases/
./scripts/build-deb.sh 2.1.0

# 2. Run the full repository test suite (Native C tests + ELF/.deb integration tests + TypeScript tests + tsc)
./scripts/run-tests.sh
```

## Interactive Web Preview & Release Workbench

```bash
npm install
npm run dev
```