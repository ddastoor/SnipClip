#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

RAW_VERSION="${1:-${GITHUB_REF_NAME:-}}"
if [[ -z "${RAW_VERSION}" ]]; then
  RAW_VERSION="$(tr -d '[:space:]' < "${ROOT_DIR}/VERSION")"
fi

# Strip optional leading 'v' from GitHub release tag (e.g., v1.2.0 -> 1.2.0)
CLEAN_VERSION="${RAW_VERSION#v}"

if ! [[ "${CLEAN_VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+([.-][0-9A-Za-z]+)*$ ]]; then
  echo "Error: Invalid Debian package version '${CLEAN_VERSION}' (derived from '${RAW_VERSION}')" >&2
  exit 1
fi

PKG_NAME="snipclip"
ARCH="all"
DEB_FILENAME="${PKG_NAME}_${CLEAN_VERSION}_${ARCH}.deb"
RELEASE_TAG="v${CLEAN_VERSION}"

# 1. Sync VERSION and Python package version so runtime app matches GitHub .deb release
printf "%s\n" "${CLEAN_VERSION}" > "${ROOT_DIR}/VERSION"

cat > "${ROOT_DIR}/linux/snipclip/version.py" <<EOF
"""Version metadata synchronized with the Debian (.deb) GitHub Release."""

__version__ = "${CLEAN_VERSION}"
DEB_PACKAGE_NAME = "${PKG_NAME}"
DEB_ARCHITECTURE = "${ARCH}"
DEB_FILENAME = f"{DEB_PACKAGE_NAME}_{__version__}_{DEB_ARCHITECTURE}.deb"
GITHUB_RELEASE_TAG = f"v{__version__}"


def get_version_banner() -> str:
    """Return formatted version string matching the GitHub .deb release."""
    return f"SnipClip {__version__} ({DEB_FILENAME}, release {GITHUB_RELEASE_TAG})"
EOF

# 2. Stage Debian package filesystem hierarchy
STAGE_DIR="$(mktemp -d)"
trap 'rm -rf "${STAGE_DIR}"' EXIT

mkdir -p \
  "${STAGE_DIR}/DEBIAN" \
  "${STAGE_DIR}/usr/bin" \
  "${STAGE_DIR}/usr/lib/snipclip/snipclip" \
  "${STAGE_DIR}/usr/share/applications" \
  "${STAGE_DIR}/usr/share/icons/hicolor/scalable/apps" \
  "${STAGE_DIR}/usr/share/doc/snipclip"

# Copy Python modules
cp "${ROOT_DIR}"/linux/snipclip/*.py "${STAGE_DIR}/usr/lib/snipclip/snipclip/"

# Create /usr/bin/snipclip executable launcher
cat > "${STAGE_DIR}/usr/bin/snipclip" <<'EOF'
#!/usr/bin/env python3
import sys

sys.path.insert(0, "/usr/lib/snipclip")
from snipclip.cli import main

if __name__ == "__main__":
    raise SystemExit(main())
EOF
chmod 0755 "${STAGE_DIR}/usr/bin/snipclip"

# Copy desktop entry and scalable icon
cp "${ROOT_DIR}/packaging/snipclip.desktop" "${STAGE_DIR}/usr/share/applications/snipclip.desktop"
chmod 0644 "${STAGE_DIR}/usr/share/applications/snipclip.desktop"

cp "${ROOT_DIR}/packaging/snipclip.svg" "${STAGE_DIR}/usr/share/icons/hicolor/scalable/apps/snipclip.svg"
chmod 0644 "${STAGE_DIR}/usr/share/icons/hicolor/scalable/apps/snipclip.svg"

# Debian copyright & compressed changelog
cat > "${STAGE_DIR}/usr/share/doc/snipclip/copyright" <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: snipclip
Source: https://github.com/snipclip/snipclip

Files: *
Copyright: 2026 SnipClip Contributors
License: MIT
EOF
chmod 0644 "${STAGE_DIR}/usr/share/doc/snipclip/copyright"

BUILD_DATE_RFC="$(date -R)"
cat > "${STAGE_DIR}/usr/share/doc/snipclip/changelog.Debian" <<EOF
snipclip (${CLEAN_VERSION}) stable; urgency=medium

  * Release SnipClip ${CLEAN_VERSION} (${RELEASE_TAG}) for Debian-based Linux distributions.
  * Automatic X11 and Wayland session detection with rectangle, window, fullscreen, and freeform snips.

 -- SnipClip Maintainers <maintainers@snipclip.local>  ${BUILD_DATE_RFC}
EOF
gzip -9 -n "${STAGE_DIR}/usr/share/doc/snipclip/changelog.Debian"
chmod 0644 "${STAGE_DIR}/usr/share/doc/snipclip/changelog.Debian.gz"

INSTALLED_KB="$(du -sk "${STAGE_DIR}/usr" | awk '{print $1}')"

cat > "${STAGE_DIR}/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${CLEAN_VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Installed-Size: ${INSTALLED_KB}
Depends: python3 (>= 3.10), python3-pyqt6, xdg-utils
Recommends: qt6-wayland, wl-clipboard, xclip, grim, x11-utils, xdg-desktop-portal
Maintainer: SnipClip Maintainers <maintainers@snipclip.local>
Homepage: https://github.com/snipclip/snipclip
Description: Windows Snipping Tool clone for Debian-based Linux (X11 & Wayland)
 SnipClip brings the Windows 11 Snipping Tool workflow to Debian, Ubuntu,
 Linux Mint, and derivatives. At startup it automatically detects X11 or
 Wayland and supports Rectangular, Window, Fullscreen, and Freeform snips,
 delay timers, Pen, Highlighter, Shapes, Ruler, Redact/Blur, and Crop.
EOF
chmod 0644 "${STAGE_DIR}/DEBIAN/control"

# Ensure deterministic standard directory permissions inside .deb
find "${STAGE_DIR}" -type d -exec chmod 0755 {} +

OUT_DIR="${ROOT_DIR}/public/releases"
mkdir -p "${OUT_DIR}"
# Clean old .deb artifacts if version changed
rm -f "${OUT_DIR}"/${PKG_NAME}_*_${ARCH}.deb

DEB_PATH="${OUT_DIR}/${DEB_FILENAME}"
dpkg-deb --build --root-owner-group "${STAGE_DIR}" "${DEB_PATH}"

(
  cd "${OUT_DIR}"
  sha256sum "${DEB_FILENAME}" > SHA256SUMS
)

SHA256_HASH="$(awk '{print $1}' "${OUT_DIR}/SHA256SUMS")"
BYTE_SIZE="$(wc -c < "${DEB_PATH}" | tr -d '[:space:]')"
BUILT_AT_ISO="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

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
  "sizeBytes": ${BYTE_SIZE},
  "installedSizeKb": ${INSTALLED_KB},
  "builtAtIso": "${BUILT_AT_ISO}",
  "depends": "python3 (>= 3.10), python3-pyqt6, xdg-utils",
  "recommends": "qt6-wayland, wl-clipboard, xclip, grim, x11-utils, xdg-desktop-portal"
}
EOF

echo "Built ${DEB_PATH} (${BYTE_SIZE} bytes, sha256: ${SHA256_HASH})"
