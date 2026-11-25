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
  gdkBackend: string;
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

export const LAUNCHER_WINDOW_WIDTH = 460;
export const LAUNCHER_WINDOW_HEIGHT = 200;
export const EDITOR_MIN_WIDTH = 560;
export const EDITOR_MIN_HEIGHT = 380;
export const EDITOR_CHROME_WIDTH = 48;
export const EDITOR_CHROME_HEIGHT = 136;

export interface SnipWindowDimensions {
  width: number;
  height: number;
  isLauncher: boolean;
}

export function computeSnipWindowDimensions(
  imageWidth: number,
  imageHeight: number,
  workareaWidth = 1920,
  workareaHeight = 1080
): SnipWindowDimensions {
  if (imageWidth <= 0 || imageHeight <= 0) {
    const width =
      workareaWidth > 120 ? Math.min(LAUNCHER_WINDOW_WIDTH, workareaWidth - 32) : LAUNCHER_WINDOW_WIDTH;
    const height =
      workareaHeight > 120
        ? Math.min(LAUNCHER_WINDOW_HEIGHT, workareaHeight - 32)
        : LAUNCHER_WINDOW_HEIGHT;
    return {
      width,
      height,
      isLauncher: true,
    };
  }

  const maxW = workareaWidth > 0 ? Math.max(320, Math.floor(workareaWidth * 0.8)) : 1100;
  const maxH = workareaHeight > 0 ? Math.max(240, Math.floor(workareaHeight * 0.8)) : 760;
  const minW = Math.min(EDITOR_MIN_WIDTH, maxW);
  const minH = Math.min(EDITOR_MIN_HEIGHT, maxH);
  const width = Math.max(minW, Math.min(maxW, Math.round(imageWidth) + EDITOR_CHROME_WIDTH));
  const height = Math.max(minH, Math.min(maxH, Math.round(imageHeight) + EDITOR_CHROME_HEIGHT));

  return {
    width,
    height,
    isLauncher: false,
  };
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
    let captureDriver = 'gdk_pixbuf_get_from_window (XWayland fallback)';
    if (bins.gdbus) {
      captureDriver = 'xdg-desktop-portal (GDBus org.freedesktop.portal.Screenshot)';
    } else if (bins.grim) {
      captureDriver = 'grim (wlroots Wayland protocol)';
    } else if (bins.gnomeScreenshot) {
      captureDriver = 'gnome-screenshot';
    } else if (bins.spectacle) {
      captureDriver = 'spectacle (KDE)';
    }

    const clipboardDriver = bins.wlCopy ? 'wl-copy + GtkClipboard' : 'GtkClipboard';
    const windowSnipStrategy = bins.gdbus
      ? 'XDG Desktop Portal interactive window picker (GDBus)'
      : 'Compositor screen region fallback';

    return {
      protocol,
      detectionSource,
      gdkBackend: 'wayland,x11',
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
    const captureDriver = 'GTK3 / GdkPixbuf Root Grab (gdk_pixbuf_get_from_window)';
    let clipboardDriver = 'GtkClipboard';
    if (bins.xclip) {
      clipboardDriver = 'xclip + GtkClipboard';
    } else if (bins.xsel) {
      clipboardDriver = 'xsel + GtkClipboard';
    }

    return {
      protocol,
      detectionSource,
      gdkBackend: 'x11',
      desktopEnvironment,
      waylandDisplay,
      x11Display,
      captureDriver,
      clipboardDriver,
      windowSnipStrategy: 'Native libX11 _NET_CLIENT_LIST_STACKING + XTranslateCoordinates',
      supportsX11WindowEnumeration: true,
    };
  }

  return {
    protocol: DisplayProtocol.Headless,
    detectionSource,
    gdkBackend: 'broadway',
    desktopEnvironment,
    waylandDisplay,
    x11Display,
    captureDriver: 'cairo-image-surface (headless)',
    clipboardDriver: 'GtkClipboard',
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
  const debFilename = `snipclip_${cleaned}_amd64.deb`;
  return {
    valid: true,
    version: cleaned,
    releaseTag,
    debFilename,
    cliBanner: `SnipClip ${cleaned} (${debFilename}, release ${releaseTag})`,
  };
}

export interface ShortcutBinding {
  ctrlOrMeta: boolean;
  alt: boolean;
  shift: boolean;
  key: string;
}

export interface KeyboardEventLike {
  key: string;
  code?: string;
  ctrlKey?: boolean;
  metaKey?: boolean;
  altKey?: boolean;
  shiftKey?: boolean;
  repeat?: boolean;
}

export interface ShortcutValidationResult {
  valid: boolean;
  binding: ShortcutBinding | null;
  pendingModifierOnly?: boolean;
  error?: string;
}

export interface CaptureShortcutCheckOptions {
  printScreenEnabled: boolean;
  customShortcut: ShortcutBinding | null;
  isEditableTarget: boolean;
  isRecordingShortcut: boolean;
  isCapturing: boolean;
  isCountingDown: boolean;
}

export interface CaptureShortcutCheckResult {
  triggered: boolean;
  matchedTrigger: string | null;
}

export const DEFAULT_CUSTOM_SHORTCUT: ShortcutBinding = {
  ctrlOrMeta: true,
  alt: false,
  shift: true,
  key: 'S',
};

export const SHORTCUT_PRESETS: ReadonlyArray<{ label: string; binding: ShortcutBinding }> = [
  {
    label: 'Ctrl+Shift+S (Windows Snipping)',
    binding: { ctrlOrMeta: true, alt: false, shift: true, key: 'S' },
  },
  {
    label: 'Alt+Shift+S (Linux / Desktop)',
    binding: { ctrlOrMeta: false, alt: true, shift: true, key: 'S' },
  },
  {
    label: 'Ctrl+Alt+P (Quick Capture)',
    binding: { ctrlOrMeta: true, alt: true, shift: false, key: 'P' },
  },
  {
    label: 'F9 (Single Function Key)',
    binding: { ctrlOrMeta: false, alt: false, shift: false, key: 'F9' },
  },
];

const MODIFIER_ONLY_KEYS = new Set([
  'Control',
  'Shift',
  'Alt',
  'Meta',
  'OS',
  'Super',
  'Hyper',
  'AltGraph',
  'CapsLock',
  'NumLock',
  'ScrollLock',
]);

export function isModifierOnlyKey(key: string): boolean {
  return MODIFIER_ONLY_KEYS.has(key);
}

export function isPrintScreenKey(key: string, code?: string): boolean {
  return (
    key === 'PrintScreen' ||
    code === 'PrintScreen' ||
    key === 'SysRq' ||
    key.toLowerCase() === 'printscreen'
  );
}

export function normalizeShortcutKey(key: string, code?: string): string {
  if (isPrintScreenKey(key, code)) {
    return 'PrintScreen';
  }

  if (code) {
    const keyMatch = /^Key([A-Z])$/.exec(code);
    if (keyMatch) {
      return keyMatch[1];
    }
    const digitMatch = /^Digit([0-9])$/.exec(code);
    if (digitMatch) {
      return digitMatch[1];
    }
    if (code === 'Space') {
      return 'Space';
    }
  }

  if (key === ' ') {
    return 'Space';
  }

  if (/^f([1-9]|1[0-2])$/i.test(key)) {
    return key.toUpperCase();
  }

  if (key.length === 1) {
    return key.toUpperCase();
  }

  return key;
}

export function formatShortcutBinding(binding: ShortcutBinding): string {
  const parts: string[] = [];
  if (binding.ctrlOrMeta) parts.push('Ctrl');
  if (binding.alt) parts.push('Alt');
  if (binding.shift) parts.push('Shift');
  parts.push(binding.key);
  return parts.join('+');
}

export function parseShortcutString(formatted: string): ShortcutBinding | null {
  const trimmed = formatted.trim();
  if (!trimmed) return null;
  const rawTokens = trimmed
    .split('+')
    .map((t) => t.trim())
    .filter(Boolean);
  if (rawTokens.length === 0) return null;

  let ctrlOrMeta = false;
  let alt = false;
  let shift = false;
  let mainKey = '';

  for (const token of rawTokens) {
    const lower = token.toLowerCase();
    if (lower === 'ctrl' || lower === 'control' || lower === 'cmd' || lower === 'meta') {
      ctrlOrMeta = true;
    } else if (lower === 'alt' || lower === 'option') {
      alt = true;
    } else if (lower === 'shift') {
      shift = true;
    } else {
      mainKey = normalizeShortcutKey(token);
    }
  }

  if (!mainKey) return null;
  return { ctrlOrMeta, alt, shift, key: mainKey };
}

export function isEditableElement(
  target: { tagName?: string; isContentEditable?: boolean } | null | undefined
): boolean {
  if (!target) return false;
  if (target.isContentEditable) return true;
  const tag = (target.tagName || '').toUpperCase();
  return tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT';
}

export function validateRecordedShortcut(event: KeyboardEventLike): ShortcutValidationResult {
  if (isModifierOnlyKey(event.key)) {
    return {
      valid: false,
      binding: null,
      pendingModifierOnly: true,
    };
  }

  const normalizedKey = normalizeShortcutKey(event.key, event.code);
  if (!normalizedKey || normalizedKey === 'Unidentified' || normalizedKey === 'Dead') {
    return {
      valid: false,
      binding: null,
      error: 'Unrecognized key — try a letter, number, function key, or PrintScreen.',
    };
  }

  if (normalizedKey === 'Tab' || normalizedKey === 'Enter') {
    return {
      valid: false,
      binding: null,
      error: `${normalizedKey} is reserved for keyboard navigation.`,
    };
  }

  const ctrlOrMeta = Boolean(event.ctrlKey || event.metaKey);
  const alt = Boolean(event.altKey);
  const shift = Boolean(event.shiftKey);

  // Prevent overriding built-in Undo / Redo editor shortcuts
  if (
    ctrlOrMeta &&
    !alt &&
    ((normalizedKey === 'Z' && !shift) ||
      (normalizedKey === 'Z' && shift) ||
      (normalizedKey === 'Y' && !shift))
  ) {
    return {
      valid: false,
      binding: null,
      error: 'Ctrl+Z and Ctrl+Y are reserved for Undo and Redo.',
    };
  }

  const isStandaloneAllowed =
    normalizedKey === 'PrintScreen' || /^F([1-9]|1[0-2])$/.test(normalizedKey);

  if (!isStandaloneAllowed && !ctrlOrMeta && !alt && !shift) {
    return {
      valid: false,
      binding: null,
      error: 'Combine regular keys with Ctrl, Alt, or Shift (or use F1–F12 / PrintScreen).',
    };
  }

  return {
    valid: true,
    binding: {
      ctrlOrMeta,
      alt,
      shift,
      key: normalizedKey,
    },
  };
}

export function matchesShortcutBinding(
  event: KeyboardEventLike,
  binding: ShortcutBinding
): boolean {
  if (event.repeat) return false;
  const normalizedKey = normalizeShortcutKey(event.key, event.code);
  if (normalizedKey !== binding.key) return false;

  const eventCtrl = Boolean(event.ctrlKey || event.metaKey);
  const eventAlt = Boolean(event.altKey);
  const eventShift = Boolean(event.shiftKey);

  return (
    eventCtrl === binding.ctrlOrMeta &&
    eventAlt === binding.alt &&
    eventShift === binding.shift
  );
}

export function evaluateCaptureShortcut(
  event: KeyboardEventLike,
  options: CaptureShortcutCheckOptions
): CaptureShortcutCheckResult {
  if (
    event.repeat ||
    options.isRecordingShortcut ||
    options.isCapturing ||
    options.isCountingDown
  ) {
    return { triggered: false, matchedTrigger: null };
  }

  const normalizedKey = normalizeShortcutKey(event.key, event.code);
  const noModifiers =
    !event.ctrlKey && !event.metaKey && !event.altKey && !event.shiftKey;

  if (options.printScreenEnabled && normalizedKey === 'PrintScreen' && noModifiers) {
    return { triggered: true, matchedTrigger: 'PrintScreen' };
  }

  // Never hijack typing inside text inputs, textareas, or selects with custom chords
  if (options.isEditableTarget) {
    return { triggered: false, matchedTrigger: null };
  }

  if (options.customShortcut && matchesShortcutBinding(event, options.customShortcut)) {
    return {
      triggered: true,
      matchedTrigger: formatShortcutBinding(options.customShortcut),
    };
  }

  return { triggered: false, matchedTrigger: null };
}

