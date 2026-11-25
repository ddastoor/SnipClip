import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {
  AnnotationTool,
  computePolygonBounds,
  computeSnipWindowDimensions,
  DEFAULT_CUSTOM_SHORTCUT,
  detectLinuxDisplaySession,
  DisplayProtocol,
  EDITOR_CHROME_HEIGHT,
  EDITOR_CHROME_WIDTH,
  EDITOR_MIN_HEIGHT,
  EDITOR_MIN_WIDTH,
  evaluateCaptureShortcut,
  fitRulerToCanvas,
  formatShortcutBinding,
  hitTestAnnotation,
  isEditableElement,
  LAUNCHER_WINDOW_HEIGHT,
  LAUNCHER_WINDOW_WIDTH,
  matchesShortcutBinding,
  normalizeRect,
  normalizeShortcutKey,
  parseShortcutString,
  resolveVersionReleaseMetadata,
  snapPointToRuler,
  validateRecordedShortcut,
} from './snipEngine';
import { decodeBase64ToBytes, isValidDebianArchiveBytes } from './debDownload';

const defaultBins = {
  gdbus: true,
  grim: false,
  gnomeScreenshot: false,
  spectacle: false,
  wlCopy: true,
  xclip: true,
  xsel: false,
};

test('detects Wayland session via XDG_SESSION_TYPE and selects portal + wl-copy', () => {
  const res = detectLinuxDisplaySession(
    {
      XDG_SESSION_TYPE: 'wayland',
      WAYLAND_DISPLAY: 'wayland-0',
      DISPLAY: ':0',
      XDG_CURRENT_DESKTOP: 'GNOME',
    },
    defaultBins
  );
  assert.equal(res.protocol, DisplayProtocol.Wayland);
  assert.equal(res.detectionSource, 'XDG_SESSION_TYPE=wayland');
  assert.equal(res.gdkBackend, 'wayland,x11');
  assert.equal(res.clipboardDriver, 'wl-copy + GtkClipboard');
  assert.match(res.captureDriver, /xdg-desktop-portal/);
  assert.equal(res.supportsX11WindowEnumeration, false);
});

test('detects X11 session and enables native libX11 _NET_CLIENT_LIST_STACKING window enumeration', () => {
  const res = detectLinuxDisplaySession(
    {
      XDG_SESSION_TYPE: 'x11',
      WAYLAND_DISPLAY: '',
      DISPLAY: ':0.0',
      XDG_CURRENT_DESKTOP: 'XFCE',
    },
    defaultBins
  );
  assert.equal(res.protocol, DisplayProtocol.X11);
  assert.equal(res.detectionSource, 'XDG_SESSION_TYPE=x11');
  assert.equal(res.gdkBackend, 'x11');
  assert.equal(res.clipboardDriver, 'xclip + GtkClipboard');
  assert.equal(res.supportsX11WindowEnumeration, true);
});

test('prefers Wayland socket over XWayland DISPLAY when XDG_SESSION_TYPE is unset', () => {
  const res = detectLinuxDisplaySession(
    {
      XDG_SESSION_TYPE: '',
      WAYLAND_DISPLAY: 'wayland-1',
      DISPLAY: ':0',
      XDG_CURRENT_DESKTOP: 'sway',
    },
    { ...defaultBins, gdbus: false, grim: true }
  );
  assert.equal(res.protocol, DisplayProtocol.Wayland);
  assert.equal(res.detectionSource, 'WAYLAND_DISPLAY=wayland-1');
  assert.match(res.captureDriver, /grim/);
});

test('falls back through Wayland screenshot drivers (gnome-screenshot, spectacle, XWayland)', () => {
  const env = {
    XDG_SESSION_TYPE: 'wayland',
    WAYLAND_DISPLAY: 'wayland-0',
    DISPLAY: ':0',
    XDG_CURRENT_DESKTOP: 'KDE',
  };
  const kdeRes = detectLinuxDisplaySession(env, {
    ...defaultBins,
    gdbus: false,
    grim: false,
    spectacle: true,
    wlCopy: false,
  });
  assert.equal(kdeRes.captureDriver, 'spectacle (KDE)');
  assert.equal(kdeRes.clipboardDriver, 'GtkClipboard');

  const xwaylandRes = detectLinuxDisplaySession(env, {
    ...defaultBins,
    gdbus: false,
    grim: false,
    gnomeScreenshot: false,
    spectacle: false,
  });
  assert.equal(xwaylandRes.captureDriver, 'gdk_pixbuf_get_from_window (XWayland fallback)');
});

test('falls back through X11 clipboard drivers (xclip -> GtkClipboard, xsel ignored)', () => {
  const env = {
    XDG_SESSION_TYPE: 'x11',
    WAYLAND_DISPLAY: '',
    DISPLAY: ':0',
    XDG_CURRENT_DESKTOP: 'i3',
  };
  const xclipRes = detectLinuxDisplaySession(env, { ...defaultBins, xclip: true, xsel: true });
  assert.equal(xclipRes.clipboardDriver, 'xclip + GtkClipboard');
  assert.equal(xclipRes.supportsX11WindowEnumeration, true);

  // xsel has no PNG image-target support in the native backend, so it never claims the clipboard
  const xselOnlyRes = detectLinuxDisplaySession(env, { ...defaultBins, xclip: false, xsel: true });
  assert.equal(xselOnlyRes.clipboardDriver, 'GtkClipboard');

  const gtkRes = detectLinuxDisplaySession(env, {
    ...defaultBins,
    xclip: false,
    xsel: false,
  });
  assert.equal(gtkRes.clipboardDriver, 'GtkClipboard');
});

test('Wayland capture priority mirrors native backend (wlroots grim -> portal -> grim -> spectacle -> gnome-screenshot)', () => {
  const waylandEnv = (desktop: string) => ({
    XDG_SESSION_TYPE: 'wayland',
    WAYLAND_DISPLAY: 'wayland-0',
    DISPLAY: ':0',
    XDG_CURRENT_DESKTOP: desktop,
  });

  // wlroots compositors (sway/Hyprland/river/Wayfire) prefer grim even when the portal exists
  for (const desktop of ['sway', 'Hyprland', 'river', 'wayfire:wlroots']) {
    const res = detectLinuxDisplaySession(waylandEnv(desktop), {
      ...defaultBins,
      gdbus: true,
      grim: true,
    });
    assert.equal(res.captureDriver, 'grim (wlroots Screencopy)', desktop);
  }

  // wlroots without grim falls back to the portal
  assert.equal(
    detectLinuxDisplaySession(waylandEnv('sway'), { ...defaultBins, gdbus: true, grim: false })
      .captureDriver,
    'xdg-desktop-portal (GDBus Screenshot)'
  );

  // GNOME with gdbus (and even grim installed) selects the portal
  assert.equal(
    detectLinuxDisplaySession(waylandEnv('GNOME'), { ...defaultBins, gdbus: true, grim: true })
      .captureDriver,
    'xdg-desktop-portal (GDBus Screenshot)'
  );

  // Non-wlroots desktop without gdbus but with grim uses grim
  assert.equal(
    detectLinuxDisplaySession(waylandEnv('GNOME'), { ...defaultBins, gdbus: false, grim: true })
      .captureDriver,
    'grim (wlroots Screencopy)'
  );

  // spectacle beats gnome-screenshot when neither gdbus nor grim are available
  assert.equal(
    detectLinuxDisplaySession(waylandEnv('KDE'), {
      ...defaultBins,
      gdbus: false,
      grim: false,
      spectacle: true,
      gnomeScreenshot: true,
    }).captureDriver,
    'spectacle (KDE)'
  );

  assert.equal(
    detectLinuxDisplaySession(waylandEnv('GNOME'), {
      ...defaultBins,
      gdbus: false,
      grim: false,
      spectacle: false,
      gnomeScreenshot: true,
    }).captureDriver,
    'gnome-screenshot'
  );
});

test('returns Headless protocol when neither Wayland nor X11 variables exist', () => {
  const res = detectLinuxDisplaySession(
    {
      XDG_SESSION_TYPE: '',
      WAYLAND_DISPLAY: '',
      DISPLAY: '',
      XDG_CURRENT_DESKTOP: '',
    },
    defaultBins
  );
  assert.equal(res.protocol, DisplayProtocol.Headless);
  assert.equal(res.gdkBackend, 'broadway');
  assert.equal(res.desktopEnvironment, 'Generic Linux Desktop');
});

test('normalizes inverted drag coordinates and clamps to desktop bounds', () => {
  const rect = normalizeRect({ x: 350, y: 280 }, { x: 100, y: -20 }, 1120, 640);
  assert.deepEqual(rect, { x: 100, y: 0, width: 250, height: 280 });

  const clamped = normalizeRect({ x: 1000, y: 500 }, { x: 1400, y: 900 }, 1120, 640);
  assert.deepEqual(clamped, { x: 1000, y: 500, width: 120, height: 140 });

  // Boundary rounding: Math.round(99.5) = 100 and Math.round(0.5) = 1 would overflow to 101
  const edge = normalizeRect({ x: 99.5, y: 99.5 }, { x: 100, y: 100 }, 100, 100);
  assert.ok(edge.x + edge.width <= 100, `x + width = ${edge.x + edge.width}`);
  assert.ok(edge.y + edge.height <= 100, `y + height = ${edge.y + edge.height}`);
  assert.deepEqual(edge, { x: 100, y: 100, width: 0, height: 0 });

  const nearEdge = normalizeRect({ x: 10.5, y: 20.5 }, { x: 200, y: 200 }, 100, 100);
  assert.ok(nearEdge.x + nearEdge.width <= 100);
  assert.ok(nearEdge.y + nearEdge.height <= 100);
});

test('computes freeform lasso bounding box and rejects tiny degenerate paths', () => {
  assert.equal(computePolygonBounds([{ x: 10, y: 10 }, { x: 40, y: 40 }], 800, 600), null);

  const tiny = computePolygonBounds(
    [
      { x: 10, y: 10 },
      { x: 12, y: 11 },
      { x: 11, y: 13 },
    ],
    800,
    600
  );
  assert.equal(tiny, null);

  const valid = computePolygonBounds(
    [
      { x: 40, y: 50 },
      { x: 240, y: 65 },
      { x: 190, y: 210 },
    ],
    800,
    600
  );
  assert.deepEqual(valid, { x: 40, y: 50, width: 200, height: 160 });
});

test('fits ruler cleanly inside narrow and wide captured canvases while preserving visibility and angle', () => {
  const fitted = fitRulerToCanvas(260, 190, { visible: true, angleDeg: 35 });
  assert.equal(fitted.visible, true);
  assert.equal(fitted.angleDeg, 35);
  assert.equal(fitted.x, 130);
  assert.equal(fitted.y, 95);
  assert.ok(fitted.length <= 210);
});

test('snaps pen points cleanly onto horizontal and rotated ruler edges', () => {
  const horizontalRuler = {
    visible: true,
    x: 300,
    y: 200,
    angleDeg: 0,
    length: 420,
    halfThickness: 28,
  };
  const snappedBottom = snapPointToRuler({ x: 340, y: 235 }, horizontalRuler, 16);
  assert.equal(Math.round(snappedBottom.x), 340);
  assert.equal(Math.round(snappedBottom.y), 228);

  const snappedTop = snapPointToRuler({ x: 280, y: 166 }, horizontalRuler, 16);
  assert.equal(Math.round(snappedTop.x), 280);
  assert.equal(Math.round(snappedTop.y), 172);

  // Hidden ruler never alters coordinates
  const hidden = snapPointToRuler({ x: 340, y: 235 }, { ...horizontalRuler, visible: false }, 16);
  assert.deepEqual(hidden, { x: 340, y: 235 });

  // Vertical 90-degree ruler snaps X to 300 - 28 = 272
  const verticalRuler = { ...horizontalRuler, angleDeg: 90 };
  const snappedVert = snapPointToRuler({ x: 266, y: 220 }, verticalRuler, 16);
  assert.equal(Math.round(snappedVert.x), 272);
  assert.equal(Math.round(snappedVert.y), 220);
});

test('hit-tests annotations accurately for stroke eraser across shapes and polylines', () => {
  const arrowOp = {
    id: 'a1',
    tool: AnnotationTool.Arrow,
    color: '#ef4444',
    strokeWidth: 3,
    points: [
      { x: 50, y: 50 },
      { x: 250, y: 50 },
    ],
  };
  assert.equal(hitTestAnnotation({ x: 150, y: 56 }, arrowOp, 12), true);
  assert.equal(hitTestAnnotation({ x: 150, y: 95 }, arrowOp, 12), false);

  const penOp = {
    id: 'p1',
    tool: AnnotationTool.Pen,
    color: '#2563eb',
    strokeWidth: 4,
    points: [
      { x: 20, y: 20 },
      { x: 120, y: 20 },
      { x: 120, y: 120 },
    ],
  };
  assert.equal(hitTestAnnotation({ x: 124, y: 75 }, penOp, 10), true);
  assert.equal(hitTestAnnotation({ x: 60, y: 75 }, penOp, 10), false);

  const redactOp = {
    id: 'r1',
    tool: AnnotationTool.Redact,
    color: '#0f172a',
    strokeWidth: 3,
    points: [
      { x: 200, y: 100 },
      { x: 340, y: 160 },
    ],
  };
  assert.equal(hitTestAnnotation({ x: 270, y: 130 }, redactOp, 10), true);
});

test('synchronizes GitHub release tag to .deb filename and CLI version string', () => {
  const synced = resolveVersionReleaseMetadata('v2.0.0');
  assert.equal(synced.valid, true);
  assert.equal(synced.version, '2.0.0');
  assert.equal(synced.releaseTag, 'v2.0.0');
  assert.equal(synced.debFilename, 'snipclip_2.0.0_amd64.deb');
  assert.match(synced.cliBanner, /SnipClip 2\.0\.0 \(snipclip_2\.0\.0_amd64\.deb, release v2\.0\.0\)/);

  const prerelease = resolveVersionReleaseMetadata('2.1.0-rc1');
  assert.equal(prerelease.valid, true);
  assert.equal(prerelease.debFilename, 'snipclip_2.1.0-rc1_amd64.deb');

  assert.equal(resolveVersionReleaseMetadata('').valid, false);
  assert.equal(resolveVersionReleaseMetadata('not-a-semver').valid, false);
});

test('normalizes PrintScreen, SysRq, function keys, and layout-independent Key/Digit codes', () => {
  assert.equal(normalizeShortcutKey('PrintScreen'), 'PrintScreen');
  assert.equal(normalizeShortcutKey('SysRq', 'PrintScreen'), 'PrintScreen');
  assert.equal(normalizeShortcutKey('Unidentified', 'PrintScreen'), 'PrintScreen');
  assert.equal(normalizeShortcutKey('ß', 'KeyS'), 'S');
  assert.equal(normalizeShortcutKey('@', 'Digit2'), '2');
  assert.equal(normalizeShortcutKey('f9'), 'F9');
  assert.equal(normalizeShortcutKey(' ', 'Space'), 'Space');
});

test('formats, parses, and validates recorded custom keyboard shortcuts cleanly', () => {
  // Modifier-only press stays pending while user continues holding chord
  const modOnly = validateRecordedShortcut({ key: 'Control', ctrlKey: true });
  assert.equal(modOnly.valid, false);
  assert.equal(modOnly.pendingModifierOnly, true);

  // Bare single character without modifier is rejected with helpful guidance
  const bareChar = validateRecordedShortcut({ key: 's', code: 'KeyS' });
  assert.equal(bareChar.valid, false);
  assert.ok(bareChar.error && bareChar.error.includes('Ctrl, Alt, or Shift'));

  // Reserved Undo/Redo combinations cannot be overwritten
  const reservedUndo = validateRecordedShortcut({ key: 'z', code: 'KeyZ', ctrlKey: true });
  assert.equal(reservedUndo.valid, false);
  assert.match(reservedUndo.error ?? '', /reserved for Undo and Redo/);

  // Valid custom combination (Alt+Shift+K)
  const validChord = validateRecordedShortcut({
    key: 'K',
    code: 'KeyK',
    altKey: true,
    shiftKey: true,
  });
  assert.equal(validChord.valid, true);
  assert.deepEqual(validChord.binding, {
    ctrlOrMeta: false,
    alt: true,
    shift: true,
    key: 'K',
  });
  const formatted = formatShortcutBinding(validChord.binding!);
  assert.equal(formatted, 'Alt+Shift+K');
  assert.deepEqual(parseShortcutString(formatted), validChord.binding);

  // Standalone F-key and PrintScreen are allowed without modifiers
  const fKey = validateRecordedShortcut({ key: 'F9', code: 'F9' });
  assert.equal(fKey.valid, true);
  assert.equal(formatShortcutBinding(fKey.binding!), 'F9');
});

test('evaluates PrintScreen and custom capture shortcuts while protecting inputs and active captures', () => {
  const baseOptions = {
    printScreenEnabled: true,
    customShortcut: DEFAULT_CUSTOM_SHORTCUT, // Ctrl+Shift+S
    isEditableTarget: false,
    isRecordingShortcut: false,
    isCapturing: false,
    isCountingDown: false,
  };

  // 1. PrintScreen triggers capture immediately
  const prtSc = evaluateCaptureShortcut(
    { key: 'PrintScreen', code: 'PrintScreen' },
    baseOptions
  );
  assert.deepEqual(prtSc, { triggered: true, matchedTrigger: 'PrintScreen' });

  // 2. Disabled PrintScreen does not trigger
  const prtScDisabled = evaluateCaptureShortcut(
    { key: 'PrintScreen', code: 'PrintScreen' },
    { ...baseOptions, printScreenEnabled: false }
  );
  assert.equal(prtScDisabled.triggered, false);

  // 3. Custom shortcut (Ctrl+Shift+S) triggers capture
  const customHit = evaluateCaptureShortcut(
    { key: 'S', code: 'KeyS', ctrlKey: true, shiftKey: true },
    baseOptions
  );
  assert.deepEqual(customHit, { triggered: true, matchedTrigger: 'Ctrl+Shift+S' });

  // 4. Custom shortcut does NOT hijack typing inside <input>, <textarea>, or <select>
  assert.equal(isEditableElement({ tagName: 'INPUT' }), true);
  assert.equal(isEditableElement({ tagName: 'DIV' }), false);
  const blockedInInput = evaluateCaptureShortcut(
    { key: 'S', code: 'KeyS', ctrlKey: true, shiftKey: true },
    { ...baseOptions, isEditableTarget: true }
  );
  assert.equal(blockedInInput.triggered, false);

  // 5. Key auto-repeat, active capture overlay, countdown, and shortcut recording suppress re-triggers
  assert.equal(
    evaluateCaptureShortcut(
      { key: 'PrintScreen', repeat: true },
      baseOptions
    ).triggered,
    false
  );
  assert.equal(
    evaluateCaptureShortcut(
      { key: 'PrintScreen' },
      { ...baseOptions, isCapturing: true }
    ).triggered,
    false
  );
  assert.equal(
    evaluateCaptureShortcut(
      { key: 'PrintScreen' },
      { ...baseOptions, isRecordingShortcut: true }
    ).triggered,
    false
  );
  assert.equal(
    matchesShortcutBinding(
      { key: 's', code: 'KeyS', metaKey: true, shiftKey: true },
      DEFAULT_CUSTOM_SHORTCUT
    ),
    true
  );
});

test('computes compact Windows 11 launcher (460x200) and dynamic editor window dimensions', () => {
  assert.equal(LAUNCHER_WINDOW_WIDTH, 460);
  assert.equal(LAUNCHER_WINDOW_HEIGHT, 200);
  assert.equal(EDITOR_MIN_WIDTH, 560);
  assert.equal(EDITOR_MIN_HEIGHT, 380);
  assert.equal(EDITOR_CHROME_WIDTH, 48);
  assert.equal(EDITOR_CHROME_HEIGHT, 136);

  // 1. Empty launcher (0x0) defaults to compact 460x200 frame
  assert.deepEqual(computeSnipWindowDimensions(0, 0), {
    width: 460,
    height: 200,
    isLauncher: true,
  });

  // Launcher clamps cleanly on very narrow workareas (> 120px)
  assert.deepEqual(computeSnipWindowDimensions(0, 0, 380, 210), {
    width: 348,
    height: 178,
    isLauncher: true,
  });

  // 2. Small snip (240x140) expands to editor minimum bounds (560x380)
  assert.deepEqual(computeSnipWindowDimensions(240, 140, 1920, 1080), {
    width: 560,
    height: 380,
    isLauncher: false,
  });

  // 3. Medium snip (620x380) sizes naturally to image + editor chrome (668x516)
  assert.deepEqual(computeSnipWindowDimensions(620, 380, 1920, 1080), {
    width: 620 + EDITOR_CHROME_WIDTH,
    height: 380 + EDITOR_CHROME_HEIGHT,
    isLauncher: false,
  });

  // 4. Fullscreen snip (1920x1080) clamps cleanly to 80% of monitor workarea (1536x864)
  assert.deepEqual(computeSnipWindowDimensions(1920, 1080, 1920, 1080), {
    width: Math.floor(1920 * 0.8),
    height: Math.floor(1080 * 0.8),
    isLauncher: false,
  });
});

test('cross-stack parity: VERSION, releaseInfo.json, C snipclip_version.h, ELF binary, and built .deb match', () => {
  const rootDir = path.resolve(import.meta.dirname, '../..');
  const rootVersion = fs.readFileSync(path.join(rootDir, 'VERSION'), 'utf-8').trim();
  const releaseJson = JSON.parse(
    fs.readFileSync(path.join(rootDir, 'src/data/releaseInfo.json'), 'utf-8')
  );
  const cVersionHeader = fs.readFileSync(
    path.join(rootDir, 'native/include/snipclip_version.h'),
    'utf-8'
  );

  assert.equal(releaseJson.version, rootVersion);
  assert.equal(releaseJson.debFilename, `snipclip_${rootVersion}_${releaseJson.architecture}.deb`);
  assert.match(
    cVersionHeader,
    new RegExp(`#define SNIPCLIP_VERSION "${rootVersion.replace(/\./g, '\\.')}"`)
  );

  // Verify compiled native ELF-64 binary exists and starts with \x7fELF
  const elfPath = path.join(rootDir, 'native/build/snipclip');
  assert.equal(fs.existsSync(elfPath), true);
  const elfBytes = fs.readFileSync(elfPath);
  assert.equal(elfBytes[0], 0x7f);
  assert.equal(elfBytes[1], 0x45); // 'E'
  assert.equal(elfBytes[2], 0x4c); // 'L'
  assert.equal(elfBytes[3], 0x46); // 'F'

  const debFilePath = path.join(rootDir, 'public/releases', releaseJson.debFilename);
  assert.equal(fs.existsSync(debFilePath), true);
  const debBytes = fs.readFileSync(debFilePath);
  const sha256 = crypto.createHash('sha256').update(debBytes).digest('hex');
  assert.equal(sha256, releaseJson.sha256);
  assert.equal(debBytes.byteLength, releaseJson.sizeBytes);

  // Verify embedded debBase64 binary payload matches exact dpkg-deb output and !<arch>\n header
  assert.equal(typeof releaseJson.debBase64, 'string');
  const embeddedBytes = decodeBase64ToBytes(releaseJson.debBase64);
  assert.equal(embeddedBytes.byteLength, releaseJson.sizeBytes);
  assert.equal(isValidDebianArchiveBytes(embeddedBytes), true);
  const embeddedSha256 = crypto.createHash('sha256').update(embeddedBytes).digest('hex');
  assert.equal(embeddedSha256, releaseJson.sha256);

  // Verify HTML interstitial pages are rejected and can never masquerade as a .deb archive
  const fakeHtml = new TextEncoder().encode(
    '<!DOCTYPE html><html><head><title>Loading...</title></head><body>Auth proxy</body></html>'
  );
  assert.equal(isValidDebianArchiveBytes(fakeHtml), false);
});
