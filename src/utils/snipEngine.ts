export enum SnipMode {
  Rectangle = 'rectangle',
  Window = 'window',
  Fullscreen = 'fullscreen',
  Freeform = 'freeform',
}

export enum AnnotationTool {
  Pen = 'pen',
  Highlighter = 'highlighter',
  Eraser = 'eraser',
  Rect = 'rect',
  Ellipse = 'ellipse',
  Line = 'line',
  Arrow = 'arrow',
  Redact = 'redact',
  Pixelate = 'pixelate',
  Crop = 'crop',
}

export enum DisplayProtocol {
  Wayland = 'wayland',
  X11 = 'x11',
  Headless = 'headless',
}

export interface Point2D {
  x: number;
  y: number;
}

export interface RectBounds {
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface RulerState {
  visible: boolean;
  x: number;
  y: number;
  angleDeg: number;
  length: number;
  halfThickness: number;
}

export interface AnnotationOperation {
  id: string;
  tool: AnnotationTool;
  color: string;
  strokeWidth: number;
  points: Point2D[];
}

export interface LinuxEnvInput {
  XDG_SESSION_TYPE: string;
  WAYLAND_DISPLAY: string;
  DISPLAY: string;
  XDG_CURRENT_DESKTOP: string;
}

export interface LinuxBinarySet {
  gdbus: boolean;
  grim: boolean;
  gnomeScreenshot: boolean;
  spectacle: boolean;
  wlCopy: boolean;
  xclip: boolean;
  xsel: boolean;
  xprop: boolean;
  xwininfo: boolean;
}

export interface DetectedSessionResult {
  protocol: DisplayProtocol;
  detectionSource: string;
  qtPlatformPlugin: string;
  desktopEnvironment: string;
  waylandDisplay: string;
  x11Display: string;
  captureDriver: string;
  clipboardDriver: string;
  windowSnipStrategy: string;
  supportsX11WindowEnumeration: boolean;
}

export interface ReleaseVersionSync {
  valid: boolean;
  error?: string;
  version: string;
  releaseTag: string;
  debFilename: string;
  cliBanner: string;
}

export function detectLinuxDisplaySession(
  env: LinuxEnvInput,
  bins: LinuxBinarySet
): DetectedSessionResult {
  const sessionType = env.XDG_SESSION_TYPE.trim().toLowerCase();
  const waylandDisplay = env.WAYLAND_DISPLAY.trim();
  const x11Display = env.DISPLAY.trim();
  const desktopEnvironment = env.XDG_CURRENT_DESKTOP.trim() || 'Generic Linux Desktop';

  let protocol = DisplayProtocol.Headless;
  let detectionSource = 'no display environment variables or sockets found';

  if (sessionType === 'wayland') {
    protocol = DisplayProtocol.Wayland;
    detectionSource = 'XDG_SESSION_TYPE=wayland';
  } else if (sessionType === 'x11') {
    protocol = DisplayProtocol.X11;
    detectionSource = 'XDG_SESSION_TYPE=x11';
  } else if (waylandDisplay.length > 0) {
    protocol = DisplayProtocol.Wayland;
    detectionSource = `WAYLAND_DISPLAY=${waylandDisplay}`;
  } else if (x11Display.length > 0) {
    protocol = DisplayProtocol.X11;
    detectionSource = `DISPLAY=${x11Display}`;
  }

  if (protocol === DisplayProtocol.Wayland) {
    let captureDriver = 'qt-screen-grab (XWayland fallback)';
    if (bins.gdbus) {
      captureDriver = 'xdg-desktop-portal (org.freedesktop.portal.Screenshot)';
    } else if (bins.grim) {
      captureDriver = 'grim (wlroots Wayland protocol)';
    } else if (bins.gnomeScreenshot) {
      captureDriver = 'gnome-screenshot';
    } else if (bins.spectacle) {
      captureDriver = 'spectacle (KDE)';
    }

    const clipboardDriver = bins.wlCopy ? 'wl-copy + QClipboard' : 'QClipboard';
    const windowSnipStrategy = bins.gdbus
      ? 'XDG Desktop Portal interactive window picker'
      : 'Compositor screen region fallback';

    return {
      protocol,
      detectionSource,
      qtPlatformPlugin: 'wayland;xcb',
      desktopEnvironment,
      waylandDisplay,
      x11Display,
      captureDriver,
      clipboardDriver,
      windowSnipStrategy,
      supportsX11WindowEnumeration: false,
    };
  }

  if (protocol === DisplayProtocol.X11) {
    const captureDriver = 'Qt X11 Screen Grab (QScreen.grabWindow)';
    let clipboardDriver = 'QClipboard';
    if (bins.xclip) {
      clipboardDriver = 'xclip + QClipboard';
    } else if (bins.xsel) {
      clipboardDriver = 'xsel + QClipboard';
    }

    const supportsX11WindowEnumeration = bins.xprop && bins.xwininfo;
    const windowSnipStrategy = supportsX11WindowEnumeration
      ? '_NET_CLIENT_LIST_STACKING + xwininfo live hover bounds'
      : 'Primary screen bounds fallback (install x11-utils for window hover)';

    return {
      protocol,
      detectionSource,
      qtPlatformPlugin: 'xcb',
      desktopEnvironment,
      waylandDisplay,
      x11Display,
      captureDriver,
      clipboardDriver,
      windowSnipStrategy,
      supportsX11WindowEnumeration,
    };
  }

  return {
    protocol: DisplayProtocol.Headless,
    detectionSource,
    qtPlatformPlugin: 'offscreen',
    desktopEnvironment,
    waylandDisplay,
    x11Display,
    captureDriver: 'offscreen-buffer',
    clipboardDriver: 'QClipboard',
    windowSnipStrategy: 'Unavailable (no active X11 or Wayland display socket)',
    supportsX11WindowEnumeration: false,
  };
}

export function normalizeRect(
  start: Point2D,
  end: Point2D,
  maxWidth: number,
  maxHeight: number
): RectBounds {
  const x1 = Math.max(0, Math.min(maxWidth, Math.min(start.x, end.x)));
  const y1 = Math.max(0, Math.min(maxHeight, Math.min(start.y, end.y)));
  const x2 = Math.max(0, Math.min(maxWidth, Math.max(start.x, end.x)));
  const y2 = Math.max(0, Math.min(maxHeight, Math.max(start.y, end.y)));
  return {
    x: Math.round(x1),
    y: Math.round(y1),
    width: Math.round(x2 - x1),
    height: Math.round(y2 - y1),
  };
}

export function computePolygonBounds(
  points: Point2D[],
  maxWidth: number,
  maxHeight: number
): RectBounds | null {
  if (points.length < 3) {
    return null;
  }
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;

  for (const pt of points) {
    if (pt.x < minX) minX = pt.x;
    if (pt.y < minY) minY = pt.y;
    if (pt.x > maxX) maxX = pt.x;
    if (pt.y > maxY) maxY = pt.y;
  }

  const clampedX1 = Math.max(0, Math.min(maxWidth, Math.floor(minX)));
  const clampedY1 = Math.max(0, Math.min(maxHeight, Math.floor(minY)));
  const clampedX2 = Math.max(0, Math.min(maxWidth, Math.ceil(maxX)));
  const clampedY2 = Math.max(0, Math.min(maxHeight, Math.ceil(maxY)));

  const width = clampedX2 - clampedX1;
  const height = clampedY2 - clampedY1;
  if (width < 8 || height < 8) {
    return null;
  }

  return { x: clampedX1, y: clampedY1, width, height };
}

export function fitRulerToCanvas(
  canvasWidth: number,
  canvasHeight: number,
  prev?: Partial<RulerState>
): RulerState {
  const safeW = Math.max(80, canvasWidth);
  const safeH = Math.max(80, canvasHeight);
  const length = Math.round(Math.min(380, Math.max(140, safeW - 56)));
  const halfThickness = Math.round(Math.min(26, Math.max(18, safeH * 0.1)));
  return {
    visible: prev?.visible ?? false,
    x: Math.round(safeW / 2),
    y: Math.round(safeH / 2),
    angleDeg: prev?.angleDeg ?? 0,
    length,
    halfThickness,
  };
}

export function snapPointToRuler(
  point: Point2D,
  ruler: RulerState,
  snapThreshold = 18
): Point2D {
  if (!ruler.visible) {
    return point;
  }
  const rad = (ruler.angleDeg * Math.PI) / 180;
  const dx = Math.cos(rad);
  const dy = Math.sin(rad);
  const nx = -dy;
  const ny = dx;

  const relX = point.x - ruler.x;
  const relY = point.y - ruler.y;
  const along = relX * dx + relY * dy;
  const perp = relX * nx + relY * ny;

  const halfLen = ruler.length / 2;
  if (Math.abs(along) > halfLen + 16) {
    return point;
  }

  if (Math.abs(perp - ruler.halfThickness) <= snapThreshold) {
    return {
      x: ruler.x + along * dx + ruler.halfThickness * nx,
      y: ruler.y + along * dy + ruler.halfThickness * ny,
    };
  }

  if (Math.abs(perp + ruler.halfThickness) <= snapThreshold) {
    return {
      x: ruler.x + along * dx - ruler.halfThickness * nx,
      y: ruler.y + along * dy - ruler.halfThickness * ny,
    };
  }

  return point;
}

function distanceToSegment(p: Point2D, a: Point2D, b: Point2D): number {
  const l2 = (b.x - a.x) ** 2 + (b.y - a.y) ** 2;
  if (l2 === 0) {
    return Math.hypot(p.x - a.x, p.y - a.y);
  }
  const t = Math.max(0, Math.min(1, ((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2));
  const projX = a.x + t * (b.x - a.x);
  const projY = a.y + t * (b.y - a.y);
  return Math.hypot(p.x - projX, p.y - projY);
}

export function hitTestAnnotation(
  point: Point2D,
  op: AnnotationOperation,
  threshold = 14
): boolean {
  if (op.points.length === 0) {
    return false;
  }
  if (op.points.length === 1) {
    return Math.hypot(point.x - op.points[0].x, point.y - op.points[0].y) <= threshold;
  }

  if (op.tool === AnnotationTool.Pen || op.tool === AnnotationTool.Highlighter) {
    for (let i = 0; i < op.points.length - 1; i++) {
      if (distanceToSegment(point, op.points[i], op.points[i + 1]) <= threshold) {
        return true;
      }
    }
    return false;
  }

  const start = op.points[0];
  const end = op.points[op.points.length - 1];

  if (op.tool === AnnotationTool.Line || op.tool === AnnotationTool.Arrow) {
    return distanceToSegment(point, start, end) <= threshold;
  }

  const minX = Math.min(start.x, end.x) - threshold;
  const maxX = Math.max(start.x, end.x) + threshold;
  const minY = Math.min(start.y, end.y) - threshold;
  const maxY = Math.max(start.y, end.y) + threshold;

  return point.x >= minX && point.x <= maxX && point.y >= minY && point.y <= maxY;
}

export function resolveVersionReleaseMetadata(rawTagOrVersion: string): ReleaseVersionSync {
  const trimmed = rawTagOrVersion.trim();
  if (!trimmed) {
    return {
      valid: false,
      error: 'Enter a semantic version or GitHub tag such as v1.0.0.',
      version: '',
      releaseTag: '',
      debFilename: '',
      cliBanner: '',
    };
  }

  const cleaned = trimmed.startsWith('v') ? trimmed.slice(1) : trimmed;
  const semverPattern = /^[0-9]+\.[0-9]+\.[0-9]+([.-][0-9A-Za-z]+)*$/;
  if (!semverPattern.test(cleaned)) {
    return {
      valid: false,
      error: 'Version must follow Debian semantic format (for example 1.0.0 or v1.2.1-rc1).',
      version: cleaned,
      releaseTag: `v${cleaned}`,
      debFilename: '',
      cliBanner: '',
    };
  }

  const releaseTag = `v${cleaned}`;
  const debFilename = `snipclip_${cleaned}_all.deb`;
  return {
    valid: true,
    version: cleaned,
    releaseTag,
    debFilename,
    cliBanner: `SnipClip ${cleaned} (${debFilename}, release ${releaseTag})`,
  };
}
