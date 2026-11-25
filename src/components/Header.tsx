import React from 'react';
import { Download, Github } from 'lucide-react';

interface HeaderProps {
  version: string;
  onDownloadDeb: () => void;
}

export const Header: React.FC<HeaderProps> = ({ version, onDownloadDeb }) => {
  return (
    <header className="sticky top-0 z-30 bg-white/95 backdrop-blur border-b border-slate-200">
      <div className="max-w-[1280px] mx-auto flex items-center justify-between gap-8 px-6 py-3.5">
        {/* Zone 1: Single-line Brand Wordmark */}
        <a
          href="#studio"
          className="text-lg font-bold tracking-tight text-slate-900 whitespace-nowrap shrink-0 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-blue-600"
        >
          SnipClip
        </a>

        {/* Zone 2: 4 Single-line Navigation Links */}
        <nav
          aria-label="Primary navigation"
          className="hidden md:flex items-center gap-7 text-sm font-medium text-slate-600"
        >
          <a
            href="#studio"
            className="hover:text-slate-900 hover:underline underline-offset-4 transition-colors whitespace-nowrap shrink-0"
          >
            Snipping Studio
          </a>
          <a
            href="#display-detection"
            className="hover:text-slate-900 hover:underline underline-offset-4 transition-colors whitespace-nowrap shrink-0"
          >
            Display Detection
          </a>
          <a
            href="#debian-package"
            className="hover:text-slate-900 hover:underline underline-offset-4 transition-colors whitespace-nowrap shrink-0"
          >
            Debian Package
          </a>
          <a
            href="#github-release"
            className="hover:text-slate-900 hover:underline underline-offset-4 transition-colors whitespace-nowrap shrink-0"
          >
            GitHub Release
          </a>
        </nav>

        {/* Zone 3: 1 Primary Action */}
        <div className="flex items-center gap-2 shrink-0">
          <a
            href="https://github.com/ddastoor/SnipClip/releases"
            target="_blank"
            rel="noopener noreferrer"
            aria-label="SnipClip GitHub releases"
            className="inline-flex items-center gap-1.5 px-3 py-2 text-xs font-medium text-slate-700 bg-white border border-slate-300 rounded-lg hover:bg-slate-100 transition-colors whitespace-nowrap shrink-0 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-blue-600"
          >
            <Github className="w-3.5 h-3.5 shrink-0" aria-hidden="true" />
            <span className="hidden sm:inline">GitHub</span>
          </a>
          <button
            type="button"
            onClick={onDownloadDeb}
            className="inline-flex items-center gap-2 px-4 py-2 text-xs font-semibold text-white bg-slate-900 rounded-lg hover:bg-slate-800 transition-colors whitespace-nowrap shrink-0 cursor-pointer focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-blue-600"
          >
            <Download className="w-3.5 h-3.5 shrink-0" aria-hidden="true" />
            <span>Download v{version} .deb</span>
          </button>
        </div>
      </div>
    </header>
  );
};
