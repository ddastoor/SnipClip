"""Integration tests for the natively compiled GTK 3 ELF binary and cross-distro .deb archive."""

from __future__ import annotations

import hashlib
import json
import os
import re
import socket
import stat
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[2]
VERSION_FILE = ROOT_DIR / "VERSION"
NATIVE_BIN = ROOT_DIR / "native" / "build" / "snipclip"
RELEASES_DIR = ROOT_DIR / "public" / "releases"
RELEASE_INFO_JSON = ROOT_DIR / "src" / "data" / "releaseInfo.json"


class NativeElfAndDebPackageTests(unittest.TestCase):
    """Verifies compiled C/GTK3 ELF binary, startup X11/Wayland detection, and multi-distro .deb."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.version = VERSION_FILE.read_text(encoding="utf-8").strip()
        cls.arch = subprocess.check_output(
            ["dpkg", "--print-architecture"], text=True
        ).strip()
        cls.deb_filename = f"snipclip_{cls.version}_{cls.arch}.deb"
        cls.deb_path = RELEASES_DIR / cls.deb_filename

        if not NATIVE_BIN.exists() or not cls.deb_path.exists():
            subprocess.run(
                [str(ROOT_DIR / "scripts" / "build-deb.sh"), cls.version],
                cwd=str(ROOT_DIR),
                check=True,
            )

        cls.extract_ctx = tempfile.TemporaryDirectory()
        cls.extract_dir = Path(cls.extract_ctx.name)
        subprocess.run(
            ["dpkg-deb", "--raw-extract", str(cls.deb_path), str(cls.extract_dir)],
            check=True,
        )
        cls.packaged_bin = cls.extract_dir / "usr" / "bin" / "snipclip"

    @classmethod
    def tearDownClass(cls) -> None:
        cls.extract_ctx.cleanup()

    def test_usr_bin_snipclip_is_compiled_elf64_and_contains_zero_python(self) -> None:
        """Verify /usr/bin/snipclip is a real compiled 64-bit ELF executable (not Python)."""
        self.assertTrue(self.packaged_bin.is_file())
        mode = self.packaged_bin.stat().st_mode
        self.assertTrue(bool(mode & stat.S_IXUSR), "/usr/bin/snipclip must be executable")

        raw = self.packaged_bin.read_bytes()
        # ELF magic: 0x7F 'E' 'L' 'F', class 2 = 64-bit
        self.assertEqual(raw[:4], b"\x7fELF", "/usr/bin/snipclip must start with ELF magic")
        self.assertEqual(raw[4], 2, "Expected 64-bit ELF binary (ELFCLASS64)")
        self.assertNotIn(b"#!/usr/bin/env python", raw[:128])

        # Ensure zero .py files exist anywhere inside the .deb payload
        py_files = list(self.extract_dir.rglob("*.py"))
        self.assertEqual(py_files, [], f"Expected zero Python files in native .deb, found {py_files}")
        self.assertFalse((self.extract_dir / "usr" / "lib" / "snipclip").exists())

    def test_dynamic_library_linkage_and_glibc_portability(self) -> None:
        """Verify GTK 3, Cairo, GdkPixbuf, GIO, and X11 shared libraries resolve cleanly."""
        ldd_out = subprocess.check_output(
            ["ldd", str(self.packaged_bin)], text=True
        )
        self.assertNotIn("not found", ldd_out)
        for required_soname in (
            "libgtk-3.so.0",
            "libcairo.so.2",
            "libgdk_pixbuf-2.0.so.0",
            "libgio-2.0.so.0",
            "libX11.so.6",
        ):
            self.assertIn(required_soname, ldd_out)

        # Check GLIBC symbol version ceiling when built on Debian 12 Bookworm
        objdump_out = subprocess.check_output(
            ["objdump", "-T", str(self.packaged_bin)], text=True
        )
        glibc_versions = {
            tuple(int(part) for part in m.split("."))
            for m in re.findall(r"GLIBC_([0-9]+\.[0-9]+(?:\.[0-9]+)?)", objdump_out)
        }
        self.assertTrue(len(glibc_versions) > 0)
        # Host libc version check
        host_ldd_ver = subprocess.check_output(["ldd", "--version"], text=True).splitlines()[0]
        if "2.36" in host_ldd_ver:
            # Built on Debian 12 Bookworm -> max symbol must be <= GLIBC_2.36 so Ubuntu 22.04/24.04/25.04 & Debian 12+ all work
            self.assertLessEqual(max(glibc_versions), (2, 36))

    def test_cli_version_and_help_exit_cleanly_including_percent_f_regression(self) -> None:
        """Verify --version and --help exit 0 cleanly with zero format-string crashes."""
        ver_proc = subprocess.run(
            [str(self.packaged_bin), "--version"],
            capture_output=True,
            text=True,
            check=True,
        )
        self.assertIn(f"SnipClip {self.version}", ver_proc.stdout)
        self.assertIn(self.deb_filename, ver_proc.stdout)

        help_proc = subprocess.run(
            [str(self.packaged_bin), "--help"],
            capture_output=True,
            text=True,
            check=True,
        )
        self.assertIn("usage: snipclip", help_proc.stdout)
        self.assertIn("--detect-display", help_proc.stdout)
        self.assertIn("PrintScreen", help_proc.stdout)

    def test_binary_auto_detects_wayland_x11_and_stripped_gui_socket_sessions(self) -> None:
        """Execute compiled ELF binary across Wayland, X11, XWayland, and stripped GUI environments."""
        # 1. GNOME Wayland session
        env_wayland = {
            "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
            "XDG_SESSION_TYPE": "wayland",
            "WAYLAND_DISPLAY": "wayland-0",
            "DISPLAY": ":0",
            "XDG_CURRENT_DESKTOP": "ubuntu:GNOME",
        }
        out_wayland = json.loads(
            subprocess.check_output(
                [str(self.packaged_bin), "--detect-display"],
                env=env_wayland,
                text=True,
            )
        )
        self.assertEqual(out_wayland["protocol"], "wayland")
        self.assertEqual(out_wayland["gdk_backend"], "wayland,x11")
        self.assertEqual(out_wayland["detection_source"], "XDG_SESSION_TYPE=wayland")
        self.assertEqual(out_wayland["desktop_environment"], "ubuntu:GNOME")

        # 2. XFCE / Cinnamon X11 session
        env_x11 = {
            "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
            "XDG_SESSION_TYPE": "x11",
            "WAYLAND_DISPLAY": "",
            "DISPLAY": ":0.0",
            "XDG_CURRENT_DESKTOP": "XFCE",
        }
        out_x11 = json.loads(
            subprocess.check_output(
                [str(self.packaged_bin), "--detect-display"],
                env=env_x11,
                text=True,
            )
        )
        self.assertEqual(out_x11["protocol"], "x11")
        self.assertEqual(out_x11["gdk_backend"], "x11")
        self.assertTrue(out_x11["supports_window_enumeration"])
        self.assertIn("libX11", out_x11["capture_driver"])

        # 3. Stripped GUI launcher environment with only a live Wayland UNIX socket in XDG_RUNTIME_DIR
        with tempfile.TemporaryDirectory() as rt_dir:
            sock_path = Path(rt_dir) / "wayland-0"
            srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                srv.bind(str(sock_path))
                env_stripped = {
                    "PATH": "/nonexistent",
                    "HOME": rt_dir,
                    "XDG_RUNTIME_DIR": rt_dir,
                }
                out_sock = json.loads(
                    subprocess.check_output(
                        [str(self.packaged_bin), "--detect-display"],
                        env=env_stripped,
                        text=True,
                    )
                )
                self.assertEqual(out_sock["protocol"], "wayland")
                self.assertEqual(out_sock["wayland_display"], "wayland-0")
                self.assertIn("socket", out_sock["detection_source"])
            finally:
                srv.close()

    def test_deb_archive_uses_xz_compression_for_cross_distro_gui_installers(self) -> None:
        """Ensure .deb uses xz (never zstd) so Debian 12, KDE Discover, GNOME Software & GDebi never hang."""
        raw_deb = self.deb_path.read_bytes()
        self.assertEqual(raw_deb[:8], b"!<arch>\n")

        ar_members = subprocess.check_output(
            ["ar", "t", str(self.deb_path)], text=True
        ).splitlines()
        self.assertEqual(ar_members[0], "debian-binary")
        self.assertIn("control.tar.xz", ar_members)
        self.assertIn("data.tar.xz", ar_members)
        for member in ar_members:
            self.assertFalse(member.endswith(".zst"), f"Forbidden zstd member {member}")

    def test_debian_control_supports_both_t64_and_pre_t64_debian_ubuntu_distros(self) -> None:
        """Verify DEBIAN/control handles Ubuntu 24.04+/Debian 13 (t64) AND Debian 12/Ubuntu 22.04."""
        control_text = (self.extract_dir / "DEBIAN" / "control").read_text(encoding="utf-8")
        self.assertIn(f"Version: {self.version}", control_text)
        self.assertIn(f"Architecture: {self.arch}", control_text)
        self.assertIn("libgtk-3-0t64 | libgtk-3-0", control_text)
        self.assertIn("libglib2.0-0t64 | libglib2.0-0", control_text)
        self.assertIn("libcairo2", control_text)
        self.assertIn("libx11-6", control_text)
        self.assertNotIn("python3-pyqt6", control_text)

    def test_md5sums_postinst_appstream_and_launch_menu_entries(self) -> None:
        """Verify md5sums, non-blocking postinst/postrm, AppStream XML, .desktop, and icons."""
        md5_file = self.extract_dir / "DEBIAN" / "md5sums"
        self.assertTrue(md5_file.is_file())
        lines = [ln.strip() for ln in md5_file.read_text(encoding="utf-8").splitlines() if ln.strip()]
        self.assertGreaterEqual(len(lines), 10)

        for line in lines:
            expected_md5, rel_path = line.split(None, 1)
            target = self.extract_dir / rel_path
            self.assertTrue(target.is_file(), f"Missing file listed in md5sums: {rel_path}")
            actual_md5 = hashlib.md5(target.read_bytes()).hexdigest()
            self.assertEqual(actual_md5, expected_md5, f"MD5 mismatch for {rel_path}")

        # Verify postinst & postrm close stdin (</dev/null) so PackageKit / KDE Discover never hangs
        for script_name in ("postinst", "postrm"):
            script_path = self.extract_dir / "DEBIAN" / script_name
            self.assertTrue(bool(script_path.stat().st_mode & stat.S_IXUSR))
            content = script_path.read_text(encoding="utf-8")
            self.assertIn("</dev/null", content)

        # Parse AppStream MetaInfo XML
        metainfo = self.extract_dir / "usr" / "share" / "metainfo" / "snipclip.metainfo.xml"
        tree = ET.parse(metainfo)
        root = tree.getroot()
        self.assertEqual(root.findtext("id"), "snipclip.desktop")
        rel_el = root.find("./releases/release")
        self.assertIsNotNone(rel_el)
        self.assertEqual(rel_el.attrib.get("version"), self.version)

        # Verify Freedesktop .desktop & multi-size icons
        desktop_text = (
            self.extract_dir / "usr" / "share" / "applications" / "snipclip.desktop"
        ).read_text(encoding="utf-8")
        self.assertIn("Exec=snipclip %f", desktop_text)
        self.assertIn("Terminal=false", desktop_text)
        self.assertIn("GTK;", desktop_text)
        self.assertIn("Utility;", desktop_text)
        self.assertIn("Graphics;", desktop_text)

        for sz in (16, 24, 32, 48, 64, 128, 256):
            icon = (
                self.extract_dir
                / "usr"
                / "share"
                / "icons"
                / "hicolor"
                / f"{sz}x{sz}"
                / "apps"
                / "snipclip.png"
            )
            self.assertTrue(icon.is_file(), f"Missing {sz}x{sz} PNG icon")
            self.assertEqual(icon.read_bytes()[:8], b"\x89PNG\r\n\x1a\n")

        # Verify releaseInfo.json matches built .deb SHA256
        rel_info = json.loads(RELEASE_INFO_JSON.read_text(encoding="utf-8"))
        self.assertEqual(rel_info["version"], self.version)
        self.assertEqual(rel_info["debFilename"], self.deb_filename)
        actual_sha256 = hashlib.sha256(self.deb_path.read_bytes()).hexdigest()
        self.assertEqual(rel_info["sha256"], actual_sha256)


if __name__ == "__main__":
    unittest.main()
