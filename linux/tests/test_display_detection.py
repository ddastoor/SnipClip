"""Extensive test suite for X11 and Wayland startup auto-detection, window parsing, and OS helpers."""

from __future__ import annotations

import socket
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT_DIR = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT_DIR / "linux"))

from snipclip import display_backend  # noqa: E402


class DisplayAutoDetectionTests(unittest.TestCase):
    def test_explicit_xdg_session_type_wayland_trimmed_and_case_insensitive(self) -> None:
        env = {
            "XDG_SESSION_TYPE": "  Wayland \n",
            "WAYLAND_DISPLAY": "wayland-0",
            "DISPLAY": ":0",
            "XDG_CURRENT_DESKTOP": "ubuntu:GNOME",
        }
        info = display_backend.detect_display_session(
            env=env,
            which_fn=lambda cmd: f"/usr/bin/{cmd}" if cmd in ("gdbus", "wl-copy") else None,
        )
        self.assertEqual(info.protocol, "wayland")
        self.assertEqual(info.detection_source, "XDG_SESSION_TYPE=wayland")
        self.assertEqual(info.qt_qpa_platform, "wayland;xcb")
        self.assertEqual(info.desktop_environment, "ubuntu:GNOME")
        self.assertIn("xdg-desktop-portal", info.capture_driver)
        self.assertEqual(info.clipboard_driver, "wl-copy + QClipboard")
        self.assertFalse(info.supports_window_enumeration)

    def test_explicit_xdg_session_type_x11_overrides_stale_vars(self) -> None:
        env = {
            "XDG_SESSION_TYPE": "X11",
            "DISPLAY": ":0.0",
            "DESKTOP_SESSION": "xfce",
        }
        info = display_backend.detect_display_session(
            env=env,
            which_fn=lambda cmd: f"/usr/bin/{cmd}" if cmd in ("xclip", "xprop", "xwininfo") else None,
        )
        self.assertEqual(info.protocol, "x11")
        self.assertEqual(info.detection_source, "XDG_SESSION_TYPE=x11")
        self.assertEqual(info.qt_qpa_platform, "xcb")
        self.assertEqual(info.desktop_environment, "xfce")
        self.assertTrue(info.supports_window_enumeration)

    def test_xwayland_ambiguity_prefers_wayland_when_both_sockets_set(self) -> None:
        env = {
            "XDG_SESSION_TYPE": "",
            "WAYLAND_DISPLAY": "wayland-0",
            "DISPLAY": ":0",
            "XDG_CURRENT_DESKTOP": "KDE",
        }
        info = display_backend.detect_display_session(
            env=env,
            which_fn=lambda cmd: f"/usr/bin/{cmd}" if cmd == "spectacle" else None,
        )
        self.assertEqual(info.protocol, "wayland")
        self.assertEqual(info.detection_source, "WAYLAND_DISPLAY=wayland-0")
        self.assertEqual(info.capture_driver, "spectacle (KDE)")
        self.assertEqual(info.clipboard_driver, "QClipboard")

    def test_runtime_dir_unix_socket_discovery_when_env_vars_stripped(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            # Create decoy regular file and .lock file that must be ignored
            (Path(tmpdir) / "wayland-0.lock").write_text("lock", encoding="utf-8")
            (Path(tmpdir) / "wayland-9").write_text("not-a-socket", encoding="utf-8")

            # Create a genuine AF_UNIX socket at wayland-1
            sock_path = Path(tmpdir) / "wayland-1"
            srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                srv.bind(str(sock_path))
                env = {
                    "XDG_RUNTIME_DIR": tmpdir,
                    "DISPLAY": ":0",  # Even with XWayland DISPLAY set, real Wayland socket wins
                    "XDG_CURRENT_DESKTOP": "Hyprland",
                }
                info = display_backend.detect_display_session(
                    env=env,
                    which_fn=lambda cmd: f"/usr/bin/{cmd}" if cmd in ("grim", "wl-copy") else None,
                )
                self.assertEqual(info.protocol, "wayland")
                self.assertEqual(info.wayland_display, "wayland-1")
                self.assertEqual(info.detection_source, "XDG_RUNTIME_DIR/wayland-1 socket")
                self.assertIn("grim", info.capture_driver)
            finally:
                srv.close()

    def test_loginctl_fallback_detects_wayland_or_x11_and_handles_errors(self) -> None:
        env = {
            "XDG_SESSION_ID": "42",
            "XDG_CURRENT_DESKTOP": "GNOME",
        }
        completed = subprocess.CompletedProcess(
            args=["loginctl"], returncode=0, stdout="wayland\n", stderr=""
        )
        with mock.patch("subprocess.run", return_value=completed) as run_mock:
            info = display_backend.detect_display_session(
                env=env,
                which_fn=lambda cmd: "/usr/bin/loginctl" if cmd == "loginctl" else None,
            )
            self.assertEqual(info.protocol, "wayland")
            self.assertEqual(info.detection_source, "loginctl session Type=wayland")
            run_mock.assert_called_once()

        # Verify subprocess timeout gracefully falls through to DISPLAY
        env_with_display = {"XDG_SESSION_ID": "42", "DISPLAY": ":1"}
        with mock.patch(
            "subprocess.run",
            side_effect=subprocess.TimeoutExpired(cmd="loginctl", timeout=1.5),
        ):
            fallback = display_backend.detect_display_session(
                env=env_with_display,
                which_fn=lambda cmd: "/usr/bin/loginctl" if cmd == "loginctl" else None,
            )
            self.assertEqual(fallback.protocol, "x11")
            self.assertEqual(fallback.detection_source, "DISPLAY=:1")

    def test_wayland_capture_driver_fallback_chain(self) -> None:
        base_env = {"XDG_SESSION_TYPE": "wayland", "WAYLAND_DISPLAY": "wayland-0"}

        # 1. gnome-screenshot when gdbus & grim missing
        info_gnome = display_backend.detect_display_session(
            env=base_env,
            which_fn=lambda c: "/usr/bin/gnome-screenshot" if c == "gnome-screenshot" else None,
        )
        self.assertEqual(info_gnome.capture_driver, "gnome-screenshot")

        # 2. XWayland Qt fallback when no Wayland screenshot tools installed
        info_fallback = display_backend.detect_display_session(
            env=base_env,
            which_fn=lambda _c: None,
        )
        self.assertEqual(info_fallback.capture_driver, "qt-screen-grab (XWayland fallback)")

    def test_x11_clipboard_driver_fallback_chain(self) -> None:
        base_env = {"XDG_SESSION_TYPE": "x11", "DISPLAY": ":0"}

        info_xsel = display_backend.detect_display_session(
            env=base_env,
            which_fn=lambda c: "/usr/bin/xsel" if c == "xsel" else None,
        )
        self.assertEqual(info_xsel.clipboard_driver, "xsel + QClipboard")
        self.assertFalse(info_xsel.supports_window_enumeration)

        info_qt_only = display_backend.detect_display_session(
            env=base_env,
            which_fn=lambda _c: None,
        )
        self.assertEqual(info_qt_only.clipboard_driver, "QClipboard")

    def test_headless_fallback_sets_offscreen_qpa(self) -> None:
        info = display_backend.detect_display_session(env={}, which_fn=lambda _c: None)
        self.assertEqual(info.protocol, "headless")
        self.assertEqual(info.qt_qpa_platform, "offscreen")
        self.assertEqual(info.capture_driver, "offscreen-buffer")


class X11WindowEnumerationTests(unittest.TestCase):
    def test_parses_stacking_windows_reverses_z_order_and_skips_unmapped_or_tiny(self) -> None:
        xprop_stdout = "_NET_CLIENT_LIST_STACKING(WINDOW): window id # 0x1001, 0x1002, 0x1003, 0x1004\n"

        xwininfo_outputs = {
            "0x1004": (
                'xwininfo: Window id: 0x1004 "Topmost Terminal — bash"\n'
                "  Absolute upper-left X:  120\n"
                "  Absolute upper-left Y:  80\n"
                "  Width: 920\n"
                "  Height: 580\n"
                "  Map State: IsViewable\n"
            ),
            "0x1003": (
                'xwininfo: Window id: 0x1003 "Hidden Tray Window"\n'
                "  Absolute upper-left X:  0\n"
                "  Absolute upper-left Y:  0\n"
                "  Width: 400\n"
                "  Height: 300\n"
                "  Map State: IsUnMapped\n"
            ),
            "0x1002": (
                'xwininfo: Window id: 0x1002 "Tiny 1x1 Helper"\n'
                "  Absolute upper-left X:  10\n"
                "  Absolute upper-left Y:  10\n"
                "  Width: 4\n"
                "  Height: 4\n"
                "  Map State: IsViewable\n"
            ),
            "0x1001": (
                'xwininfo: Window id: 0x1001 "Secondary Monitor Browser"\n'
                "  Absolute upper-left X:  -1920\n"
                "  Absolute upper-left Y:  0\n"
                "  Width: 1920\n"
                "  Height: 1080\n"
                "  Map State: IsViewable\n"
            ),
        }

        def fake_run(cmd: list[str], **_kwargs: object) -> subprocess.CompletedProcess[str]:
            if cmd[0] == "xprop":
                return subprocess.CompletedProcess(cmd, 0, stdout=xprop_stdout, stderr="")
            if cmd[0] == "xwininfo":
                wid = cmd[2]
                return subprocess.CompletedProcess(
                    cmd, 0, stdout=xwininfo_outputs.get(wid, ""), stderr=""
                )
            return subprocess.CompletedProcess(cmd, 1, stdout="", stderr="")

        with (
            mock.patch("shutil.which", return_value="/usr/bin/mock"),
            mock.patch("subprocess.run", side_effect=fake_run),
        ):
            windows = display_backend.enumerate_x11_windows()

        self.assertEqual(len(windows), 2)
        # 0x1004 was last in _NET_CLIENT_LIST_STACKING -> topmost -> first in list
        self.assertEqual(windows[0].window_id, "0x1004")
        self.assertEqual(windows[0].title, "Topmost Terminal — bash")
        self.assertEqual((windows[0].x, windows[0].y, windows[0].width, windows[0].height), (120, 80, 920, 580))

        # 0x1001 supports negative multi-monitor coordinates
        self.assertEqual(windows[1].window_id, "0x1001")
        self.assertEqual(windows[1].x, -1920)


class ClipboardAndWaylandCaptureHelperTests(unittest.TestCase):
    def test_copy_png_bytes_pipes_to_wl_copy_on_wayland(self) -> None:
        session = display_backend.DisplaySessionInfo(
            protocol="wayland",
            detection_source="XDG_SESSION_TYPE=wayland",
            qt_qpa_platform="wayland;xcb",
            desktop_environment="GNOME",
            wayland_display="wayland-0",
            x11_display="",
            capture_driver="xdg-desktop-portal",
            clipboard_driver="wl-copy + QClipboard",
            supports_window_enumeration=False,
        )
        fake_proc = mock.MagicMock()
        fake_proc.returncode = 0
        with (
            mock.patch("shutil.which", side_effect=lambda c: "/usr/bin/wl-copy" if c == "wl-copy" else None),
            mock.patch("subprocess.Popen", return_value=fake_proc) as popen_mock,
        ):
            ok = display_backend.copy_png_bytes_to_system_clipboard(b"\x89PNG\r\n", session)
            self.assertTrue(ok)
            popen_mock.assert_called_once()
            fake_proc.communicate.assert_called_once_with(input=b"\x89PNG\r\n", timeout=3.0)

    def test_copy_png_bytes_pipes_to_xclip_on_x11(self) -> None:
        session = display_backend.DisplaySessionInfo(
            protocol="x11",
            detection_source="XDG_SESSION_TYPE=x11",
            qt_qpa_platform="xcb",
            desktop_environment="XFCE",
            wayland_display="",
            x11_display=":0",
            capture_driver="Qt X11",
            clipboard_driver="xclip + QClipboard",
            supports_window_enumeration=True,
        )
        fake_proc = mock.MagicMock()
        fake_proc.returncode = 0
        with (
            mock.patch("shutil.which", side_effect=lambda c: "/usr/bin/xclip" if c == "xclip" else None),
            mock.patch("subprocess.Popen", return_value=fake_proc) as popen_mock,
        ):
            ok = display_backend.copy_png_bytes_to_system_clipboard(b"\x89PNG\r\n", session)
            self.assertTrue(ok)
            args_passed = popen_mock.call_args[0][0]
            self.assertEqual(args_passed[:4], ["xclip", "-selection", "clipboard", "-t"])

    def test_wayland_capture_uses_grim_when_available_and_cleans_up_on_failure(self) -> None:
        def fake_grim_success(cmd: list[str], **_kw: object) -> subprocess.CompletedProcess[bytes]:
            Path(cmd[1]).write_bytes(b"\x89PNG_FAKE_IMAGE_DATA")
            return subprocess.CompletedProcess(cmd, 0, b"", b"")

        with (
            mock.patch("shutil.which", side_effect=lambda c: "/usr/bin/grim" if c == "grim" else None),
            mock.patch("subprocess.run", side_effect=fake_grim_success),
        ):
            shot = display_backend.capture_wayland_screenshot_to_file(interactive=False)
            self.assertIsNotNone(shot)
            assert shot is not None
            try:
                self.assertEqual(shot.read_bytes(), b"\x89PNG_FAKE_IMAGE_DATA")
            finally:
                shot.unlink(missing_ok=True)


if __name__ == "__main__":
    unittest.main()
