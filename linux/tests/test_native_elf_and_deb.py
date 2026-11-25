"""Integration tests for the natively compiled GTK 3 ELF binary and cross-distro .deb archive."""

from __future__ import annotations

import gzip
import hashlib
import json
import os
import re
import shutil
import signal
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

CANONICAL_HOMEPAGE = "https://github.com/ddastoor/SnipClip"
CANONICAL_DEVELOPER_ID = "io.github.ddastoor"
FORBIDDEN_LEGACY_METADATA = ("snipclip/snipclip", "maintainers@snipclip.org")

# Runs INSIDE an `xvfb-run -a` session: one X11 client grabs XK_Print (0xff61) on the root
# window with AnyModifier, then launches SnipClip (argv[1]) so its own XGrabKey hits BadAccess.
# Prints a single JSON line describing the outcome.
X11_HOTKEY_COLLISION_HELPER = r"""
import ctypes, json, os, subprocess, sys, time
xlib = ctypes.CDLL("libX11.so.6")
xlib.XOpenDisplay.argtypes = [ctypes.c_char_p]
xlib.XOpenDisplay.restype = ctypes.c_void_p
xlib.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
xlib.XDefaultRootWindow.restype = ctypes.c_ulong
xlib.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
xlib.XKeysymToKeycode.restype = ctypes.c_ubyte
xlib.XGrabKey.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_uint, ctypes.c_ulong,
                          ctypes.c_int, ctypes.c_int, ctypes.c_int]
xlib.XGrabKey.restype = ctypes.c_int
xlib.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
xlib.XSync.restype = ctypes.c_int
xlib.XCloseDisplay.argtypes = [ctypes.c_void_p]
xlib.XCloseDisplay.restype = ctypes.c_int
ERRS = []
HANDLER_T = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
def _on_error(_dpy, _ev):
    ERRS.append(1)
    return 0
handler = HANDLER_T(_on_error)
xlib.XSetErrorHandler.argtypes = [HANDLER_T]
xlib.XSetErrorHandler.restype = ctypes.c_void_p
xlib.XSetErrorHandler(handler)
dpy = xlib.XOpenDisplay(None)
if not dpy:
    print(json.dumps({"error": "XOpenDisplay failed"}))
    sys.exit(3)
root = xlib.XDefaultRootWindow(dpy)
keycode = xlib.XKeysymToKeycode(dpy, 0xff61)
ANY_MODIFIER, GRAB_MODE_ASYNC = 1 << 15, 1
xlib.XGrabKey(dpy, keycode, ANY_MODIFIER, root, 1, GRAB_MODE_ASYNC, GRAB_MODE_ASYNC)
xlib.XSync(dpy, 0)
env = dict(os.environ)
env.pop("WAYLAND_DISPLAY", None)
env["XDG_SESSION_TYPE"] = "x11"
env["GDK_BACKEND"] = "x11"
proc = subprocess.Popen([sys.argv[1]], env=env, stdin=subprocess.DEVNULL,
                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
alive = True
try:
    proc.wait(timeout=float(sys.argv[2]))
    alive = False
except subprocess.TimeoutExpired:
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
out, err = proc.communicate()
xlib.XCloseDisplay(dpy)
print(json.dumps({
    "keycode": int(keycode),
    "pre_grab_errors": len(ERRS),
    "alive_at_deadline": alive,
    "returncode": proc.returncode,
    "stderr": err[-4000:],
}))
"""


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
                ["bash", str(ROOT_DIR / "scripts" / "build-deb.sh"), cls.version],
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

                # 4. Explicit X11 DISPLAY overrides unreferenced Wayland socket when WAYLAND_DISPLAY is unset.
                #    PATH excludes loginctl so a developer's live Wayland login session cannot leak in.
                env_x11_override = {
                    "PATH": "/nonexistent",
                    "HOME": rt_dir,
                    "XDG_RUNTIME_DIR": rt_dir,
                    "DISPLAY": ":99",
                }
                out_x11_override = json.loads(
                    subprocess.check_output(
                        [str(self.packaged_bin), "--detect-display"],
                        env=env_x11_override,
                        text=True,
                    )
                )
                self.assertEqual(out_x11_override["protocol"], "x11")
                self.assertEqual(out_x11_override["x11_display"], ":99")
            finally:
                srv.close()

    def test_startup_window_matches_microsoft_snipping_tool_dimensions(self) -> None:
        """Verify live GTK 3 startup window opens at compact 460x200 Snipping Tool dimensions."""
        xvfb = shutil.which("xvfb-run")
        if not xvfb:
            self.skipTest("xvfb-run not installed in minimal container")

        metrics_out = subprocess.check_output(
            [
                xvfb,
                "-a",
                "-s",
                "-screen 0 1920x1080x24",
                str(self.packaged_bin),
                "--print-ui-metrics",
            ],
            text=True,
        )
        # Some user-space xvfb-run wrappers merge GTK/GDK stderr diagnostics into stdout;
        # the metrics contract is the single JSON object line.
        json_lines = [ln.strip() for ln in metrics_out.splitlines() if ln.strip().startswith("{")]
        self.assertTrue(json_lines, f"No --print-ui-metrics JSON in output: {metrics_out[-2000:]!r}")
        metrics = json.loads(json_lines[-1])
        self.assertEqual(metrics["launcher_default_w"], 460)
        self.assertEqual(metrics["launcher_default_h"], 200)
        self.assertLessEqual(
            metrics["launcher_min_w"],
            460,
            "Launcher toolbar minimum width must never stretch wider than 460px",
        )
        self.assertLessEqual(
            metrics["editor_min_w"],
            600,
            "Editor toolbar minimum width must stay compact and never span the screen",
        )

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

    def test_package_metadata_uses_canonical_ddastoor_snipclip_upstream(self) -> None:
        """Verify control, AppStream, copyright & changelog point at github.com/ddastoor/SnipClip."""
        control_text = (self.extract_dir / "DEBIAN" / "control").read_text(encoding="utf-8")
        self.assertIn(f"Homepage: {CANONICAL_HOMEPAGE}", control_text)
        self.assertIn("Maintainer: Darayus Dastoor <ddastoor@users.noreply.github.com>", control_text)

        metainfo_path = self.extract_dir / "usr" / "share" / "metainfo" / "snipclip.metainfo.xml"
        metainfo_text = metainfo_path.read_text(encoding="utf-8")
        self.assertIn(CANONICAL_HOMEPAGE, metainfo_text)
        self.assertIn(CANONICAL_DEVELOPER_ID, metainfo_text)
        root = ET.parse(metainfo_path).getroot()
        self.assertEqual(root.findtext("./url[@type='homepage']"), CANONICAL_HOMEPAGE)
        dev_el = root.find("./developer")
        self.assertIsNotNone(dev_el)
        self.assertEqual(dev_el.attrib.get("id"), CANONICAL_DEVELOPER_ID)

        doc_dir = self.extract_dir / "usr" / "share" / "doc" / "snipclip"
        copyright_text = (doc_dir / "copyright").read_text(encoding="utf-8")
        self.assertIn(f"Source: {CANONICAL_HOMEPAGE}", copyright_text)
        changelog_text = gzip.decompress((doc_dir / "changelog.Debian.gz").read_bytes()).decode("utf-8")
        self.assertIn("Darayus Dastoor <ddastoor@users.noreply.github.com>", changelog_text)

        for label, text in (
            ("DEBIAN/control", control_text),
            ("snipclip.metainfo.xml", metainfo_text),
            ("copyright", copyright_text),
            ("changelog.Debian.gz", changelog_text),
        ):
            for legacy in FORBIDDEN_LEGACY_METADATA:
                self.assertNotIn(legacy, text, f"Legacy metadata '{legacy}' found in {label}")

    def test_x11_global_hotkey_registration_handles_grab_collision_cleanly(self) -> None:
        """Pre-grab XK_Print in another X11 client; SnipClip's XGrabKey BadAccess must not crash it."""
        xvfb = shutil.which("xvfb-run")
        if not xvfb:
            self.skipTest("xvfb-run not installed in minimal container")

        with tempfile.TemporaryDirectory() as home_dir:
            env = dict(os.environ)
            env.update(
                {
                    "HOME": home_dir,
                    "XDG_CONFIG_HOME": str(Path(home_dir) / ".config"),
                    "XDG_CACHE_HOME": str(Path(home_dir) / ".cache"),
                    "NO_AT_BRIDGE": "1",
                }
            )
            proc = subprocess.run(
                [
                    xvfb,
                    "-a",
                    "-s",
                    "-screen 0 1920x1080x24",
                    "python3",
                    "-c",
                    X11_HOTKEY_COLLISION_HELPER,
                    str(self.packaged_bin),
                    "1.5",
                ],
                env=env,
                capture_output=True,
                text=True,
                timeout=90,
            )
        self.assertEqual(proc.returncode, 0, f"X11 collision helper failed: {proc.stderr}")
        json_lines = [ln for ln in proc.stdout.splitlines() if ln.startswith("{")]
        self.assertTrue(json_lines, f"No helper JSON output: {proc.stdout!r} {proc.stderr!r}")
        result = json.loads(json_lines[-1])

        self.assertNotIn("error", result)
        self.assertGreater(result["keycode"], 0, "XK_Print must map to a keycode under Xvfb")
        self.assertEqual(result["pre_grab_errors"], 0, "Helper's own pre-grab of XK_Print failed")

        stderr = result["stderr"]
        self.assertNotIn("X Error of failed request", stderr, f"Unhandled X11 error:\n{stderr}")
        self.assertNotIn("Gdk-ERROR", stderr, f"GDK aborted on X11 error:\n{stderr}")
        rc = result["returncode"]
        crash_codes = {
            -signal.SIGABRT,
            -signal.SIGSEGV,
            -signal.SIGTRAP,
            -signal.SIGBUS,
            128 + signal.SIGABRT,
            128 + signal.SIGSEGV,
            128 + signal.SIGTRAP,
        }
        self.assertNotIn(rc, crash_codes, f"SnipClip crashed (rc={rc}) on XGrabKey BadAccess:\n{stderr}")
        if not result["alive_at_deadline"]:
            # Exiting on its own before the deadline is only acceptable as a clean exit.
            self.assertEqual(rc, 0, f"SnipClip exited early (rc={rc}) on hotkey collision:\n{stderr}")


if __name__ == "__main__":
    unittest.main()
