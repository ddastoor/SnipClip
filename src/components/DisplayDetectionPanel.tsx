import React from 'react';
import {
  DetectedSessionResult,
  LinuxBinarySet,
  LinuxEnvInput,
} from '../utils/snipEngine';

export interface SessionPreset {
  id: string;
  label: string;
  env: LinuxEnvInput;
  bins: LinuxBinarySet;
}

export const SESSION_PRESETS: SessionPreset[] = [
  {
    id: 'debian-gnome-wayland',
    label: 'Debian 12 GNOME (Wayland)',
    env: {
      XDG_SESSION_TYPE: 'wayland',
      WAYLAND_DISPLAY: 'wayland-0',
      DISPLAY: ':0',
      XDG_CURRENT_DESKTOP: 'GNOME',
    },
    bins: {
      gdbus: true,
      grim: false,
      gnomeScreenshot: true,
      spectacle: false,
      wlCopy: true,
      xclip: false,
      xsel: false,
    },
  },
  {
    id: 'kde-wayland',
    label: 'KDE Plasma 6 (Wayland)',
    env: {
      XDG_SESSION_TYPE: 'wayland',
      WAYLAND_DISPLAY: 'wayland-0',
      DISPLAY: ':1',
      XDG_CURRENT_DESKTOP: 'KDE',
    },
    bins: {
      gdbus: true,
      grim: false,
      gnomeScreenshot: false,
      spectacle: true,
      wlCopy: true,
      xclip: false,
      xsel: false,
    },
  },
  {
    id: 'sway-wlroots',
    label: 'Sway / Hyprland (Wayland)',
    env: {
      XDG_SESSION_TYPE: 'wayland',
      WAYLAND_DISPLAY: 'wayland-1',
      DISPLAY: '',
      XDG_CURRENT_DESKTOP: 'sway',
    },
    bins: {
      gdbus: false,
      grim: true,
      gnomeScreenshot: false,
      spectacle: false,
      wlCopy: true,
      xclip: false,
      xsel: false,
    },
  },
  {
    id: 'xfce-x11',
    label: 'Debian XFCE / Cinnamon (X11)',
    env: {
      XDG_SESSION_TYPE: 'x11',
      WAYLAND_DISPLAY: '',
      DISPLAY: ':0.0',
      XDG_CURRENT_DESKTOP: 'XFCE',
    },
    bins: {
      gdbus: true,
      grim: false,
      gnomeScreenshot: false,
      spectacle: false,
      wlCopy: false,
      xclip: true,
      xsel: true,
    },
  },
  {
    id: 'i3-x11',
    label: 'i3wm Minimal Xorg (X11)',
    env: {
      XDG_SESSION_TYPE: '',
      WAYLAND_DISPLAY: '',
      DISPLAY: ':0',
      XDG_CURRENT_DESKTOP: 'i3',
    },
    bins: {
      gdbus: false,
      grim: false,
      gnomeScreenshot: false,
      spectacle: false,
      wlCopy: false,
      xclip: true,
      xsel: false,
    },
  },
];

interface DisplayDetectionPanelProps {
  activePresetId: string;
  env: LinuxEnvInput;
  bins: LinuxBinarySet;
  detected: DetectedSessionResult;
  onSelectPreset: (preset: SessionPreset) => void;
  onChangeEnv: (next: LinuxEnvInput) => void;
  onChangeBins: (next: LinuxBinarySet) => void;
}

export const DisplayDetectionPanel: React.FC<DisplayDetectionPanelProps> = ({
  activePresetId,
  env,
  bins,
  detected,
  onSelectPreset,
  onChangeEnv,
  onChangeBins,
}) => {
  return (
    <section id="display-detection" className="py-12 border-t border-slate-200 bg-white">
      <div className="max-w-[1280px] mx-auto px-6">
        <div className="max-w-2xl mb-8">
          <h2 className="text-xl sm:text-2xl font-bold tracking-tight text-slate-900 [text-wrap:balance]">
            Automatic GUI Startup X11 &amp; Wayland Detection
          </h2>
          <p className="mt-2 text-sm sm:text-base text-slate-600">
            The moment you open <strong>SnipClip</strong> from KDE Discover, GNOME Software, or your
            desktop Application Menu, it automatically inspects session variables, Wayland/X11
            sockets, and <code className="font-mono text-xs text-slate-800">loginctl</code> to
            configure native capture and clipboard drivers with zero command-line setup.
          </p>
        </div>

        {/* Interactive Preset Switcher */}
        <div className="mb-6">
          <div className="text-xs font-medium text-slate-500 mb-2">
            Simulate Debian / Ubuntu Desktop Environment:
          </div>
          <div
            role="group"
            aria-label="Linux display presets"
            className="inline-flex flex-wrap items-center gap-1 p-1 bg-slate-100 rounded-lg"
          >
            {SESSION_PRESETS.map((preset) => {
              const active = activePresetId === preset.id;
              return (
                <button
                  key={preset.id}
                  type="button"
                  aria-pressed={active}
                  onClick={() => onSelectPreset(preset)}
                  className={`px-3 py-1.5 text-xs font-medium rounded-md transition-colors whitespace-nowrap shrink-0 cursor-pointer ${
                    active
                      ? 'bg-white text-slate-900 shadow-xs'
                      : 'text-slate-600 hover:text-slate-900'
                  }`}
                >
                  {preset.label}
                </button>
              );
            })}
          </div>
        </div>

        {/* 2-Column Split: Environment Inputs & Resolved Backend Output */}
        <div className="grid grid-cols-1 lg:grid-cols-12 gap-8 items-stretch">
          {/* Left Column: Environment Variables & Installed Packages */}
          <div className="lg:col-span-6 p-6 bg-slate-50 border border-slate-200 rounded-xl flex flex-col justify-between gap-6">
            <div>
              <h3 className="text-sm font-semibold text-slate-900 mb-4">
                Session Environment Variables
              </h3>
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
                <div>
                  <label
                    htmlFor="env-xdg-session"
                    className="block text-xs font-mono text-slate-600 mb-1"
                  >
                    XDG_SESSION_TYPE
                  </label>
                  <input
                    id="env-xdg-session"
                    type="text"
                    value={env.XDG_SESSION_TYPE}
                    placeholder="wayland | x11 | (empty)"
                    onChange={(e) => onChangeEnv({ ...env, XDG_SESSION_TYPE: e.target.value })}
                    className="w-full px-3 py-2 text-xs font-mono bg-white border border-slate-300 rounded-lg text-slate-900 focus:outline-2 focus:outline-blue-600"
                  />
                </div>
                <div>
                  <label
                    htmlFor="env-wayland-display"
                    className="block text-xs font-mono text-slate-600 mb-1"
                  >
                    WAYLAND_DISPLAY
                  </label>
                  <input
                    id="env-wayland-display"
                    type="text"
                    value={env.WAYLAND_DISPLAY}
                    placeholder="wayland-0"
                    onChange={(e) => onChangeEnv({ ...env, WAYLAND_DISPLAY: e.target.value })}
                    className="w-full px-3 py-2 text-xs font-mono bg-white border border-slate-300 rounded-lg text-slate-900 focus:outline-2 focus:outline-blue-600"
                  />
                </div>
                <div>
                  <label
                    htmlFor="env-x11-display"
                    className="block text-xs font-mono text-slate-600 mb-1"
                  >
                    DISPLAY
                  </label>
                  <input
                    id="env-x11-display"
                    type="text"
                    value={env.DISPLAY}
                    placeholder=":0"
                    onChange={(e) => onChangeEnv({ ...env, DISPLAY: e.target.value })}
                    className="w-full px-3 py-2 text-xs font-mono bg-white border border-slate-300 rounded-lg text-slate-900 focus:outline-2 focus:outline-blue-600"
                  />
                </div>
                <div>
                  <label
                    htmlFor="env-desktop"
                    className="block text-xs font-mono text-slate-600 mb-1"
                  >
                    XDG_CURRENT_DESKTOP
                  </label>
                  <input
                    id="env-desktop"
                    type="text"
                    value={env.XDG_CURRENT_DESKTOP}
                    placeholder="GNOME | KDE | XFCE | sway"
                    onChange={(e) => onChangeEnv({ ...env, XDG_CURRENT_DESKTOP: e.target.value })}
                    className="w-full px-3 py-2 text-xs font-mono bg-white border border-slate-300 rounded-lg text-slate-900 focus:outline-2 focus:outline-blue-600"
                  />
                </div>
              </div>
            </div>

            <div className="pt-4 border-t border-slate-200">
              <h4 className="text-xs font-semibold text-slate-700 mb-3">
                Installed Optional Debian Helper Binaries
              </h4>
              <div className="grid grid-cols-2 sm:grid-cols-3 gap-2.5 text-xs font-mono text-slate-700">
                {(
                  [
                    { key: 'gdbus', label: 'xdg-desktop-portal' },
                    { key: 'grim', label: 'grim' },
                    { key: 'wlCopy', label: 'wl-copy' },
                    { key: 'xclip', label: 'xclip' },
                    { key: 'spectacle', label: 'spectacle' },
                    { key: 'gnomeScreenshot', label: 'gnome-screenshot' },
                  ] as const
                ).map(({ key, label }) => (
                  <label key={key} className="inline-flex items-center gap-2 cursor-pointer">
                    <input
                      type="checkbox"
                      checked={bins[key]}
                      onChange={(e) => onChangeBins({ ...bins, [key]: e.target.checked })}
                      className="rounded accent-blue-600 cursor-pointer"
                    />
                    <span>{label}</span>
                  </label>
                ))}
              </div>
            </div>
          </div>

          {/* Right Column: Resolved Runtime Pipeline */}
          <div className="lg:col-span-6 p-6 bg-slate-50 border border-slate-200 rounded-xl flex flex-col justify-between">
            <div>
              <div className="flex items-center justify-between pb-3 mb-4 border-b border-slate-200">
                <h3 className="text-sm font-semibold text-slate-900">
                  Resolved SnipClip GUI Startup Pipeline
                </h3>
                <span className="text-xs font-mono font-semibold text-blue-700">
                  Auto-detected on GUI launch
                </span>
              </div>

              <dl className="divide-y divide-slate-200 text-xs">
                <div className="py-2.5 flex items-baseline justify-between gap-4">
                  <dt className="text-slate-500 font-medium shrink-0">Detected Protocol</dt>
                  <dd
                    data-testid="detected-protocol-value"
                    className="font-mono font-semibold text-slate-900 text-right"
                  >
                    {detected.protocol.toUpperCase()}
                  </dd>
                </div>
                <div className="py-2.5 flex items-baseline justify-between gap-4">
                  <dt className="text-slate-500 font-medium shrink-0">Startup Detection Source</dt>
                  <dd
                    data-testid="detected-source-value"
                    className="font-mono text-slate-800 text-right"
                  >
                    {detected.detectionSource}
                  </dd>
                </div>
                <div className="py-2.5 flex items-baseline justify-between gap-4">
                  <dt className="text-slate-500 font-medium shrink-0">Startup GDK_BACKEND</dt>
                  <dd className="font-mono text-slate-800 text-right">
                    {detected.gdkBackend}
                  </dd>
                </div>
                <div className="py-2.5 flex items-baseline justify-between gap-4">
                  <dt className="text-slate-500 font-medium shrink-0">Desktop Session</dt>
                  <dd className="font-mono text-slate-800 text-right">
                    {detected.desktopEnvironment}
                  </dd>
                </div>
                <div className="py-2.5 flex items-baseline justify-between gap-4">
                  <dt className="text-slate-500 font-medium shrink-0">Screen Capture Driver</dt>
                  <dd
                    data-testid="detected-capture-driver"
                    className="font-mono text-slate-800 text-right"
                  >
                    {detected.captureDriver}
                  </dd>
                </div>
                <div className="py-2.5 flex items-baseline justify-between gap-4">
                  <dt className="text-slate-500 font-medium shrink-0">Clipboard Persistence</dt>
                  <dd
                    data-testid="detected-clipboard-driver"
                    className="font-mono text-slate-800 text-right"
                  >
                    {detected.clipboardDriver}
                  </dd>
                </div>
                <div className="py-2.5 flex items-baseline justify-between gap-4">
                  <dt className="text-slate-500 font-medium shrink-0">Window Snip Strategy</dt>
                  <dd className="font-mono text-slate-800 text-right">
                    {detected.windowSnipStrategy}
                  </dd>
                </div>
              </dl>
            </div>

            <p className="mt-4 pt-3 border-t border-slate-200 text-xs text-slate-500">
              Note: Changing the active preset above also updates the simulated Debian desktop inside
              the Snipping Studio so you can test captures under both Wayland and X11 sessions.
            </p>
          </div>
        </div>
      </div>
    </section>
  );
};
