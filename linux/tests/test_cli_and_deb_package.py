"""End-to-end CLI, startup QT_QPA_PLATFORM configuration, and built .deb binary archive tests."""

from __future__ import annotations

import gzip
import hashlib
import io
import json
import os
import stat
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest import mock

ROOT_DIR = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT_DIR / "linux"))

from snipclip import cli, version  # noqa: E402


class CliAndDebianPackageTests(unittest.TestCase):
    def test_arg_parser_modes_and_delays(self) -> None:
        parser = cli.build_arg_parser()
        parsed = parser.parse_args(["--mode", "window", "--delay", "5", "--capture-now"])
        self.assertEqual(parsed.mode, "window")
        self.assertEqual(parsed.delay, 5)
        self.assertTrue(parsed.capture_now)

        with self.assertRaises(SystemExit):
            parser.parse_args(["--mode", "invalid-mode"])

        with self.assertRaises(SystemExit):
            parser.parse_args(["--delay", "7"])

    def test_cli_sets_qt_qpa_platform_on_startup_without_overwriting_user_override(self) -> None:
        # Verify Wayland startup sets QT_QPA_PLATFORM="wayland;xcb" prior to GUI import
        clean_env = {
            "XDG_SESSION_TYPE": "wayland",
            "WAYLAND_DISPLAY": "wayland-0",
        }
        with (
            mock.patch.dict(os.environ, clean_env, clear=True),
            mock.patch.dict(sys.modules, {"PyQt6": None, "PyQt6.QtWidgets": None}),
            mock.patch("sys.stderr", new_callable=io.StringIO),
        ):
            rc = cli.main([])
            self.assertEqual(rc, 1)  # Exits cleanly with code 1 when PyQt6 not installed in test env
            self.assertEqual(os.environ.get("QT_QPA_PLATFORM"), "wayland;xcb")

        # Verify explicit user QT_QPA_PLATFORM is respected
        override_env = {
            "XDG_SESSION_TYPE": "wayland",
            "WAYLAND_DISPLAY": "wayland-0",
            "QT_QPA_PLATFORM": "xcb",
        }
        with (
            mock.patch.dict(os.environ, override_env, clear=True),
            mock.patch.dict(sys.modules, {"PyQt6": None, "PyQt6.QtWidgets": None}),
            mock.patch("sys.stderr", new_callable=io.StringIO),
        ):
            cli.main([])
            self.assertEqual(os.environ.get("QT_QPA_PLATFORM"), "xcb")

    def test_deb_package_metadata_checksum_and_extracted_binary_execution(self) -> None:
        expected_version = (ROOT_DIR / "VERSION").read_text(encoding="utf-8").strip()
        deb_filename = f"snipclip_{expected_version}_all.deb"
        deb_path = ROOT_DIR / "public" / "releases" / deb_filename
        sums_path = ROOT_DIR / "public" / "releases" / "SHA256SUMS"
        release_json_path = ROOT_DIR / "src" / "data" / "releaseInfo.json"

        self.assertTrue(deb_path.exists(), f"Expected built .deb at {deb_path}")
        self.assertTrue(sums_path.exists(), "Expected SHA256SUMS alongside .deb")
        self.assertTrue(release_json_path.exists(), "Expected src/data/releaseInfo.json")

        # 1. Verify SHA256SUMS and releaseInfo.json match actual .deb digest and byte size
        actual_sha256 = hashlib.sha256(deb_path.read_bytes()).hexdigest()
        sums_line = sums_path.read_text(encoding="utf-8").strip()
        self.assertEqual(sums_line, f"{actual_sha256}  {deb_filename}")

        release_info = json.loads(release_json_path.read_text(encoding="utf-8"))
        self.assertEqual(release_info["version"], expected_version)
        self.assertEqual(release_info["debFilename"], deb_filename)
        self.assertEqual(release_info["sha256"], actual_sha256)
        self.assertEqual(release_info["sizeBytes"], deb_path.stat().st_size)

        # 2. Verify dpkg-deb control fields
        pkg_name = subprocess.check_output(["dpkg-deb", "-f", str(deb_path), "Package"], text=True).strip()
        pkg_ver = subprocess.check_output(["dpkg-deb", "-f", str(deb_path), "Version"], text=True).strip()
        pkg_depends = subprocess.check_output(["dpkg-deb", "-f", str(deb_path), "Depends"], text=True).strip()
        pkg_recommends = subprocess.check_output(["dpkg-deb", "-f", str(deb_path), "Recommends"], text=True).strip()

        self.assertEqual(pkg_name, "snipclip")
        self.assertEqual(pkg_ver, expected_version)
        self.assertIn("python3-pyqt6", pkg_depends)
        self.assertIn("qt6-wayland", pkg_recommends)
        self.assertIn("xdg-desktop-portal", pkg_recommends)

        # 3. Extract .deb into an isolated rootfs and execute installed launcher script
        with tempfile.TemporaryDirectory() as rootfs:
            subprocess.check_call(["dpkg-deb", "-x", str(deb_path), rootfs])
            root = Path(rootfs)

            bin_launcher = root / "usr" / "bin" / "snipclip"
            desktop_file = root / "usr" / "share" / "applications" / "snipclip.desktop"
            svg_icon = root / "usr" / "share" / "icons" / "hicolor" / "scalable" / "apps" / "snipclip.svg"
            changelog_gz = root / "usr" / "share" / "doc" / "snipclip" / "changelog.Debian.gz"
            installed_geom = root / "usr" / "lib" / "snipclip" / "snipclip" / "geometry.py"

            self.assertTrue(bin_launcher.exists())
            self.assertTrue(bool(bin_launcher.stat().st_mode & stat.S_IXUSR))
            self.assertTrue(desktop_file.exists())
            self.assertTrue(svg_icon.exists())
            self.assertTrue(changelog_gz.exists())
            self.assertTrue(installed_geom.exists())

            desktop_text = desktop_file.read_text(encoding="utf-8")
            self.assertIn("Actions=RectSnip;WindowSnip;FullSnip;", desktop_text)

            with gzip.open(changelog_gz, "rt", encoding="utf-8") as gz_f:
                changelog_text = gz_f.read()
            self.assertIn(f"snipclip ({expected_version})", changelog_text)

            # Execute extracted /usr/bin/snipclip by pointing sys.path to extracted usr/lib/snipclip
            py_env = os.environ.copy()
            py_env["PYTHONPATH"] = str(root / "usr" / "lib" / "snipclip")
            py_env["XDG_SESSION_TYPE"] = "wayland"
            py_env["WAYLAND_DISPLAY"] = "wayland-0"
            py_env["XDG_CURRENT_DESKTOP"] = "GNOME"

            ver_out = subprocess.check_output(
                [sys.executable, "-m", "snipclip.cli", "--version"],
                env=py_env,
                text=True,
            ).strip()
            self.assertEqual(
                ver_out,
                f"SnipClip {expected_version} ({deb_filename}, release v{expected_version})",
            )

            detect_out = subprocess.check_output(
                [sys.executable, "-m", "snipclip.cli", "--detect-display"],
                env=py_env,
                text=True,
            )
            detect_json = json.loads(detect_out)
            self.assertEqual(detect_json["protocol"], "wayland")
            self.assertEqual(detect_json["detection_source"], "XDG_SESSION_TYPE=wayland")


if __name__ == "__main__":
    unittest.main()
