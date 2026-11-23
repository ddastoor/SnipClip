# SnipClip Sync Point — 2026-11-23

## Current Repository & Branch State

- **Active Working Branch**: `fix/snipping-tool-startup-dimensions` (pushed to `origin/fix/snipping-tool-startup-dimensions`)
- **Open Pull Requests**:
  1. **PR #1** (`chore/add-agents-environment-setup` -> `main`): Adds `.agents/environment.json` for automated cloud VM setup (`apt-get` GTK 3/X11 dev packages + `npm` + `make -C native all`).
  2. **PR #2** (`fix/snipping-tool-startup-dimensions` -> `main`):
     - Compact **Microsoft Snipping Tool startup dimensions (`460 × 200 px`)** in both native GTK 3 (`native/src/main.c`, `native/src/editor_canvas.c`, `native/src/geometry.c`) and web workbench (`src/components/SnipStudio.tsx`, `src/utils/snipEngine.ts`).
     - Eliminates horizontal full-screen stretch (`launcher_min_w = 340 px`, `editor_min_w = 551 px` verified under live `Xvfb` `1920×1080`).
     - Hardened startup X11 & Wayland auto-detection (`native/src/display_detect.c`).
     - Version bumped to **`2.1.0` (`v2.1.0`)** across `VERSION`, `package.json`, `README.md`, `native/include/snipclip_version.h`, `public/releases/snipclip_2.1.0_amd64.deb`, `public/releases/SHA256SUMS`, and `src/data/releaseInfo.json`.
     - Fixed pre-existing GitHub Actions CI bugs in `.github/workflows/ci.yml` (removed lockfile-dependent `cache: "npm"`, added `xvfb xauth`) and `scripts/build-deb.sh` (guarded `GITHUB_REF_NAME` so `main` and PR `2/merge` refs fall back cleanly to `VERSION`).

## Exact Next Steps to Resume Tomorrow

1. **Verify CI check on PR #2** (`gh pr checks 2 -R ddastoor/SnipClip`).
2. **Merge PR #1 and PR #2 into `main`**:
   ```bash
   gh pr merge 1 -R ddastoor/SnipClip --squash --delete-branch
   gh pr merge 2 -R ddastoor/SnipClip --squash --delete-branch
   ```
3. **Verify Automated `v2.1.0` GitHub Release**:
   - Merging PR #2 to `main` automatically triggers `.github/workflows/release-deb.yml` inside a `debian:bookworm` container (`glibc <= 2.36` cross-distro baseline) to build and publish `snipclip_2.1.0_amd64.deb` + `SHA256SUMS` to `https://github.com/ddastoor/SnipClip/releases/tag/v2.1.0`.
   - Watch the release workflow to completion and verify `gh release view v2.1.0 -R ddastoor/SnipClip --json tagName,assets,url`.
