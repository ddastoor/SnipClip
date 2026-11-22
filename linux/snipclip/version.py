"""Version metadata synchronized with the Debian (.deb) GitHub Release."""

__version__ = "1.0.0"
DEB_PACKAGE_NAME = "snipclip"
DEB_ARCHITECTURE = "all"
DEB_FILENAME = f"{DEB_PACKAGE_NAME}_{__version__}_{DEB_ARCHITECTURE}.deb"
GITHUB_RELEASE_TAG = f"v{__version__}"


def get_version_banner() -> str:
    """Return formatted version string matching the GitHub .deb release."""
    return f"SnipClip {__version__} ({DEB_FILENAME}, release {GITHUB_RELEASE_TAG})"
