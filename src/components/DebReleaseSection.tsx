import React, { useState } from 'react';
import { Check, Copy, Download } from 'lucide-react';
import { resolveVersionReleaseMetadata } from '../utils/snipEngine';

export interface ReleaseInfoData {
  packageName: string;
  version: string;
  releaseTag: string;
  architecture: string;
  debFilename: string;
  downloadPath: string;
  sha256: string;
  sizeBytes: number;
  installedSizeKb: number;
  builtAtIso: string;
  depends: string;
  recommends: string;
}

interface DebReleaseSectionProps {
  release: ReleaseInfoData;
}

type CodeTab = 'control' | 'workflow' | 'files';

export const DebReleaseSection: React.FC<DebReleaseSectionProps> = ({ release }) => {
  const [copiedCmd, setCopiedCmd] = useState<string | null>(null);
  const [activeTab, setActiveTab] = useState<CodeTab>('control');
  const [tagInput, setTagInput] = useState<string>(release.releaseTag);

  const simulatedSync = resolveVersionReleaseMetadata(tagInput);

  const copyCommand = async (id: string, text: string) => {
    try {
      await navigator.clipboard.writeText(text);
    } catch {
      // Clipboard API may be restricted in sandboxed frames; visual confirmation still shown
    }
    setCopiedCmd(id);
    window.setTimeout(() => setCopiedCmd(null), 1800);
  };

  const installCmd = `sudo apt install ./${release.debFilename}`;
  const verifyCmd = `snipclip --version && sha256sum -c SHA256SUMS`;

  const controlPreview = `Package: ${release.packageName}
Version: ${simulatedSync.valid ? simulatedSync.version : release.version}
Section: utils
Priority: optional
Architecture: ${release.architecture}
Installed-Size: ${release.installedSizeKb}
Depends: ${release.depends}
Recommends: ${release.recommends}
Maintainer: SnipClip Maintainers <maintainers@snipclip.local>
Description: Windows Snipping Tool clone for Debian-based Linux (X11 & Wayland)
 SnipClip brings the Windows 11 Snipping Tool workflow to Debian, Ubuntu,
 Linux Mint, and derivatives with automatic X11/Wayland session detection.`;

  const workflowPreview = `# .github/workflows/release-deb.yml
on:
  push:
    tags:
      - "v*.*.*"

jobs:
  build-deb-release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build .deb synced to GitHub Release tag
        run: ./scripts/build-deb.sh "\${GITHUB_REF_NAME#v}"
      - name: Publish .deb to GitHub Releases page
        uses: softprops/action-gh-release@v2
        with:
          files: |
            public/releases/snipclip_\${GITHUB_REF_NAME#v}_all.deb
            public/releases/SHA256SUMS`;

  const debFileTree = `/usr/bin/snipclip
/usr/lib/snipclip/snipclip/__init__.py
/usr/lib/snipclip/snipclip/version.py          # __version__ == "${simulatedSync.valid ? simulatedSync.version : release.version}"
/usr/lib/snipclip/snipclip/display_backend.py  # X11 & Wayland startup auto-detection
/usr/lib/snipclip/snipclip/geometry.py         # Pure bounds, ruler snap & hit-test math
/usr/lib/snipclip/snipclip/capture_overlay.py  # Rectangle, Window, Fullscreen, Freeform
/usr/lib/snipclip/snipclip/canvas_editor.py    # Pen, Highlighter, Shapes, Ruler, Redact, Crop
/usr/lib/snipclip/snipclip/main_window.py      # Windows 11 Snipping Tool PyQt6 UI
/usr/lib/snipclip/snipclip/cli.py              # --version, --detect-display, --mode, --delay
/usr/share/applications/snipclip.desktop
/usr/share/icons/hicolor/scalable/apps/snipclip.svg
/usr/share/doc/snipclip/changelog.Debian.gz
/usr/share/doc/snipclip/copyright`;

  return (
    <section id="debian-package" className="py-12 border-t border-slate-200 bg-slate-50">
      <div className="max-w-[1280px] mx-auto px-6">
        <div className="flex flex-col lg:flex-row lg:items-end lg:justify-between gap-6 mb-8">
          <div className="max-w-2xl">
            <h2 className="text-xl sm:text-2xl font-bold tracking-tight text-slate-900 [text-wrap:balance]">
              Debian (.deb) Deliverable &amp; GitHub Release Synchronization
            </h2>
            <p className="mt-2 text-sm sm:text-base text-slate-600">
              Built with <code className="font-mono text-xs text-slate-800">dpkg-deb</code> for
              Debian 12+, Ubuntu 22.04+, Linux Mint, and Pop!_OS. Every GitHub Release tag
              automatically injects its version into{' '}
              <code className="font-mono text-xs text-slate-800">snipclip/version.py</code> and{' '}
              <code className="font-mono text-xs text-slate-800">DEBIAN/control</code> before
              packaging.
            </p>
          </div>

          <div className="flex flex-wrap items-center gap-3">
            <a
              href={release.downloadPath}
              download={release.debFilename}
              className="inline-flex items-center gap-2 px-4 py-2.5 text-xs font-semibold text-white bg-slate-900 rounded-lg hover:bg-slate-800 transition-colors whitespace-nowrap shrink-0"
            >
              <Download className="w-4 h-4 shrink-0" aria-hidden="true" />
              <span>Download {release.debFilename}</span>
            </a>
            <a
              href="/releases/SHA256SUMS"
              download="SHA256SUMS"
              className="inline-flex items-center gap-1.5 px-3.5 py-2.5 text-xs font-medium text-slate-700 bg-white border border-slate-300 rounded-lg hover:bg-slate-100 transition-colors whitespace-nowrap shrink-0"
            >
              <span>SHA256SUMS</span>
            </a>
          </div>
        </div>

        {/* Package Spec Strip */}
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-px bg-slate-200 border border-slate-200 rounded-xl overflow-hidden mb-8">
          <div className="bg-white p-5">
            <div className="text-xs text-slate-500">Release Artifact</div>
            <div className="mt-1 text-sm font-mono font-semibold text-slate-900 truncate">
              {release.debFilename}
            </div>
            <div className="mt-1 text-xs text-slate-500 font-mono tabular-nums">
              Tag {release.releaseTag} · Arch: {release.architecture}
            </div>
          </div>

          <div className="bg-white p-5">
            <div className="text-xs text-slate-500">App &amp; Package Version</div>
            <div className="mt-1 text-sm font-mono font-semibold text-slate-900 tabular-nums">
              {release.version}
            </div>
            <div className="mt-1 text-xs text-slate-500 font-mono">
              Matches snipclip --version
            </div>
          </div>

          <div className="bg-white p-5">
            <div className="text-xs text-slate-500">Binary Package Size</div>
            <div className="mt-1 text-sm font-mono font-semibold text-slate-900 tabular-nums">
              {(release.sizeBytes / 1024).toFixed(1)} KB ({release.sizeBytes.toLocaleString()} B)
            </div>
            <div className="mt-1 text-xs text-slate-500 font-mono tabular-nums">
              Installed: {release.installedSizeKb} KB
            </div>
          </div>

          <div className="bg-white p-5">
            <div className="text-xs text-slate-500">SHA-256 Checksum</div>
            <div
              title={release.sha256}
              className="mt-1 text-sm font-mono font-semibold text-slate-900 truncate tabular-nums"
            >
              {release.sha256.slice(0, 20)}…
            </div>
            <div className="mt-1 text-xs text-slate-500">Verified dpkg-deb v2.0 archive</div>
          </div>
        </div>

        {/* 2-Column Split: Installation + Version Sync Simulator vs Package Contents */}
        <div
          id="github-release"
          className="grid grid-cols-1 lg:grid-cols-12 gap-8 items-stretch"
        >
          {/* Left Column: Install Commands & GitHub Tag Version Sync Tester */}
          <div className="lg:col-span-5 bg-white border border-slate-200 rounded-xl p-6 flex flex-col justify-between gap-6">
            <div>
              <h3 className="text-sm font-semibold text-slate-900 mb-3">
                Install on Debian / Ubuntu
              </h3>
              <div className="space-y-3">
                <div>
                  <div className="text-xs text-slate-500 mb-1">
                    1. Install package and resolve apt dependencies automatically:
                  </div>
                  <div className="flex items-center justify-between gap-2 px-3 py-2 bg-slate-900 text-slate-100 rounded-lg font-mono text-xs">
                    <span className="truncate">{installCmd}</span>
                    <button
                      type="button"
                      aria-label="Copy apt install command"
                      onClick={() => copyCommand('install', installCmd)}
                      className="p-1 text-slate-300 hover:text-white shrink-0 cursor-pointer"
                    >
                      {copiedCmd === 'install' ? (
                        <Check className="w-3.5 h-3.5 text-emerald-400" aria-hidden="true" />
                      ) : (
                        <Copy className="w-3.5 h-3.5" aria-hidden="true" />
                      )}
                    </button>
                  </div>
                </div>

                <div>
                  <div className="text-xs text-slate-500 mb-1">
                    2. Verify CLI version matches the downloaded .deb release:
                  </div>
                  <div className="flex items-center justify-between gap-2 px-3 py-2 bg-slate-900 text-slate-100 rounded-lg font-mono text-xs">
                    <span className="truncate">{verifyCmd}</span>
                    <button
                      type="button"
                      aria-label="Copy verification command"
                      onClick={() => copyCommand('verify', verifyCmd)}
                      className="p-1 text-slate-300 hover:text-white shrink-0 cursor-pointer"
                    >
                      {copiedCmd === 'verify' ? (
                        <Check className="w-3.5 h-3.5 text-emerald-400" aria-hidden="true" />
                      ) : (
                        <Copy className="w-3.5 h-3.5" aria-hidden="true" />
                      )}
                    </button>
                  </div>
                </div>
              </div>
            </div>

            {/* Interactive Release Version Sync Simulator */}
            <div className="pt-5 border-t border-slate-200">
              <h4 className="text-sm font-semibold text-slate-900 mb-2">
                GitHub Tag → Version Sync Preview
              </h4>
              <p className="text-xs text-slate-600 mb-3">
                Test how pushing a Git tag updates the <code className="font-mono">.deb</code>{' '}
                filename, <code className="font-mono">DEBIAN/control</code>, and runtime{' '}
                <code className="font-mono">--version</code> output:
              </p>

              <label htmlFor="release-tag-input" className="block text-xs font-medium text-slate-700 mb-1">
                GitHub Release Tag
              </label>
              <input
                id="release-tag-input"
                type="text"
                value={tagInput}
                onChange={(e) => setTagInput(e.target.value)}
                placeholder="e.g. v1.2.0"
                className="w-full px-3 py-2 text-xs font-mono bg-slate-50 border border-slate-300 rounded-lg text-slate-900 focus:outline-2 focus:outline-blue-600"
              />

              {!simulatedSync.valid ? (
                <p role="alert" className="mt-2 text-xs text-red-600 font-medium">
                  {simulatedSync.error}
                </p>
              ) : (
                <dl className="mt-3 space-y-1.5 text-xs font-mono bg-slate-50 p-3 rounded-lg border border-slate-200">
                  <div className="flex justify-between gap-2">
                    <dt className="text-slate-500">GitHub Asset:</dt>
                    <dd data-testid="synced-deb-filename" className="text-slate-900 font-semibold">
                      {simulatedSync.debFilename}
                    </dd>
                  </div>
                  <div className="flex justify-between gap-2">
                    <dt className="text-slate-500">App Version:</dt>
                    <dd className="text-slate-900">{simulatedSync.version}</dd>
                  </div>
                  <div className="pt-1 border-t border-slate-200 text-slate-700 break-all">
                    $ snipclip --version → {simulatedSync.cliBanner}
                  </div>
                </dl>
              )}
            </div>
          </div>

          {/* Right Column: Debian Control, GitHub Workflow & Archive Contents */}
          <div className="lg:col-span-7 bg-white border border-slate-200 rounded-xl overflow-hidden flex flex-col">
            <div className="flex items-center justify-between gap-4 px-4 py-3 bg-slate-50 border-b border-slate-200">
              <div
                role="tablist"
                aria-label="Debian package and GitHub workflow files"
                className="flex items-center gap-1 p-1 bg-slate-200/70 rounded-lg"
              >
                {(
                  [
                    { id: 'control', label: 'DEBIAN/control' },
                    { id: 'workflow', label: 'release-deb.yml' },
                    { id: 'files', label: '.deb File Tree' },
                  ] as const
                ).map((t) => (
                  <button
                    key={t.id}
                    type="button"
                    role="tab"
                    aria-selected={activeTab === t.id}
                    onClick={() => setActiveTab(t.id)}
                    className={`px-3 py-1.5 text-xs font-medium rounded-md transition-colors whitespace-nowrap shrink-0 cursor-pointer ${
                      activeTab === t.id
                        ? 'bg-white text-slate-900 shadow-xs'
                        : 'text-slate-600 hover:text-slate-900'
                    }`}
                  >
                    {t.label}
                  </button>
                ))}
              </div>

              <span className="text-xs font-mono text-slate-500 hidden sm:inline">
                dpkg-deb --root-owner-group
              </span>
            </div>

            <pre className="p-5 text-xs font-mono leading-relaxed text-slate-800 bg-white overflow-x-auto flex-1">
              <code>
                {activeTab === 'control' && controlPreview}
                {activeTab === 'workflow' && workflowPreview}
                {activeTab === 'files' && debFileTree}
              </code>
            </pre>
          </div>
        </div>
      </div>
    </section>
  );
};
