import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {
  AnnotationTool,
  computePolygonBounds,
  detectLinuxDisplaySession,
  DisplayProtocol,
  fitRulerToCanvas,
  hitTestAnnotation,
  normalizeRect,
  resolveVersionReleaseMetadata,
  snapPointToRuler,
} from './snipEngine';

const defaultBins = {
  gdbus: true,
  grim: false,
  gnomeScreenshot: false,
  spectacle: false,
  wlCopy: true,
  xclip: true,
  xsel: false,
  xprop: true,
  xwininfo: true,
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
  assert.equal(res.qtPlatformPlugin, 'wayland;xcb');
  assert.equal(res.clipboardDriver, 'wl-copy + QClipboard');
  assert.match(res.captureDriver, /xdg-desktop-portal/);
  assert.equal(res.supportsX11WindowEnumeration, false);
});

test('detects X11 session and enables _NET_CLIENT_LIST window enumeration when x11-utils present', () => {
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
  assert.equal(res.qtPlatformPlugin, 'xcb');
  assert.equal(res.clipboardDriver, 'xclip + QClipboard');
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
  assert.equal(kdeRes.clipboardDriver, 'QClipboard');

  const xwaylandRes = detectLinuxDisplaySession(env, {
    ...defaultBins,
    gdbus: false,
    grim: false,
    gnomeScreenshot: false,
    spectacle: false,
  });
  assert.equal(xwaylandRes.captureDriver, 'qt-screen-grab (XWayland fallback)');
});

test('falls back through X11 clipboard drivers (xclip -> xsel -> QClipboard)', () => {
  const env = {
    XDG_SESSION_TYPE: 'x11',
    WAYLAND_DISPLAY: '',
    DISPLAY: ':0',
    XDG_CURRENT_DESKTOP: 'i3',
  };
  const xselRes = detectLinuxDisplaySession(env, {
    ...defaultBins,
    xclip: false,
    xsel: true,
    xprop: false,
  });
  assert.equal(xselRes.clipboardDriver, 'xsel + QClipboard');
  assert.equal(xselRes.supportsX11WindowEnumeration, false);

  const qtRes = detectLinuxDisplaySession(env, {
    ...defaultBins,
    xclip: false,
    xsel: false,
  });
  assert.equal(qtRes.clipboardDriver, 'QClipboard');
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
  assert.equal(res.qtPlatformPlugin, 'offscreen');
  assert.equal(res.desktopEnvironment, 'Generic Linux Desktop');
});

test('normalizes inverted drag coordinates and clamps to desktop bounds', () => {
  const rect = normalizeRect({ x: 350, y: 280 }, { x: 100, y: -20 }, 1120, 640);
  assert.deepEqual(rect, { x: 100, y: 0, width: 250, height: 280 });

  const clamped = normalizeRect({ x: 1000, y: 500 }, { x: 1400, y: 900 }, 1120, 640);
  assert.deepEqual(clamped, { x: 1000, y: 500, width: 120, height: 140 });
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
  const synced = resolveVersionReleaseMetadata('v1.4.2');
  assert.equal(synced.valid, true);
  assert.equal(synced.version, '1.4.2');
  assert.equal(synced.releaseTag, 'v1.4.2');
  assert.equal(synced.debFilename, 'snipclip_1.4.2_all.deb');
  assert.match(synced.cliBanner, /SnipClip 1\.4\.2 \(snipclip_1\.4\.2_all\.deb, release v1\.4\.2\)/);

  const prerelease = resolveVersionReleaseMetadata('2.0.0-rc1');
  assert.equal(prerelease.valid, true);
  assert.equal(prerelease.debFilename, 'snipclip_2.0.0-rc1_all.deb');

  assert.equal(resolveVersionReleaseMetadata('').valid, false);
  assert.equal(resolveVersionReleaseMetadata('not-a-semver').valid, false);
});

test('cross-stack parity: VERSION, releaseInfo.json, Python version.py, and built .deb match', () => {
  const rootDir = path.resolve(import.meta.dirname, '../..');
  const rootVersion = fs.readFileSync(path.join(rootDir, 'VERSION'), 'utf-8').trim();
  const releaseJson = JSON.parse(
    fs.readFileSync(path.join(rootDir, 'src/data/releaseInfo.json'), 'utf-8')
  );
  const pyVersionFile = fs.readFileSync(
    path.join(rootDir, 'linux/snipclip/version.py'),
    'utf-8'
  );

  assert.equal(releaseJson.version, rootVersion);
  assert.equal(releaseJson.debFilename, `snipclip_${rootVersion}_all.deb`);
  assert.match(pyVersionFile, new RegExp(`__version__ = "${rootVersion.replace(/\./g, '\\.')}"`));

  const debFilePath = path.join(rootDir, 'public/releases', releaseJson.debFilename);
  assert.equal(fs.existsSync(debFilePath), true);
  const debBytes = fs.readFileSync(debFilePath);
  const sha256 = crypto.createHash('sha256').update(debBytes).digest('hex');
  assert.equal(sha256, releaseJson.sha256);
  assert.equal(debBytes.byteLength, releaseJson.sizeBytes);
});
