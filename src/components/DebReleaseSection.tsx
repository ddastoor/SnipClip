import React, { useState } from 'react';
import { Check, Copy, Download, ExternalLink } from 'lucide-react';
import { resolveVersionReleaseMetadata } from '../utils/snipEngine';
import {
  DebReleasePayload,
  downloadVerifiedDebPackage,
  downloadVerifiedSha256Sums,
} from '../utils/debDownload';
import releaseWorkflowRaw from '../../.github/workflows/release-deb.yml?raw';

export const GITHUB_REPO_URL = 'https://github.com/ddastoor/SnipClip';
export const GITHUB_RELEASES_URL = `${GITHUB_REPO_URL}/releases`;

interface DebReleaseSectionProps {
  release: DebReleasePayload;
}

type CodeTab = 'control' | 'workflow' | 'files';

export const DebReleaseSection: React.FC<DebReleaseSectionProps> = ({ release }) => {
  const [copiedCmd, setCopiedCmd] = useState<string | null>(null);
  const [copyFallback, setCopyFallback] = useState<string | null>(null);
  const [activeTab, setActiveTab] = useState<CodeTab>('control');
  const [tagInput, setTagInput] = useState<string>(release.releaseTag);

  const simulatedSync = resolveVersionReleaseMetadata(tagInput);

  const copyCommand = async (id: string, text: string) => {
    try {
      if (!navigator.clipboard?.writeText) {
        throw new Error('Clipboard API unavailable');
      }
      await navigator.clipboard.writeText(text);
    } catch {
      // Clipboard API is restricted (sandboxed iframe, insecure origin, or denied permission):
      // never flash a false "Copied" checkmark — tell the user to copy manually instead.
      setCopiedCmd(null);
      setCopyFallback('Clipboard access is blocked here — select the command text and copy it manually.');
      window.setTimeout(() => setCopyFallback(null), 4000);
      return;
    }
    setCopyFallback(null);
    setCopiedCmd(id);
    window.setTimeout(() => setCopiedCmd(null), 1800);
  };

  const installCmd = `sudo apt install ./${release.debFilename}`;
  const verifyCmd = `snipclip --version && sha256sum -c SHA256SUMS`;

  const controlPreview = `Package: ${release.packageName}
Version: ${simulatedSync.valid ? simulatedSync.version : release.version}
Section: graphics
Priority: optional
Architecture: ${release.architecture}
Installed-Size: ${release.installedSizeKb}
Depends: ${release.depends}
Recommends: ${release.recommends}
Maintainer: Darayus Dastoor <ddastoor@users.noreply.github.com>
Homepage: https://github.com/ddastoor/SnipClip
Description: Native GTK 3 Windows Snipping Tool clone for Debian Linux (X11 & Wayland)
 SnipClip is a natively compiled C + GTK 3 + Cairo screenshot and annotation
 application for Debian, Ubuntu, Linux Mint, Pop!_OS, KDE Plasma, GNOME, XFCE,
 and derivatives with automatic X11/Wayland startup detection.`;

  // Rendered verbatim from the real workflow file so the preview can never drift from CI.
  const workflowPreview = `# .github/workflows/release-deb.yml\n${releaseWorkflowRaw}`;

  const debFileTree = `/usr/bin/snipclip                                   # Stripped 64-bit ELF binary (C11 + GTK 3 + Cairo + libX11)
/usr/share/applications/snipclip.desktop            # Freedesktop Utilities & Graphics launch menus
/usr/share/doc/snipclip/changelog.Debian.gz         # Version ${simulatedSync.valid ? simulatedSync.version : release.version}
/usr/share/doc/snipclip/copyright
/usr/share/icons/hicolor/16x16/apps/snipclip.png
/usr/share/icons/hicolor/24x24/apps/snipclip.png
/usr/share/icons/hicolor/32x32/apps/snipclip.png
/usr/share/icons/hicolor/48x48/apps/snipclip.png
/usr/share/icons/hicolor/64x64/apps/snipclip.png
/usr/share/icons/hicolor/128x128/apps/snipclip.png
/usr/share/icons/hicolor/256x256/apps/snipclip.png
/usr/share/icons/hicolor/scalable/apps/snipclip.svg
/usr/share/menu/snipclip                            # Debian Menu system entry
/usr/share/metainfo/snipclip.metainfo.xml           # KDE Discover & GNOME Software AppStream
/usr/share/pixmaps/snipclip.png
/usr/share/snipclip/snipclip.png                    # Runtime window icon (16 files total)`;

  return (
    <section id="debian-package" className="py-12 border-t border-slate-200 bg-slate-50">
      <div className="max-w-[1280px] mx-auto px-6">
        <div className="flex flex-col lg:flex-row lg:items-end lg:justify-between gap-6 mb-8">
          <div className="max-w-2xl">
            <h2 className="text-xl sm:text-2xl font-bold tracking-tight text-slate-900 [text-wrap:balance]">
              Debian (.deb) Deliverable &amp; GitHub Release Synchronization
            </h2>
            <p className="mt-2 text-sm sm:text-base text-slate-600">
              Built with <code className="font-mono text-xs text-slate-800">dpkg-deb -Zxz</code>,
              AppStream MetaInfo, and multi-resolution Hicolor/Desktop menu entries so double-click
              installation works cleanly in KDE Discover, GNOME Software, GDebi, and{' '}
              <code className="font-mono text-xs text-slate-800">apt</code>, registering SnipClip in
              all desktop launch menus.
            </p>
          </div>

          <div className="flex flex-wrap items-center gap-3">
            <button
              type="button"
              onClick={() => {
                void downloadVerifiedDebPackage(release);
              }}
              className="inline-flex items-center gap-2 px-4 py-2.5 text-xs font-semibold text-white bg-slate-900 rounded-lg hover:bg-slate-800 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
            >
              <Download className="w-4 h-4 shrink-0" aria-hidden="true" />
              <span>Download {release.debFilename}</span>
            </button>
            <button
              type="button"
              onClick={() => downloadVerifiedSha256Sums(release)}
              className="inline-flex items-center gap-1.5 px-3.5 py-2.5 text-xs font-medium text-slate-700 bg-white border border-slate-300 rounded-lg hover:bg-slate-100 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
            >
              <span>SHA256SUMS</span>
            </button>
            <a
              href={GITHUB_RELEASES_URL}
              target="_blank"
              rel="noopener noreferrer"
              data-testid="github-releases-link"
              className="inline-flex items-center gap-1.5 px-3.5 py-2.5 text-xs font-medium text-slate-700 bg-white border border-slate-300 rounded-lg hover:bg-slate-100 transition-colors whitespace-nowrap shrink-0"
            >
              <ExternalLink className="w-3.5 h-3.5 shrink-0" aria-hidden="true" />
              <span>GitHub Releases</span>
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
                Graphical Install &amp; Launch (No Command Line Required)
              </h3>
              <ol className="space-y-2.5 text-xs text-slate-700 mb-4">
                <li className="p-2.5 rounded-lg bg-slate-50 border border-slate-200">
                  <span className="font-semibold text-slate-900">1. Double-click in File Explorer:</span>{' '}
                  Open <code className="font-mono text-slate-900">{release.debFilename}</code> from
                  Dolphin, GNOME Files, or Nemo to launch <strong>KDE Discover</strong>,{' '}
                  <strong>GNOME Software</strong>, or <strong>GDebi</strong>.
                </li>
                <li className="p-2.5 rounded-lg bg-slate-50 border border-slate-200">
                  <span className="font-semibold text-slate-900">2. Click Install / Update:</span>{' '}
                  Native GTK 3 &amp; X11 shared libraries (<code className="font-mono">libgtk-3-0t64 | libgtk-3-0</code>)
                  and desktop icons are configured automatically across Debian, Ubuntu, Mint &amp; derivatives.
                </li>
                <li className="p-2.5 rounded-lg bg-slate-50 border border-slate-200">
                  <span className="font-semibold text-slate-900">
                    3. Launch from Installer or App Menu:
                  </span>{' '}
                  Click <strong>Launch</strong> directly inside Discover / Software, or open{' '}
                  <strong>SnipClip</strong> under <strong>Utilities</strong> or{' '}
                  <strong>Graphics</strong>. Wayland or X11 is auto-detected on startup.
                </li>
              </ol>

              <div className="space-y-2">
                <div className="text-xs text-slate-500">
                  Optional terminal alternative (<code className="font-mono">apt</code>):
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
                {copyFallback && (
                  <p role="status" data-testid="copy-fallback-status" className="text-xs text-amber-700">
                    {copyFallback}
                  </p>
                )}
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
