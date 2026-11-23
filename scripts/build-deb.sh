#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

RAW_VERSION="${1:-${GITHUB_REF_NAME:-}}"
if [[ -z "${RAW_VERSION}" ]]; then
  RAW_VERSION="$(tr -d '[:space:]' < "${ROOT_DIR}/VERSION")"
fi

# Strip optional leading 'v' from GitHub release tag (e.g., v2.0.0 -> 2.0.0)
CLEAN_VERSION="${RAW_VERSION#v}"

if ! [[ "${CLEAN_VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+([.-][0-9A-Za-z]+)*$ ]]; then
  echo "Error: Invalid Debian package version '${CLEAN_VERSION}' (derived from '${RAW_VERSION}')" >&2
  exit 1
fi

PKG_NAME="snipclip"
ARCH="${DEB_ARCH:-$(dpkg --print-architecture 2>/dev/null || echo amd64)}"
DEB_FILENAME="${PKG_NAME}_${CLEAN_VERSION}_${ARCH}.deb"
RELEASE_TAG="v${CLEAN_VERSION}"
RELEASE_DATE_YMD="$(date -u +"%Y-%m-%d")"

# Cross-distro runtime dependencies covering both pre-t64 (Debian 12, Ubuntu 22.04, Mint 21,
# MX Linux 23, Zorin 17) and t64 (Debian 13+, Ubuntu 24.04/24.10/25.04, Mint 22, Pop!_OS 24.04, Kali)
DEPENDS_LIST="libc6 (>= 2.35), libgtk-3-0t64 | libgtk-3-0, libglib2.0-0t64 | libglib2.0-0, libcairo2, libx11-6"
RECOMMENDS_LIST="xdg-desktop-portal, xdg-desktop-portal-gtk | xdg-desktop-portal-kde | xdg-desktop-portal-gnome, wl-clipboard, xclip, grim, desktop-file-utils, shared-mime-info"

# 1. Sync VERSION and native C header so compiled ELF binary matches GitHub .deb release 1:1
printf "%s\n" "${CLEAN_VERSION}" > "${ROOT_DIR}/VERSION"

cat > "${ROOT_DIR}/native/include/snipclip_version.h" <<EOF
#ifndef SNIPCLIP_VERSION_H
#define SNIPCLIP_VERSION_H

#define SNIPCLIP_NAME "SnipClip"
#define SNIPCLIP_PKG_NAME "${PKG_NAME}"
#define SNIPCLIP_VERSION "${CLEAN_VERSION}"
#define SNIPCLIP_DEB_ARCH "${ARCH}"
#define SNIPCLIP_DEB_FILENAME "${DEB_FILENAME}"
#define SNIPCLIP_RELEASE_TAG "${RELEASE_TAG}"

#endif /* SNIPCLIP_VERSION_H */
EOF

# 2. Compile native C11 + GTK 3 ELF executable and run native C unit tests
make -C "${ROOT_DIR}/native" clean all
make -C "${ROOT_DIR}/native" test

# 3. Stage Debian package filesystem hierarchy
STAGE_DIR="$(mktemp -d)"
trap 'rm -rf "${STAGE_DIR}"' EXIT

mkdir -p \
  "${STAGE_DIR}/DEBIAN" \
  "${STAGE_DIR}/usr/bin" \
  "${STAGE_DIR}/usr/share/applications" \
  "${STAGE_DIR}/usr/share/metainfo" \
  "${STAGE_DIR}/usr/share/menu" \
  "${STAGE_DIR}/usr/share/icons/hicolor/scalable/apps" \
  "${STAGE_DIR}/usr/share/pixmaps" \
  "${STAGE_DIR}/usr/share/doc/snipclip"

# Install compiled, stripped native ELF binary
install -m 0755 "${ROOT_DIR}/native/build/snipclip" "${STAGE_DIR}/usr/bin/snipclip"

# Copy Freedesktop .desktop entry, Debian menu entry, scalable SVG, and multi-size raster PNGs
cp "${ROOT_DIR}/packaging/snipclip.desktop" "${STAGE_DIR}/usr/share/applications/snipclip.desktop"
chmod 0644 "${STAGE_DIR}/usr/share/applications/snipclip.desktop"

cp "${ROOT_DIR}/packaging/snipclip.menu" "${STAGE_DIR}/usr/share/menu/snipclip"
chmod 0644 "${STAGE_DIR}/usr/share/menu/snipclip"

cp "${ROOT_DIR}/packaging/snipclip.svg" "${STAGE_DIR}/usr/share/icons/hicolor/scalable/apps/snipclip.svg"
chmod 0644 "${STAGE_DIR}/usr/share/icons/hicolor/scalable/apps/snipclip.svg"

python3 "${ROOT_DIR}/scripts/generate_icons.py" "${STAGE_DIR}"

# Generate AppStream MetaInfo XML for KDE Discover and GNOME Software GUI installers
cat > "${STAGE_DIR}/usr/share/metainfo/snipclip.metainfo.xml" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>snipclip.desktop</id>
  <metadata_license>CC0-1.0</metadata_license>
  <project_license>MIT</project_license>
  <name>SnipClip</name>
  <summary>Native GTK 3 Windows Snipping Tool clone for Debian-based Linux (X11 &amp; Wayland)</summary>
  <developer id="io.github.snipclip">
    <name>SnipClip Contributors</name>
  </developer>
  <developer_name>SnipClip Contributors</developer_name>
  <description>
    <p>
      SnipClip is a natively compiled GTK 3 + Cairo screenshot and annotation studio bringing the
      Windows 11 Snipping Tool workflow to Debian, Ubuntu, Kubuntu, Linux Mint, Pop!_OS, Kali,
      Zorin OS, MX Linux, and derivatives. At GUI startup it automatically detects X11 or Wayland
      and configures native GDBus XDG Desktop Portal or libX11 screen capture and clipboard support.
    </p>
    <p>
      Includes Rectangular, Window, Fullscreen, and Freeform lasso snips, 0/3/5/10-second delay
      timers, PrintScreen and customizable shortcuts, Pen, Highlighter, Eraser, Shapes,
      graduated Ruler with straight-edge snapping, Redact/Pixelate, and Crop.
    </p>
  </description>
  <launchable type="desktop-id">snipclip.desktop</launchable>
  <provides>
    <binary>snipclip</binary>
  </provides>
  <categories>
    <category>Utility</category>
    <category>Graphics</category>
    <category>2DGraphics</category>
    <category>GTK</category>
  </categories>
  <url type="homepage">https://github.com/snipclip/snipclip</url>
  <content_rating type="oars-1.1" />
  <releases>
    <release version="${CLEAN_VERSION}" date="${RELEASE_DATE_YMD}" />
  </releases>
</component>
EOF
chmod 0644 "${STAGE_DIR}/usr/share/metainfo/snipclip.metainfo.xml"

# Debian copyright & compressed changelog
cat > "${STAGE_DIR}/usr/share/doc/snipclip/copyright" <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: snipclip
Source: https://github.com/snipclip/snipclip

Files: *
Copyright: 2026 SnipClip Contributors
License: MIT
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 .
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
EOF
chmod 0644 "${STAGE_DIR}/usr/share/doc/snipclip/copyright"

BUILD_DATE_RFC="$(date -R)"
cat > "${STAGE_DIR}/usr/share/doc/snipclip/changelog.Debian" <<EOF
snipclip (${CLEAN_VERSION}) stable; urgency=medium

  * Release natively compiled GTK 3 + Cairo SnipClip ${CLEAN_VERSION} (${RELEASE_TAG}).
  * Cross-distro Debian/Ubuntu support (t64 & pre-t64 GTK/GLib dependencies, glibc >= 2.35).
  * Automatic GUI startup X11 and Wayland detection via environment, UNIX sockets, and loginctl.
  * PackageKit/Discover xz archive format, AppStream metainfo, and XDG/Debian menu integration.

 -- SnipClip Maintainers <maintainers@snipclip.org>  ${BUILD_DATE_RFC}
EOF
gzip -9 -n "${STAGE_DIR}/usr/share/doc/snipclip/changelog.Debian"
chmod 0644 "${STAGE_DIR}/usr/share/doc/snipclip/changelog.Debian.gz"

INSTALLED_KB="$(du -sk "${STAGE_DIR}/usr" | awk '{print $1}')"

cat > "${STAGE_DIR}/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${CLEAN_VERSION}
Section: graphics
Priority: optional
Architecture: ${ARCH}
Installed-Size: ${INSTALLED_KB}
Depends: ${DEPENDS_LIST}
Recommends: ${RECOMMENDS_LIST}
Maintainer: SnipClip Contributors <maintainers@snipclip.org>
Homepage: https://github.com/snipclip/snipclip
Description: Native GTK 3 Windows Snipping Tool clone for Debian Linux (X11 & Wayland)
 SnipClip is a natively compiled C + GTK 3 + Cairo screenshot and annotation
 application for Debian, Ubuntu, Linux Mint, Pop!_OS, KDE Plasma, GNOME, XFCE,
 and derivatives. On GUI launch it automatically detects X11 or Wayland and
 provides Rectangular, Window, Fullscreen, and Freeform snips, delay timers,
 PrintScreen and custom shortcuts, Pen, Highlighter, Shapes, Ruler, Redact,
 Pixelate, and Crop with zero command-line setup.
EOF
chmod 0644 "${STAGE_DIR}/DEBIAN/control"

# Non-blocking PackageKit-safe postinst & postrm scripts to refresh desktop & icon launch menus immediately
cat > "${STAGE_DIR}/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e

if [ "$1" = "configure" ] || [ "$1" = "abort-upgrade" ] || [ "$1" = "abort-deconfigure" ] || [ "$1" = "abort-remove" ]; then
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database -q /usr/share/applications </dev/null >/dev/null 2>&1 || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f -t -q /usr/share/icons/hicolor </dev/null >/dev/null 2>&1 || true
    fi
    if command -v xdg-desktop-menu >/dev/null 2>&1; then
        xdg-desktop-menu forceupdate </dev/null >/dev/null 2>&1 || true
    fi
    if [ -x /usr/bin/update-menus ]; then
        /usr/bin/update-menus </dev/null >/dev/null 2>&1 || true
    fi
fi

exit 0
EOF
chmod 0755 "${STAGE_DIR}/DEBIAN/postinst"

cat > "${STAGE_DIR}/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e

if [ "$1" = "remove" ] || [ "$1" = "purge" ] || [ "$1" = "upgrade" ]; then
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database -q /usr/share/applications </dev/null >/dev/null 2>&1 || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f -t -q /usr/share/icons/hicolor </dev/null >/dev/null 2>&1 || true
    fi
    if command -v xdg-desktop-menu >/dev/null 2>&1; then
        xdg-desktop-menu forceupdate </dev/null >/dev/null 2>&1 || true
    fi
    if [ -x /usr/bin/update-menus ]; then
        /usr/bin/update-menus </dev/null >/dev/null 2>&1 || true
    fi
fi

exit 0
EOF
chmod 0755 "${STAGE_DIR}/DEBIAN/postrm"

# Generate standard DEBIAN/md5sums for PackageKit / GDebi / debsums verification
(
  cd "${STAGE_DIR}"
  find usr -type f -print0 | sort -z | xargs -0 md5sum > "${STAGE_DIR}/DEBIAN/md5sums"
)
chmod 0644 "${STAGE_DIR}/DEBIAN/md5sums"

# Ensure deterministic standard directory permissions inside .deb
find "${STAGE_DIR}" -type d -exec chmod 0755 {} +

OUT_DIR="${ROOT_DIR}/public/releases"
mkdir -p "${OUT_DIR}"
rm -f "${OUT_DIR}"/${PKG_NAME}_*.deb

DEB_PATH="${OUT_DIR}/${DEB_FILENAME}"
# Force -Zxz compression so Ubuntu 24.04+ never emits zstd .deb archives that hang Debian/Discover/GDebi
dpkg-deb -Zxz --uniform-compression --root-owner-group --build "${STAGE_DIR}" "${DEB_PATH}"

(
  cd "${OUT_DIR}"
  sha256sum "${DEB_FILENAME}" > SHA256SUMS
)

SHA256_HASH="$(awk '{print $1}' "${OUT_DIR}/SHA256SUMS")"
SHA256_LINE="$(tr -d '\r\n' < "${OUT_DIR}/SHA256SUMS")"
BYTE_SIZE="$(wc -c < "${DEB_PATH}" | tr -d '[:space:]')"
BUILT_AT_ISO="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
DEB_BASE64="$(base64 -w 0 "${DEB_PATH}")"

mkdir -p "${ROOT_DIR}/src/data"
cat > "${ROOT_DIR}/src/data/releaseInfo.json" <<EOF
{
  "packageName": "${PKG_NAME}",
  "version": "${CLEAN_VERSION}",
  "releaseTag": "${RELEASE_TAG}",
  "architecture": "${ARCH}",
  "debFilename": "${DEB_FILENAME}",
  "downloadPath": "/releases/${DEB_FILENAME}",
  "sha256": "${SHA256_HASH}",
  "sha256sumLine": "${SHA256_LINE}",
  "sizeBytes": ${BYTE_SIZE},
  "installedSizeKb": ${INSTALLED_KB},
  "builtAtIso": "${BUILT_AT_ISO}",
  "depends": "${DEPENDS_LIST}",
  "recommends": "${RECOMMENDS_LIST}",
  "debBase64": "${DEB_BASE64}"
}
EOF

echo "Built native ELF .deb: ${DEB_PATH} (${BYTE_SIZE} bytes, arch=${ARCH}, xz-compressed, sha256: ${SHA256_HASH})"
