"""Runtime X11 / Wayland auto-detection, screenshot capture, and clipboard integration."""

from __future__ import annotations

import dataclasses
import os
import re
import shutil
import subprocess
import tempfile
import urllib.parse
from pathlib import Path
from typing import Callable, Dict, List, Mapping, Optional, Tuple


@dataclasses.dataclass(frozen=True)
class WindowGeometry:
    """Represents a desktop window rectangle on X11."""

    window_id: str
    title: str
    x: int
    y: int
    width: int
    height: int


@dataclasses.dataclass(frozen=True)
class DisplaySessionInfo:
    """Detected Linux display server environment and active capture/clipboard backends."""

    protocol: str  # "wayland" | "x11" | "headless"
    detection_source: str
    qt_qpa_platform: str
    desktop_environment: str
    wayland_display: str
    x11_display: str
    capture_driver: str
    clipboard_driver: str
    supports_window_enumeration: bool

    def to_dict(self) -> Dict[str, object]:
        return dataclasses.asdict(self)


def _probe_loginctl_session_type(
    environ: Mapping[str, str],
    which_fn: Callable[[str], Optional[str]],
) -> str:
    """Query systemd-logind for session Type if environment variables were stripped."""
    session_id = environ.get("XDG_SESSION_ID", "").strip()
    if not session_id or not which_fn("loginctl"):
        return ""
    try:
        res = subprocess.run(
            ["loginctl", "show-session", session_id, "-p", "Type", "--value"],
            capture_output=True,
            text=True,
            timeout=1.5,
            check=False,
        )
        if res.returncode == 0:
            val = res.stdout.strip().lower()
            if val in ("wayland", "x11"):
                return val
    except (OSError, subprocess.SubprocessError):
        pass
    return ""


def _discover_runtime_wayland_socket(environ: Mapping[str, str]) -> str:
    """Check XDG_RUNTIME_DIR for active wayland-* sockets when launched from keybind daemons."""
    runtime_dir = environ.get("XDG_RUNTIME_DIR", "").strip()
    if not runtime_dir:
        return ""
    rt_path = Path(runtime_dir)
    if not rt_path.is_dir():
        return ""
    try:
        for candidate in sorted(rt_path.glob("wayland-[0-9]*")):
            if not candidate.name.endswith(".lock") and candidate.is_socket():
                return candidate.name
    except OSError:
        pass
    return ""


def detect_display_session(
    env: Optional[Mapping[str, str]] = None,
    which_fn: Callable[[str], Optional[str]] = shutil.which,
) -> DisplaySessionInfo:
    """Auto-detect whether the current Linux session is running Wayland or X11 at startup.

    Startup auto-detection cascade:
    1. Explicit ``XDG_SESSION_TYPE`` ("wayland" or "x11")
    2. ``WAYLAND_DISPLAY`` socket variable -> Wayland
    3. Active ``$XDG_RUNTIME_DIR/wayland-*`` socket discovery -> Wayland
    4. ``loginctl show-session $XDG_SESSION_ID -p Type`` -> Wayland or X11
    5. ``DISPLAY`` X11 socket variable -> X11
    6. Fallback -> headless
    """
    environ = env if env is not None else os.environ
    session_type = environ.get("XDG_SESSION_TYPE", "").strip().lower()
    wayland_display = environ.get("WAYLAND_DISPLAY", "").strip()
    x11_display = environ.get("DISPLAY", "").strip()
    desktop = (
        environ.get("XDG_CURRENT_DESKTOP")
        or environ.get("DESKTOP_SESSION")
        or "Generic Linux Desktop"
    ).strip()

    protocol = "headless"
    detection_source = "no display environment variables or sockets found"

    if session_type == "wayland":
        protocol = "wayland"
        detection_source = "XDG_SESSION_TYPE=wayland"
    elif session_type == "x11":
        protocol = "x11"
        detection_source = "XDG_SESSION_TYPE=x11"
    elif wayland_display:
        protocol = "wayland"
        detection_source = f"WAYLAND_DISPLAY={wayland_display}"
    else:
        discovered_wl = _discover_runtime_wayland_socket(environ)
        if discovered_wl:
            protocol = "wayland"
            wayland_display = discovered_wl
            detection_source = f"XDG_RUNTIME_DIR/{discovered_wl} socket"
        else:
            loginctl_type = _probe_loginctl_session_type(environ, which_fn)
            if loginctl_type in ("wayland", "x11"):
                protocol = loginctl_type
                detection_source = f"loginctl session Type={loginctl_type}"
            elif x11_display:
                protocol = "x11"
                detection_source = f"DISPLAY={x11_display}"

    if protocol == "wayland":
        qt_qpa_platform = "wayland;xcb"
        if which_fn("gdbus"):
            capture_driver = "xdg-desktop-portal (org.freedesktop.portal.Screenshot)"
        elif which_fn("grim"):
            capture_driver = "grim (wlroots Wayland protocol)"
        elif which_fn("gnome-screenshot"):
            capture_driver = "gnome-screenshot"
        elif which_fn("spectacle"):
            capture_driver = "spectacle (KDE)"
        else:
            capture_driver = "qt-screen-grab (XWayland fallback)"

        clipboard_driver = "wl-copy + QClipboard" if which_fn("wl-copy") else "QClipboard"
        supports_window_enum = False
    elif protocol == "x11":
        qt_qpa_platform = "xcb"
        capture_driver = "Qt X11 Screen Grab (QScreen.grabWindow)"
        if which_fn("xclip"):
            clipboard_driver = "xclip + QClipboard"
        elif which_fn("xsel"):
            clipboard_driver = "xsel + QClipboard"
        else:
            clipboard_driver = "QClipboard"
        supports_window_enum = bool(which_fn("xprop") and which_fn("xwininfo"))
    else:
        qt_qpa_platform = "offscreen"
        capture_driver = "offscreen-buffer"
        clipboard_driver = "QClipboard"
        supports_window_enum = False

    return DisplaySessionInfo(
        protocol=protocol,
        detection_source=detection_source,
        qt_qpa_platform=qt_qpa_platform,
        desktop_environment=desktop,
        wayland_display=wayland_display,
        x11_display=x11_display,
        capture_driver=capture_driver,
        clipboard_driver=clipboard_driver,
        supports_window_enumeration=supports_window_enum,
    )


def enumerate_x11_windows() -> List[WindowGeometry]:
    """List visible top-level X11 windows for Window Snip hover highlighting."""
    if not (shutil.which("xprop") and shutil.which("xwininfo")):
        return []

    try:
        proc = subprocess.run(
            ["xprop", "-root", "_NET_CLIENT_LIST_STACKING"],
            capture_output=True,
            text=True,
            timeout=2.0,
            check=False,
        )
        if proc.returncode != 0 or "#" not in proc.stdout:
            proc = subprocess.run(
                ["xprop", "-root", "_NET_CLIENT_LIST"],
                capture_output=True,
                text=True,
                timeout=2.0,
                check=False,
            )
        if proc.returncode != 0 or "#" not in proc.stdout:
            return []

        ids_part = proc.stdout.split("#", 1)[1]
        window_ids = [w.strip() for w in ids_part.split(",") if w.strip().startswith("0x")]
        # Reverse stacking order so topmost window is tested first
        window_ids.reverse()
    except (OSError, subprocess.SubprocessError):
        return []

    windows: List[WindowGeometry] = []
    for wid in window_ids:
        geom = _inspect_x11_window(wid)
        if geom is not None:
            windows.append(geom)
    return windows


def _inspect_x11_window(window_id: str) -> Optional[WindowGeometry]:
    try:
        info = subprocess.run(
            ["xwininfo", "-id", window_id],
            capture_output=True,
            text=True,
            timeout=1.5,
            check=False,
        )
        if info.returncode != 0:
            return None
        out = info.stdout
        if "Map State: IsViewable" not in out:
            return None

        title_match = re.search(r'xwininfo: Window id: \S+ "(.*)"', out)
        x_match = re.search(r"Absolute upper-left X:\s+(-?\d+)", out)
        y_match = re.search(r"Absolute upper-left Y:\s+(-?\d+)", out)
        w_match = re.search(r"Width:\s+(\d+)", out)
        h_match = re.search(r"Height:\s+(\d+)", out)

        if not (x_match and y_match and w_match and h_match):
            return None

        width = int(w_match.group(1))
        height = int(h_match.group(1))
        if width < 24 or height < 24:
            return None

        return WindowGeometry(
            window_id=window_id,
            title=title_match.group(1) if title_match else window_id,
            x=int(x_match.group(1)),
            y=int(y_match.group(1)),
            width=width,
            height=height,
        )
    except (OSError, subprocess.SubprocessError, ValueError):
        return None


def capture_wayland_screenshot_to_file(interactive: bool = False) -> Optional[Path]:
    """Capture a Wayland screenshot via XDG Desktop Portal, grim, gnome-screenshot, or spectacle."""
    tmp_fd, tmp_name = tempfile.mkstemp(prefix="snipclip_", suffix=".png")
    os.close(tmp_fd)
    target = Path(tmp_name)

    # 1. Try grim first on wlroots compositors when non-interactive full desktop grab is requested
    if not interactive and shutil.which("grim"):
        res = subprocess.run(["grim", str(target)], capture_output=True, timeout=5.0, check=False)
        if res.returncode == 0 and target.exists() and target.stat().st_size > 0:
            return target

    # 2. Try gnome-screenshot or spectacle
    if not interactive and shutil.which("gnome-screenshot"):
        res = subprocess.run(
            ["gnome-screenshot", "-f", str(target)],
            capture_output=True,
            timeout=6.0,
            check=False,
        )
        if res.returncode == 0 and target.exists() and target.stat().st_size > 0:
            return target

    if not interactive and shutil.which("spectacle"):
        res = subprocess.run(
            ["spectacle", "-b", "-n", "-o", str(target)],
            capture_output=True,
            timeout=6.0,
            check=False,
        )
        if res.returncode == 0 and target.exists() and target.stat().st_size > 0:
            return target

    # 3. Try XDG Desktop Portal via QtDBus if PyQt6.QtDBus is available
    portal_path = _capture_via_qt_portal(interactive=interactive)
    if portal_path is not None and portal_path.exists():
        try:
            target.unlink(missing_ok=True)
        except OSError:
            pass
        return portal_path

    try:
        target.unlink(missing_ok=True)
    except OSError:
        pass
    return None


def _capture_via_qt_portal(interactive: bool = False) -> Optional[Path]:
    """Request a screenshot from org.freedesktop.portal.Screenshot over session DBus."""
    try:
        from PyQt6.QtCore import QEventLoop, QObject, QTimer, pyqtSlot
        from PyQt6.QtDBus import QDBusConnection, QDBusInterface, QDBusMessage
    except ImportError:
        return None

    bus = QDBusConnection.sessionBus()
    if not bus.isConnected():
        return None

    iface = QDBusInterface(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Screenshot",
        bus,
    )
    if not iface.isValid():
        return None

    token = f"snipclip_{os.getpid()}"
    options = {"handle_token": token, "interactive": bool(interactive), "modal": bool(interactive)}
    reply = iface.call("Screenshot", "", options)
    if reply.type() == QDBusMessage.MessageType.ErrorMessage or not reply.arguments():
        return None

    request_path = str(reply.arguments()[0])
    result_uri: List[str] = []
    loop = QEventLoop()

    class _PortalReceiver(QObject):
        @pyqtSlot(int, "QVariantMap")
        def on_response(self, response_code: int, results: Dict[str, object]) -> None:
            if response_code == 0 and "uri" in results:
                result_uri.append(str(results["uri"]))
            loop.quit()

    receiver = _PortalReceiver()
    bus.connect(
        "org.freedesktop.portal.Desktop",
        request_path,
        "org.freedesktop.portal.Request",
        "Response",
        receiver,
        "on_response",
    )
    QTimer.singleShot(30000 if interactive else 8000, loop.quit)
    loop.exec()

    if not result_uri:
        return None

    parsed = urllib.parse.urlparse(result_uri[0])
    file_path = Path(urllib.parse.unquote(parsed.path))
    return file_path if file_path.exists() else None


def copy_png_bytes_to_system_clipboard(png_bytes: bytes, session: DisplaySessionInfo) -> bool:
    """Persist PNG image bytes into the native Wayland (wl-copy) or X11 (xclip/xsel) clipboard."""
    if session.protocol == "wayland" and shutil.which("wl-copy"):
        try:
            proc = subprocess.Popen(["wl-copy", "--type", "image/png"], stdin=subprocess.PIPE)
            proc.communicate(input=png_bytes, timeout=3.0)
            return proc.returncode == 0
        except (OSError, subprocess.SubprocessError):
            return False

    if session.protocol == "x11" and shutil.which("xclip"):
        try:
            proc = subprocess.Popen(
                ["xclip", "-selection", "clipboard", "-t", "image/png", "-i"],
                stdin=subprocess.PIPE,
            )
            proc.communicate(input=png_bytes, timeout=3.0)
            return proc.returncode == 0
        except (OSError, subprocess.SubprocessError):
            return False

    return False


def grab_full_virtual_desktop_qt() -> Tuple[Optional[object], int, int]:
    """Grab and stitch all screens into one composite QPixmap with virtual origin (min_x, min_y)."""
    from PyQt6.QtCore import QRect, Qt
    from PyQt6.QtGui import QGuiApplication, QPainter, QPixmap

    screens = QGuiApplication.screens()
    if not screens:
        return None, 0, 0

    total_rect = QRect()
    for screen in screens:
        total_rect = total_rect.united(screen.geometry())

    composite = QPixmap(total_rect.size())
    composite.fill(Qt.GlobalColor.black)

    painter = QPainter(composite)
    for screen in screens:
        geom = screen.geometry()
        grab = screen.grabWindow(0)
        target_rect = QRect(
            geom.x() - total_rect.x(),
            geom.y() - total_rect.y(),
            geom.width(),
            geom.height(),
        )
        painter.drawPixmap(target_rect, grab)
    painter.end()

    return composite, total_rect.x(), total_rect.y()
