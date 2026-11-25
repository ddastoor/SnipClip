import React from 'react';
import { act, fireEvent, render, screen, waitFor, within } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';
import App from '../src/App';
import releaseData from '../src/data/releaseInfo.json';
import { DebReleaseSection } from '../src/components/DebReleaseSection';
import { MAX_EDITOR_ZOOM } from '../src/components/SnipStudio';
import { DESKTOP_WIDTH, DESKTOP_HEIGHT, SIMULATED_WINDOWS } from '../src/utils/desktopScene';
import { LAUNCHER_WINDOW_HEIGHT, LAUNCHER_WINDOW_WIDTH } from '../src/utils/snipEngine';

const RELEASES_URL = 'https://github.com/ddastoor/SnipClip/releases';
const REPO_URL = 'https://github.com/ddastoor/SnipClip';

/** jsdom lays nothing out, so give the editor canvas a 1:1 on-screen box matching the image. */
function stubCanvasBox(canvas: HTMLElement, width: number, height: number): void {
  vi.spyOn(canvas, 'getBoundingClientRect').mockReturnValue({
    x: 0,
    y: 0,
    left: 0,
    top: 0,
    right: width,
    bottom: height,
    width,
    height,
    toJSON: () => ({}),
  });
}

function openSampleEditor(): HTMLElement {
  render(<App />);
  fireEvent.click(screen.getByRole('button', { name: 'Load sample window snip' }));
  return screen.getByTestId('snip-editor-canvas');
}

function setClipboard(value: Partial<Clipboard> | undefined): void {
  Object.defineProperty(window.navigator, 'clipboard', {
    configurable: true,
    value,
  });
}

describe('SnipStudio compact launcher (snip-launcher-window)', () => {
  it('opens as the compact 460x200 Windows 11 launcher frame with no image loaded', () => {
    render(<App />);
    const launcher = screen.getByTestId('snip-launcher-window');
    expect(LAUNCHER_WINDOW_WIDTH).toBe(460);
    expect(LAUNCHER_WINDOW_HEIGHT).toBe(200);
    expect(launcher.style.maxWidth).toBe('460px');
    expect(launcher.style.minHeight).toBe('200px');
    expect(within(launcher).getByText('460×200')).toBeTruthy();
    expect(screen.queryByTestId('snip-editor-window')).toBeNull();
    expect(screen.queryByTestId('capture-overlay-canvas')).toBeNull();
  });
});

describe('Capture overlay (capture-overlay-canvas)', () => {
  it('clicking + New opens the fullscreen capture overlay and Escape returns to the launcher', () => {
    render(<App />);
    fireEvent.click(screen.getByRole('button', { name: 'New' }));

    const overlay = screen.getByTestId('capture-overlay-canvas') as HTMLCanvasElement;
    expect(overlay.width).toBe(DESKTOP_WIDTH);
    expect(overlay.height).toBe(DESKTOP_HEIGHT);
    expect(screen.queryByTestId('snip-launcher-window')).toBeNull();

    fireEvent.keyDown(window, { key: 'Escape' });
    expect(screen.queryByTestId('capture-overlay-canvas')).toBeNull();
    expect(screen.getByTestId('snip-launcher-window')).toBeTruthy();
  });

  it('captures a rectangular drag inside the overlay into the editor', () => {
    render(<App />);
    fireEvent.click(screen.getByRole('button', { name: 'New' }));
    const overlay = screen.getByTestId('capture-overlay-canvas');
    stubCanvasBox(overlay, DESKTOP_WIDTH, DESKTOP_HEIGHT);

    fireEvent.pointerDown(overlay, { clientX: 100, clientY: 80, pointerId: 1 });
    fireEvent.pointerMove(overlay, { clientX: 340, clientY: 260, pointerId: 1 });
    fireEvent.pointerUp(overlay, { clientX: 340, clientY: 260, pointerId: 1 });

    const canvas = screen.getByTestId('snip-editor-canvas');
    expect(canvas.getAttribute('data-width')).toBe('240');
    expect(canvas.getAttribute('data-height')).toBe('180');
  });
});

describe('Annotation editor (snip-editor-window / snip-editor-canvas)', () => {
  it('loads the sample window snip and transitions from launcher to editor', () => {
    const canvas = openSampleEditor();
    const win = SIMULATED_WINDOWS[0];
    expect(screen.getByTestId('snip-editor-window')).toBeTruthy();
    expect(screen.queryByTestId('snip-launcher-window')).toBeNull();
    expect(canvas.getAttribute('data-width')).toBe(String(win.width));
    expect(canvas.getAttribute('data-height')).toBe(String(win.height));
    expect(canvas.getAttribute('data-annotations')).toBe('0');

    fireEvent.click(screen.getByRole('button', { name: 'Close snip' }));
    expect(screen.getByTestId('snip-launcher-window')).toBeTruthy();
  });

  it('drawing a pen stroke increments data-annotations and Undo reverts it', () => {
    const canvas = openSampleEditor();
    const win = SIMULATED_WINDOWS[0];
    stubCanvasBox(canvas, win.width, win.height);

    fireEvent.pointerDown(canvas, { clientX: 40, clientY: 40, pointerId: 1 });
    fireEvent.pointerMove(canvas, { clientX: 120, clientY: 90, pointerId: 1 });
    fireEvent.pointerMove(canvas, { clientX: 200, clientY: 140, pointerId: 1 });
    fireEvent.pointerUp(canvas, { clientX: 200, clientY: 140, pointerId: 1 });
    expect(canvas.getAttribute('data-annotations')).toBe('1');

    fireEvent.click(screen.getByRole('button', { name: 'Arrow' }));
    fireEvent.pointerDown(canvas, { clientX: 300, clientY: 60, pointerId: 1 });
    fireEvent.pointerMove(canvas, { clientX: 420, clientY: 200, pointerId: 1 });
    fireEvent.pointerUp(canvas, { clientX: 420, clientY: 200, pointerId: 1 });
    expect(canvas.getAttribute('data-annotations')).toBe('2');

    fireEvent.click(screen.getByRole('button', { name: 'Undo' }));
    expect(canvas.getAttribute('data-annotations')).toBe('1');
  });

  it('zooms up to 400% via the toolbar and supports Ctrl + Wheel zoom', () => {
    const canvas = openSampleEditor();
    const zoomLabel = screen.getByTestId('editor-zoom-level');
    const zoomIn = screen.getByRole('button', { name: 'Zoom in' }) as HTMLButtonElement;
    expect(MAX_EDITOR_ZOOM).toBe(4);
    expect(zoomLabel.textContent).toBe('100%');

    for (let i = 0; i < 20; i++) {
      fireEvent.click(zoomIn);
    }
    expect(zoomLabel.textContent).toBe('400%');
    expect(canvas.getAttribute('data-zoom')).toBe('4');
    expect(zoomIn.disabled).toBe(true);

    fireEvent.click(screen.getByRole('button', { name: 'Reset zoom to 100%' }));
    expect(zoomLabel.textContent).toBe('100%');

    fireEvent.wheel(canvas, { deltaY: -120, ctrlKey: true });
    expect(zoomLabel.textContent).toBe('125%');
    fireEvent.wheel(canvas, { deltaY: 120, ctrlKey: true });
    fireEvent.wheel(canvas, { deltaY: 120, ctrlKey: true });
    expect(zoomLabel.textContent).toBe('75%');

    // Plain wheel without Ctrl (and away from the hidden ruler) leaves zoom untouched
    fireEvent.wheel(canvas, { deltaY: -120 });
    expect(zoomLabel.textContent).toBe('75%');
  });

  it('rotates the visible ruler by ±5° when the wheel is scrolled over it', () => {
    const canvas = openSampleEditor();
    const win = SIMULATED_WINDOWS[0];
    stubCanvasBox(canvas, win.width, win.height);

    fireEvent.click(screen.getByRole('button', { name: 'Toggle Ruler' }));
    const angle = screen.getByLabelText('Angle') as HTMLInputElement;
    expect(angle.value).toBe('0');

    const centerX = Math.round(win.width / 2);
    const centerY = Math.round(win.height / 2);
    fireEvent.wheel(canvas, { deltaY: -100, clientX: centerX, clientY: centerY });
    expect(angle.value).toBe('5');
    fireEvent.wheel(canvas, { deltaY: -100, clientX: centerX, clientY: centerY });
    expect(angle.value).toBe('10');
    fireEvent.wheel(canvas, { deltaY: 100, clientX: centerX, clientY: centerY });
    expect(angle.value).toBe('5');

    // Wheel far outside the ruler body does not rotate it
    fireEvent.wheel(canvas, { deltaY: -100, clientX: 2, clientY: 2 });
    expect(angle.value).toBe('5');
  });

  it('exports PNG by default and JPEG after toggling the export format', () => {
    openSampleEditor();
    const win = SIMULATED_WINDOWS[0];
    const downloads: Array<{ name: string; href: string }> = [];
    vi.spyOn(HTMLAnchorElement.prototype, 'click').mockImplementation(function (
      this: HTMLAnchorElement
    ) {
      downloads.push({ name: this.download, href: this.href });
    });

    const saveButton = screen.getByTestId('save-snip-button');
    expect(saveButton.textContent).toContain('Save PNG');
    fireEvent.click(saveButton);
    expect(downloads[0].name).toBe(`snipclip-${releaseData.version}-${win.width}x${win.height}.png`);
    expect(downloads[0].href.startsWith('data:image/png')).toBe(true);

    fireEvent.click(screen.getByRole('button', { name: 'Export as JPEG' }));
    expect(screen.getByRole('button', { name: 'Export as JPEG' }).getAttribute('aria-pressed')).toBe(
      'true'
    );
    expect(saveButton.textContent).toContain('Save JPEG');
    fireEvent.click(saveButton);
    expect(downloads[1].name).toBe(`snipclip-${releaseData.version}-${win.width}x${win.height}.jpg`);
    expect(downloads[1].href.startsWith('data:image/jpeg')).toBe(true);

    fireEvent.click(screen.getByRole('button', { name: 'Export as PNG' }));
    expect(saveButton.textContent).toContain('Save PNG');
  });

  it('never shows a false "Copied" state when the Clipboard API is unavailable', async () => {
    setClipboard(undefined);
    try {
      openSampleEditor();
      const copyButton = screen.getByTestId('copy-snip-button');
      await act(async () => {
        fireEvent.click(copyButton);
      });
      expect(await screen.findByText(/Clipboard image copy is blocked/)).toBeTruthy();
      expect(copyButton.textContent).toBe('Copy');
    } finally {
      setClipboard(undefined);
    }
  });

  it('shows "Copied" only after navigator.clipboard.write actually succeeds', async () => {
    const write = vi.fn<(items: ClipboardItem[]) => Promise<void>>().mockResolvedValue(undefined);
    class FakeClipboardItem {
      constructor(public readonly items: Record<string, Blob>) {}
    }
    vi.stubGlobal('ClipboardItem', FakeClipboardItem);
    setClipboard({ write });
    try {
      openSampleEditor();
      const copyButton = screen.getByTestId('copy-snip-button');
      await act(async () => {
        fireEvent.click(copyButton);
      });
      await waitFor(() => expect(copyButton.textContent).toBe('Copied'));
      expect(write).toHaveBeenCalledTimes(1);
      expect(screen.getByText(/^Copied PNG \(/)).toBeTruthy();
    } finally {
      setClipboard(undefined);
    }
  });

  it('leaves Crop mode when a new snip replaces the image', () => {
    openSampleEditor();
    fireEvent.click(screen.getByRole('button', { name: 'Crop' }));
    expect(screen.getByRole('button', { name: 'Apply Crop' })).toBeTruthy();
    expect(screen.getByRole('button', { name: 'Crop' }).getAttribute('aria-pressed')).toBe('true');

    // Start a new Window snip from the editor and capture a different window
    fireEvent.click(screen.getByRole('button', { name: 'New' }));
    fireEvent.click(screen.getByRole('button', { name: 'Window' }));
    const target = SIMULATED_WINDOWS[1];
    fireEvent.click(screen.getAllByRole('button', { name: /^Capture Window:/ })[1]);

    const canvas = screen.getByTestId('snip-editor-canvas');
    expect(canvas.getAttribute('data-width')).toBe(String(target.width));
    expect(screen.queryByRole('button', { name: 'Apply Crop' })).toBeNull();
    expect(screen.getByRole('button', { name: 'Crop' }).getAttribute('aria-pressed')).toBe('false');
    expect(screen.getByRole('button', { name: 'Pen' }).getAttribute('aria-pressed')).toBe('true');
  });
});

describe('Display detection telemetry (detected-* hooks)', () => {
  it('updates protocol, source, capture, and clipboard drivers live when switching presets', () => {
    render(<App />);
    const protocol = screen.getByTestId('detected-protocol-value');
    const source = screen.getByTestId('detected-source-value');
    const capture = screen.getByTestId('detected-capture-driver');
    const clipboard = screen.getByTestId('detected-clipboard-driver');

    // Default: Debian 12 GNOME (Wayland)
    expect(protocol.textContent).toBe('WAYLAND');
    expect(source.textContent).toBe('XDG_SESSION_TYPE=wayland');
    expect(capture.textContent).toBe('xdg-desktop-portal (GDBus Screenshot)');
    expect(clipboard.textContent).toBe('wl-copy + GtkClipboard');

    fireEvent.click(screen.getByRole('button', { name: 'KDE Plasma 6 (Wayland)' }));
    expect(capture.textContent).toBe('xdg-desktop-portal (GDBus Screenshot)');
    expect(
      screen.getByRole('button', { name: 'KDE Plasma 6 (Wayland)' }).getAttribute('aria-pressed')
    ).toBe('true');

    fireEvent.click(screen.getByRole('button', { name: 'Sway / Hyprland (Wayland)' }));
    expect(protocol.textContent).toBe('WAYLAND');
    expect(capture.textContent).toBe('grim (wlroots Screencopy)');
    expect(clipboard.textContent).toBe('wl-copy + GtkClipboard');

    fireEvent.click(screen.getByRole('button', { name: 'Debian XFCE / Cinnamon (X11)' }));
    expect(protocol.textContent).toBe('X11');
    expect(source.textContent).toBe('XDG_SESSION_TYPE=x11');
    expect(capture.textContent).toBe('GTK3 / GdkPixbuf Root Grab (gdk_pixbuf_get_from_window)');
    expect(clipboard.textContent).toBe('xclip + GtkClipboard');

    fireEvent.click(screen.getByRole('button', { name: 'i3wm Minimal Xorg (X11)' }));
    expect(protocol.textContent).toBe('X11');
    expect(source.textContent).toBe('DISPLAY=:0');
    expect(clipboard.textContent).toBe('xclip + GtkClipboard');

    // Unticking xclip falls back to GtkClipboard (xsel is never claimed for PNG images)
    fireEvent.click(screen.getByRole('checkbox', { name: 'xclip' }));
    expect(clipboard.textContent).toBe('GtkClipboard');
  });

  it('prefers spectacle over gnome-screenshot once portal and grim are unavailable', () => {
    render(<App />);
    const capture = screen.getByTestId('detected-capture-driver');
    fireEvent.click(screen.getByRole('button', { name: 'KDE Plasma 6 (Wayland)' }));
    fireEvent.click(screen.getByRole('checkbox', { name: 'gnome-screenshot' }));
    fireEvent.click(screen.getByRole('checkbox', { name: 'xdg-desktop-portal' }));
    expect(capture.textContent).toBe('spectacle (KDE)');

    fireEvent.click(screen.getByRole('checkbox', { name: 'spectacle' }));
    expect(capture.textContent).toBe('gnome-screenshot');

    fireEvent.click(screen.getByRole('checkbox', { name: 'gnome-screenshot' }));
    expect(capture.textContent).toBe('gdk_pixbuf_get_from_window (XWayland fallback)');
  });
});

describe('Debian release section (synced-deb-filename)', () => {
  it('updates the synced .deb filename preview when the release tag changes', () => {
    render(<DebReleaseSection release={releaseData} />);
    const filename = screen.getByTestId('synced-deb-filename');
    expect(filename.textContent).toBe(releaseData.debFilename);

    const tagInput = screen.getByLabelText('GitHub Release Tag');
    fireEvent.change(tagInput, { target: { value: 'v3.4.5' } });
    expect(screen.getByTestId('synced-deb-filename').textContent).toBe('snipclip_3.4.5_amd64.deb');

    fireEvent.change(tagInput, { target: { value: '2.2.0-rc1' } });
    expect(screen.getByTestId('synced-deb-filename').textContent).toBe(
      'snipclip_2.2.0-rc1_amd64.deb'
    );

    fireEvent.change(tagInput, { target: { value: 'not-a-version' } });
    expect(screen.queryByTestId('synced-deb-filename')).toBeNull();
    expect(screen.getByRole('alert').textContent).toMatch(/Debian semantic format/);
  });

  it('links outbound to the GitHub releases page in the section, header, and footer', () => {
    render(<App />);
    const sectionLink = screen.getByTestId('github-releases-link');
    expect(sectionLink.getAttribute('href')).toBe(RELEASES_URL);
    expect(sectionLink.getAttribute('target')).toBe('_blank');
    expect(sectionLink.getAttribute('rel')).toBe('noopener noreferrer');

    const links = screen.getAllByRole('link');
    const releaseLinks = links.filter((a) => a.getAttribute('href') === RELEASES_URL);
    expect(releaseLinks.length).toBeGreaterThanOrEqual(3);
    for (const a of releaseLinks) {
      expect(a.getAttribute('rel')).toBe('noopener noreferrer');
    }
    expect(screen.getByRole('link', { name: 'SnipClip GitHub releases' })).toBeTruthy();
    expect(links.some((a) => a.getAttribute('href') === REPO_URL)).toBe(true);
  });

  it('renders the real control metadata, workflow YAML, and all 16 packaged files', () => {
    render(<DebReleaseSection release={releaseData} />);
    const pre = () => document.querySelector('pre code')?.textContent ?? '';

    expect(pre()).toContain('Maintainer: Darayus Dastoor <ddastoor@users.noreply.github.com>');
    expect(pre()).toContain('Homepage: https://github.com/ddastoor/SnipClip');
    expect(pre()).not.toContain('snipclip.org');

    fireEvent.click(screen.getByRole('tab', { name: 'release-deb.yml' }));
    expect(pre()).toContain('name: Build and Release Debian (.deb) Package');
    expect(pre()).toContain('debian:bookworm');

    fireEvent.click(screen.getByRole('tab', { name: '.deb File Tree' }));
    const files = pre()
      .split('\n')
      .map((line) => line.trim().split(/\s+/)[0])
      .filter((p) => p.startsWith('/usr/'));
    expect(files).toHaveLength(16);
    expect(files).toContain('/usr/share/snipclip/snipclip.png');
    expect(files).toContain('/usr/bin/snipclip');
  });

  it('surfaces an honest fallback instead of "Copied" when clipboard writes are blocked', async () => {
    const writeText = vi
      .fn<(text: string) => Promise<void>>()
      .mockRejectedValue(new DOMException('denied', 'NotAllowedError'));
    setClipboard({ writeText });
    try {
      render(<DebReleaseSection release={releaseData} />);
      await act(async () => {
        fireEvent.click(screen.getByRole('button', { name: 'Copy apt install command' }));
      });
      expect(writeText).toHaveBeenCalledWith(`sudo apt install ./${releaseData.debFilename}`);
      const status = await screen.findByTestId('copy-fallback-status');
      expect(status.textContent).toMatch(/copy it manually/);
    } finally {
      setClipboard(undefined);
    }
  });

  it('clears the fallback message and confirms the copy when writeText succeeds', async () => {
    const writeText = vi.fn<(text: string) => Promise<void>>().mockResolvedValue(undefined);
    setClipboard({ writeText });
    try {
      render(<DebReleaseSection release={releaseData} />);
      await act(async () => {
        fireEvent.click(screen.getByRole('button', { name: 'Copy verification command' }));
      });
      expect(writeText).toHaveBeenCalledWith('snipclip --version && sha256sum -c SHA256SUMS');
      expect(screen.queryByTestId('copy-fallback-status')).toBeNull();
    } finally {
      setClipboard(undefined);
    }
  });
});
