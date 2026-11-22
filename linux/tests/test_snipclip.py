"""Unit tests for SnipClip Linux application, X11/Wayland detection, and .deb version parity."""

from __future__ import annotations

import io
import json
import subprocess
import sys
import unittest
from contextlib import redirect_stdout
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT_DIR / "linux"))

from snipclip import cli, display_backend, version  # noqa: E402


class SnipClipLinuxTests(unittest.TestCase):
    def test_wayland_detection_via_xdg_session_type(self) -> None:
        env = {
            "XDG_SESSION_TYPE": "wayland",
            "WAYLAND_DISPLAY": "wayland-0",
            "DISPLAY": ":0",
            "XDG_CURRENT_DESKTOP": "GNOME",
        }
        info = display_backend.detect_display_session(
            env=env,
            which_fn=lambda cmd: f"/usr/bin/{cmd}" if cmd in ("gdbus", "wl-copy") else None,
        )
        self.assertEqual(info.protocol, "wayland")
        self.assertEqual(info.detection_source, "XDG_SESSION_TYPE=wayland")
        self.assertEqual(info.qt_qpa_platform, "wayland;xcb")
        self.assertEqual(info.desktop_environment, "GNOME")
        self.assertIn("xdg-desktop-portal", info.capture_driver)
        self.assertEqual(info.clipboard_driver, "wl-copy + QClipboard")
        self.assertFalse(info.supports_window_enumeration)

    def test_wayland_detection_via_socket_when_session_type_missing(self) -> None:
        env = {
            "WAYLAND_DISPLAY": "wayland-1",
            "DISPLAY": ":0",
            "XDG_CURRENT_DESKTOP": "sway",
        }
        info = display_backend.detect_display_session(
            env=env,
            which_fn=lambda cmd: f"/usr/bin/{cmd}" if cmd == "grim" else None,
        )
        self.assertEqual(info.protocol, "wayland")
        self.assertEqual(info.detection_source, "WAYLAND_DISPLAY=wayland-1")
        self.assertIn("grim", info.capture_driver)

    def test_x11_detection_and_window_enumeration_capability(self) -> None:
        env = {
            "XDG_SESSION_TYPE": "x11",
            "DISPLAY": ":0.0",
            "XDG_CURRENT_DESKTOP": "XFCE",
        }
        info = display_backend.detect_display_session(
            env=env,
            which_fn=lambda cmd: f"/usr/bin/{cmd}" if cmd in ("xclip", "xprop", "xwininfo") else None,
        )
        self.assertEqual(info.protocol, "x11")
        self.assertEqual(info.detection_source, "XDG_SESSION_TYPE=x11")
        self.assertEqual(info.qt_qpa_platform, "xcb")
        self.assertEqual(info.desktop_environment, "XFCE")
        self.assertEqual(info.clipboard_driver, "xclip + QClipboard")
        self.assertTrue(info.supports_window_enumeration)

    def test_headless_fallback_when_no_display_vars_exist(self) -> None:
        info = display_backend.detect_display_session(env={}, which_fn=lambda _cmd: None)
        self.assertEqual(info.protocol, "headless")

    def test_cli_version_matches_version_file_and_deb_artifact(self) -> None:
        expected_version = (ROOT_DIR / "VERSION").read_text(encoding="utf-8").strip()
        self.assertEqual(version.__version__, expected_version)
        self.assertEqual(version.DEB_FILENAME, f"snipclip_{expected_version}_all.deb")

        buf = io.StringIO()
        with redirect_stdout(buf):
            rc = cli.main(["--version"])
        self.assertEqual(rc, 0)
        self.assertIn(expected_version, buf.getvalue())
        self.assertIn(version.DEB_FILENAME, buf.getvalue())

        deb_path = ROOT_DIR / "public" / "releases" / version.DEB_FILENAME
        if deb_path.exists():
            info_out = subprocess.check_output(["dpkg-deb", "-f", str(deb_path), "Version"], text=True).strip()
            self.assertEqual(info_out, expected_version)

    def test_cli_detect_display_outputs_valid_json(self) -> None:
        buf = io.StringIO()
        with redirect_stdout(buf):
            rc = cli.main(["--detect-display"])
        self.assertEqual(rc, 0)
        parsed = json.loads(buf.getvalue())
        self.assertIn(parsed["protocol"], ("wayland", "x11", "headless"))
        self.assertIn("capture_driver", parsed)


if __name__ == "__main__":
    unittest.main()
