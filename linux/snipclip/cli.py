"""Command-line entry point for SnipClip on Debian-based Linux systems."""

from __future__ import annotations

import argparse
import json
import os
import sys
from typing import List, Optional

from .display_backend import detect_display_session
from .version import get_version_banner


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="snipclip",
        description="Windows Snipping Tool clone for Debian-based Linux (X11 & Wayland).",
    )
    parser.add_argument(
        "-v",
        "--version",
        action="store_true",
        help="Print SnipClip version matching the GitHub .deb release and exit.",
    )
    parser.add_argument(
        "--detect-display",
        action="store_true",
        help="Auto-detect X11 or Wayland display server details as JSON and exit.",
    )
    parser.add_argument(
        "-m",
        "--mode",
        choices=["rectangle", "window", "fullscreen", "freeform"],
        default="rectangle",
        help="Initial snipping mode (default: rectangle).",
    )
    parser.add_argument(
        "-d",
        "--delay",
        type=int,
        choices=[0, 3, 5, 10],
        default=0,
        help="Delay in seconds before snipping starts (0, 3, 5, or 10).",
    )
    parser.add_argument(
        "--capture-now",
        action="store_true",
        help="Immediately start a snip capture when launched (useful for PrintScreen bindings).",
    )
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    if args.version:
        print(get_version_banner())
        return 0

    session = detect_display_session()
    if args.detect_display:
        print(json.dumps(session.to_dict(), indent=2))
        return 0

    # Configure Qt platform plugin automatically at startup for Wayland (with xcb fallback) or X11
    if "QT_QPA_PLATFORM" not in os.environ and session.protocol in ("wayland", "x11"):
        os.environ["QT_QPA_PLATFORM"] = session.qt_qpa_platform
    if session.protocol == "wayland" and session.wayland_display and "WAYLAND_DISPLAY" not in os.environ:
        os.environ["WAYLAND_DISPLAY"] = session.wayland_display

    try:
        from PyQt6.QtWidgets import QApplication
    except ImportError:
        sys.stderr.write(
            "Error: python3-pyqt6 is required to launch the SnipClip GUI.\n"
            "Install dependencies via: sudo apt-get install -y python3-pyqt6\n"
        )
        return 1

    from .main_window import SnipClipMainWindow

    app = QApplication(sys.argv)
    app.setApplicationName("SnipClip")
    window = SnipClipMainWindow(
        session=session,
        initial_mode=args.mode,
        initial_delay=args.delay,
        autostart_snip=args.capture_now,
    )
    window.show()
    return int(app.exec())


if __name__ == "__main__":
    raise SystemExit(main())
