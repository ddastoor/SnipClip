import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  ArrowUpRight,
  Check,
  Circle,
  Copy,
  Crop,
  Download,
  Eraser,
  EyeOff,
  FolderOpen,
  Grid,
  Highlighter,
  Keyboard,
  Minus,
  MoreHorizontal,
  PenTool,
  Plus,
  Redo2,
  RotateCcw,
  Ruler,
  Scissors,
  Square,
  Trash2,
  Undo2,
  X,
  ZoomIn,
  ZoomOut,
} from 'lucide-react';
import {
  AnnotationOperation,
  AnnotationTool,
  computePolygonBounds,
  computeSnipWindowDimensions,
  DEFAULT_CUSTOM_SHORTCUT,
  DetectedSessionResult,
  evaluateCaptureShortcut,
  fitRulerToCanvas,
  formatShortcutBinding,
  hitTestAnnotation,
  isEditableElement,
  isPrintScreenKey,
  LAUNCHER_WINDOW_HEIGHT,
  LAUNCHER_WINDOW_WIDTH,
  normalizeRect,
  parseShortcutString,
  Point2D,
  RectBounds,
  RulerState,
  SHORTCUT_PRESETS,
  ShortcutBinding,
  SnipMode,
  snapPointToRuler,
  validateRecordedShortcut,
} from '../utils/snipEngine';
import {
  DESKTOP_HEIGHT,
  DESKTOP_WIDTH,
  extractSubCanvas,
  renderSimulatedDebianDesktop,
  SIMULATED_WINDOWS,
  SimulatedWindow,
} from '../utils/desktopScene';

interface SnipStudioProps {
  version: string;
  debFilename: string;
  sessionInfo: DetectedSessionResult;
}

interface EditorSnapshot {
  imageCanvas: HTMLCanvasElement;
  annotations: AnnotationOperation[];
}

const INK_COLORS = [
  { name: 'Crimson', hex: '#ef4444', swatchClass: 'bg-red-500' },
  { name: 'Amber', hex: '#f59e0b', swatchClass: 'bg-amber-500' },
  { name: 'Emerald', hex: '#10b981', swatchClass: 'bg-emerald-500' },
  { name: 'Royal Blue', hex: '#2563eb', swatchClass: 'bg-blue-600' },
  { name: 'Violet', hex: '#7c3aed', swatchClass: 'bg-violet-600' },
  { name: 'Slate Ink', hex: '#0f172a', swatchClass: 'bg-slate-900' },
];

function cloneCanvas(src: HTMLCanvasElement): HTMLCanvasElement {
  const copy = document.createElement('canvas');
  copy.width = src.width;
  copy.height = src.height;
  const ctx = copy.getContext('2d');
  if (ctx) {
    ctx.drawImage(src, 0, 0);
  }
  return copy;
}

function drawAnnotationOnContext(
  ctx: CanvasRenderingContext2D,
  op: AnnotationOperation,
  backingSource: HTMLCanvasElement
): void {
  if (op.points.length === 0) return;

  ctx.save();
  if (op.tool === AnnotationTool.Highlighter) {
    ctx.globalAlpha = 0.38;
    ctx.strokeStyle = op.color;
    ctx.lineWidth = op.strokeWidth * 4.2;
    ctx.lineCap = 'butt';
    ctx.lineJoin = 'round';
    ctx.beginPath();
    ctx.moveTo(op.points[0].x, op.points[0].y);
    for (let i = 1; i < op.points.length; i++) {
      ctx.lineTo(op.points[i].x, op.points[i].y);
    }
    if (op.points.length === 1) {
      ctx.lineTo(op.points[0].x + 0.5, op.points[0].y);
    }
    ctx.stroke();
    ctx.restore();
    return;
  }

  if (op.tool === AnnotationTool.Pen) {
    ctx.strokeStyle = op.color;
    ctx.lineWidth = op.strokeWidth;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    ctx.beginPath();
    ctx.moveTo(op.points[0].x, op.points[0].y);
    for (let i = 1; i < op.points.length; i++) {
      ctx.lineTo(op.points[i].x, op.points[i].y);
    }
    if (op.points.length === 1) {
      ctx.lineTo(op.points[0].x + 0.5, op.points[0].y);
    }
    ctx.stroke();
    ctx.restore();
    return;
  }

  if (op.points.length < 2) {
    ctx.restore();
    return;
  }

  const p1 = op.points[0];
  const p2 = op.points[op.points.length - 1];
  const rx = Math.min(p1.x, p2.x);
  const ry = Math.min(p1.y, p2.y);
  const rw = Math.abs(p2.x - p1.x);
  const rh = Math.abs(p2.y - p1.y);

  ctx.strokeStyle = op.color;
  ctx.lineWidth = op.strokeWidth;
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';

  if (op.tool === AnnotationTool.Rect) {
    ctx.strokeRect(rx, ry, rw, rh);
  } else if (op.tool === AnnotationTool.Ellipse) {
    ctx.beginPath();
    ctx.ellipse(rx + rw / 2, ry + rh / 2, rw / 2, rh / 2, 0, 0, Math.PI * 2);
    ctx.stroke();
  } else if (op.tool === AnnotationTool.Line) {
    ctx.beginPath();
    ctx.moveTo(p1.x, p1.y);
    ctx.lineTo(p2.x, p2.y);
    ctx.stroke();
  } else if (op.tool === AnnotationTool.Arrow) {
    const angle = Math.atan2(p2.y - p1.y, p2.x - p1.x);
    const headLen = Math.max(11, op.strokeWidth * 3.6);
    ctx.beginPath();
    ctx.moveTo(p1.x, p1.y);
    ctx.lineTo(p2.x, p2.y);
    ctx.moveTo(p2.x, p2.y);
    ctx.lineTo(
      p2.x - headLen * Math.cos(angle - Math.PI / 6),
      p2.y - headLen * Math.sin(angle - Math.PI / 6)
    );
    ctx.moveTo(p2.x, p2.y);
    ctx.lineTo(
      p2.x - headLen * Math.cos(angle + Math.PI / 6),
      p2.y - headLen * Math.sin(angle + Math.PI / 6)
    );
    ctx.stroke();
  } else if (op.tool === AnnotationTool.Redact) {
    ctx.fillStyle = '#0f172a';
    ctx.fillRect(rx, ry, rw, rh);
  } else if (op.tool === AnnotationTool.Pixelate) {
    if (rw >= 6 && rh >= 6) {
      const blockW = Math.max(2, Math.floor(rw / 10));
      const blockH = Math.max(2, Math.floor(rh / 10));
      const off = document.createElement('canvas');
      off.width = blockW;
      off.height = blockH;
      const octx = off.getContext('2d');
      if (octx) {
        octx.imageSmoothingEnabled = false;
        octx.drawImage(backingSource, rx, ry, rw, rh, 0, 0, blockW, blockH);
        ctx.imageSmoothingEnabled = false;
        ctx.drawImage(off, 0, 0, blockW, blockH, rx, ry, rw, rh);
      }
    }
  }

  ctx.restore();
}

export const SnipStudio: React.FC<SnipStudioProps> = ({
  version,
  debFilename,
  sessionInfo,
}) => {
  const [snipMode, setSnipMode] = useState<SnipMode>(SnipMode.Rectangle);
  const [delaySeconds, setDelaySeconds] = useState<number>(0);
  const [countdownLeft, setCountdownLeft] = useState<number | null>(null);
  const [isCapturing, setIsCapturing] = useState<boolean>(false);

  const [tool, setTool] = useState<AnnotationTool>(AnnotationTool.Pen);
  const [inkColor, setInkColor] = useState<string>('#ef4444');
  const [strokeWidth, setStrokeWidth] = useState<number>(3);
  const [zoom, setZoom] = useState<number>(1);

  const [baseCanvas, setBaseCanvas] = useState<HTMLCanvasElement | null>(null);
  const [annotations, setAnnotations] = useState<AnnotationOperation[]>([]);
  const [activeOp, setActiveOp] = useState<AnnotationOperation | null>(null);
  const [undoStack, setUndoStack] = useState<EditorSnapshot[]>([]);
  const [redoStack, setRedoStack] = useState<EditorSnapshot[]>([]);

  const [ruler, setRuler] = useState<RulerState>({
    visible: false,
    x: 295,
    y: 175,
    angleDeg: 0,
    length: 400,
    halfThickness: 26,
  });
  const [draggingRulerOffset, setDraggingRulerOffset] = useState<Point2D | null>(null);

  const [cropRect, setCropRect] = useState<RectBounds | null>(null);
  const [cropDragStart, setCropDragStart] = useState<Point2D | null>(null);

  const [statusMessage, setStatusMessage] = useState<string>(
    `Startup auto-detected ${sessionInfo.protocol.toUpperCase()} (${sessionInfo.detectionSource}) — ready to snip or annotate.`
  );
  const [copyFeedback, setCopyFeedback] = useState<boolean>(false);

  // Keyboard shortcut configuration (PrintScreen + user-definable key combination)
  const [printScreenEnabled, setPrintScreenEnabled] = useState<boolean>(() => {
    try {
      return window.localStorage.getItem('snipclip.printScreenEnabled') !== 'false';
    } catch {
      return true;
    }
  });
  const [customShortcut, setCustomShortcut] = useState<ShortcutBinding>(() => {
    try {
      const saved = window.localStorage.getItem('snipclip.customShortcut');
      return (saved && parseShortcutString(saved)) || DEFAULT_CUSTOM_SHORTCUT;
    } catch {
      return DEFAULT_CUSTOM_SHORTCUT;
    }
  });
  const [isRecordingShortcut, setIsRecordingShortcut] = useState<boolean>(false);
  const [shortcutError, setShortcutError] = useState<string | null>(null);
  const [isSettingsOpen, setIsSettingsOpen] = useState<boolean>(false);
  const lastPrintScreenKeyDownAtRef = useRef<number>(0);

  // Capture overlay interaction state
  const [captureStart, setCaptureStart] = useState<Point2D | null>(null);
  const [captureCurrent, setCaptureCurrent] = useState<Point2D | null>(null);
  const [lassoPoints, setLassoPoints] = useState<Point2D[]>([]);
  const [hoveredWindow, setHoveredWindow] = useState<SimulatedWindow | null>(null);

  const editorCanvasRef = useRef<HTMLCanvasElement | null>(null);
  const captureCanvasRef = useRef<HTMLCanvasElement | null>(null);
  const fileInputRef = useRef<HTMLInputElement | null>(null);
  const settingsContainerRef = useRef<HTMLDivElement | null>(null);

  const simulatedDesktop = useMemo(() => {
    return renderSimulatedDebianDesktop(
      version,
      sessionInfo.protocol,
      sessionInfo.desktopEnvironment,
      sessionInfo.captureDriver
    );
  }, [version, sessionInfo.protocol, sessionInfo.desktopEnvironment, sessionInfo.captureDriver]);

  // Announce session auto-detection when session environment changes
  useEffect(() => {
    setStatusMessage(
      `Auto-detected ${sessionInfo.protocol.toUpperCase()} via ${sessionInfo.detectionSource} → ${sessionInfo.captureDriver}.`
    );
  }, [sessionInfo.protocol, sessionInfo.detectionSource, sessionInfo.captureDriver]);

  // Close shortcut settings popover when clicking outside
  useEffect(() => {
    if (!isSettingsOpen) return;
    const onPointerDownOutside = (e: MouseEvent) => {
      if (
        settingsContainerRef.current &&
        !settingsContainerRef.current.contains(e.target as Node)
      ) {
        setIsSettingsOpen(false);
        setIsRecordingShortcut(false);
        setShortcutError(null);
      }
    };
    window.addEventListener('mousedown', onPointerDownOutside);
    return () => window.removeEventListener('mousedown', onPointerDownOutside);
  }, [isSettingsOpen]);

  const handleLoadSampleWindow = useCallback(() => {
    const termWin = SIMULATED_WINDOWS[0];
    const sample = extractSubCanvas(
      simulatedDesktop,
      termWin.x,
      termWin.y,
      termWin.width,
      termWin.height
    );
    setBaseCanvas(sample);
    setAnnotations([]);
    setActiveOp(null);
    setUndoStack([]);
    setRedoStack([]);
    setCropRect(null);
    setZoom(1);
    setIsSettingsOpen(false);
    setStatusMessage(
      `Loaded sample window snip "${termWin.title}" (${termWin.width} × ${termWin.height} px).`
    );
  }, [simulatedDesktop]);

  const handleResetToLauncher = useCallback(() => {
    setBaseCanvas(null);
    setAnnotations([]);
    setActiveOp(null);
    setUndoStack([]);
    setRedoStack([]);
    setCropRect(null);
    setZoom(1);
    setIsCapturing(false);
    setIsSettingsOpen(false);
    setStatusMessage('Closed snip — ready in compact launcher mode.');
  }, []);

  // Keep ruler centered and proportioned inside newly captured or cropped canvases
  useEffect(() => {
    if (!baseCanvas) return;
    setRuler((prev) => fitRulerToCanvas(baseCanvas.width, baseCanvas.height, prev));
  }, [baseCanvas]);

  // Countdown timer effect for delayed snips
  useEffect(() => {
    if (countdownLeft === null) return;
    if (countdownLeft <= 0) {
      setCountdownLeft(null);
      if (snipMode === SnipMode.Fullscreen) {
        const full = cloneCanvas(simulatedDesktop);
        setBaseCanvas(full);
        setAnnotations([]);
        setUndoStack([]);
        setRedoStack([]);
        setCropRect(null);
        setZoom(0.75);
        setStatusMessage(`Captured Fullscreen snip (${full.width} × ${full.height} px).`);
      } else {
        setIsCapturing(true);
      }
      return;
    }
    const id = window.setTimeout(() => {
      setCountdownLeft((prev) => (prev !== null ? prev - 1 : null));
    }, 1000);
    return () => window.clearTimeout(id);
  }, [countdownLeft, snipMode, simulatedDesktop]);

  const pushUndoSnapshot = useCallback(() => {
    if (!baseCanvas) return;
    setUndoStack((prev) => [
      ...prev.slice(-29),
      {
        imageCanvas: cloneCanvas(baseCanvas),
        annotations: annotations.map((a) => ({
          ...a,
          points: a.points.map((p) => ({ ...p })),
        })),
      },
    ]);
    setRedoStack([]);
  }, [baseCanvas, annotations]);

  const handleUndo = useCallback(() => {
    if (undoStack.length === 0 || !baseCanvas) return;
    const prev = undoStack[undoStack.length - 1];
    setRedoStack((r) => [
      ...r,
      {
        imageCanvas: cloneCanvas(baseCanvas),
        annotations: annotations.map((a) => ({ ...a, points: [...a.points] })),
      },
    ]);
    setUndoStack((u) => u.slice(0, -1));
    setBaseCanvas(prev.imageCanvas);
    setAnnotations(prev.annotations);
    setStatusMessage('Undid last edit.');
  }, [undoStack, baseCanvas, annotations]);

  const handleRedo = useCallback(() => {
    if (redoStack.length === 0 || !baseCanvas) return;
    const next = redoStack[redoStack.length - 1];
    setUndoStack((u) => [
      ...u,
      {
        imageCanvas: cloneCanvas(baseCanvas),
        annotations: annotations.map((a) => ({ ...a, points: [...a.points] })),
      },
    ]);
    setRedoStack((r) => r.slice(0, -1));
    setBaseCanvas(next.imageCanvas);
    setAnnotations(next.annotations);
    setStatusMessage('Redid edit.');
  }, [redoStack, baseCanvas, annotations]);

  const formattedCustomShortcut = useMemo(
    () => formatShortcutBinding(customShortcut),
    [customShortcut]
  );

  useEffect(() => {
    try {
      window.localStorage.setItem('snipclip.printScreenEnabled', String(printScreenEnabled));
    } catch {
      // Ignore storage errors in restricted environments
    }
  }, [printScreenEnabled]);

  useEffect(() => {
    try {
      window.localStorage.setItem('snipclip.customShortcut', formattedCustomShortcut);
    } catch {
      // Ignore storage errors in restricted environments
    }
  }, [formattedCustomShortcut]);

  const triggerSnipCapture = useCallback(
    (triggerSource?: string) => {
      setCaptureStart(null);
      setCaptureCurrent(null);
      setLassoPoints([]);
      setHoveredWindow(SIMULATED_WINDOWS[0]);

      const viaSuffix = triggerSource ? ` via ${triggerSource}` : '';

      if (delaySeconds > 0) {
        setCountdownLeft(delaySeconds);
        setStatusMessage(`Timer started${viaSuffix} — capturing in ${delaySeconds} seconds…`);
        return;
      }

      if (snipMode === SnipMode.Fullscreen) {
        const full = cloneCanvas(simulatedDesktop);
        setBaseCanvas(full);
        setAnnotations([]);
        setUndoStack([]);
        setRedoStack([]);
        setCropRect(null);
        setZoom(0.75);
        setStatusMessage(
          `Captured Fullscreen snip${viaSuffix} (${full.width} × ${full.height} px).`
        );
        return;
      }

      setIsSettingsOpen(false);
      setIsCapturing(true);
      setStatusMessage(
        `Snipping active${viaSuffix} (${snipMode} mode) — select an area on the Debian desktop.`
      );
    },
    [delaySeconds, snipMode, simulatedDesktop]
  );

  const buildFlattenedCanvas = useCallback((): HTMLCanvasElement | null => {
    if (!baseCanvas) return null;
    const flat = cloneCanvas(baseCanvas);
    const ctx = flat.getContext('2d');
    if (!ctx) return flat;
    for (const op of annotations) {
      drawAnnotationOnContext(ctx, op, flat);
    }
    return flat;
  }, [baseCanvas, annotations]);

  const handleCopySnip = useCallback(async () => {
    const flat = buildFlattenedCanvas();
    if (!flat) return;

    let copiedViaClipboardApi = false;
    try {
      const blob = await new Promise<Blob | null>((resolve) => flat.toBlob(resolve, 'image/png'));
      if (blob && navigator.clipboard && typeof ClipboardItem !== 'undefined') {
        await navigator.clipboard.write([new ClipboardItem({ 'image/png': blob })]);
        copiedViaClipboardApi = true;
      }
    } catch {
      copiedViaClipboardApi = false;
    }

    setCopyFeedback(true);
    window.setTimeout(() => setCopyFeedback(false), 2000);
    setStatusMessage(
      copiedViaClipboardApi
        ? `Copied PNG (${flat.width} × ${flat.height} px) via ${sessionInfo.clipboardDriver}.`
        : `Prepared PNG buffer (${flat.width} × ${flat.height} px) for ${sessionInfo.clipboardDriver}.`
    );
  }, [buildFlattenedCanvas, sessionInfo.clipboardDriver]);

  const handleSavePng = useCallback(() => {
    const flat = buildFlattenedCanvas();
    if (!flat) return;
    const url = flat.toDataURL('image/png');
    const a = document.createElement('a');
    a.href = url;
    a.download = `snipclip-${version}-${flat.width}x${flat.height}.png`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setStatusMessage(`Saved snipclip-${version}-${flat.width}x${flat.height}.png.`);
  }, [buildFlattenedCanvas, version]);

  // Keyboard shortcuts (PrintScreen, custom capture combination, recorder, Ctrl+Z, Ctrl+Y, Ctrl+C, Ctrl+S, Escape)
  useEffect(() => {
    const onKeyDown = (e: KeyboardEvent) => {
      if (isRecordingShortcut) {
        if (e.key === 'Escape') {
          e.preventDefault();
          e.stopPropagation();
          setIsRecordingShortcut(false);
          setShortcutError(null);
          setStatusMessage('Shortcut recording cancelled.');
          return;
        }

        const validation = validateRecordedShortcut(e);
        e.preventDefault();
        e.stopPropagation();

        if (validation.pendingModifierOnly) {
          return;
        }

        if (!validation.valid || !validation.binding) {
          setShortcutError(validation.error ?? 'Invalid shortcut combination.');
          return;
        }

        const nextFormatted = formatShortcutBinding(validation.binding);
        setCustomShortcut(validation.binding);
        setIsRecordingShortcut(false);
        setShortcutError(null);
        setStatusMessage(`Saved custom capture shortcut: ${nextFormatted}.`);
        return;
      }

      if (e.key === 'Escape') {
        if (isCapturing) {
          setIsCapturing(false);
          setCaptureStart(null);
          setCaptureCurrent(null);
          setLassoPoints([]);
          setStatusMessage('Snip cancelled.');
        } else if (countdownLeft !== null) {
          setCountdownLeft(null);
          setStatusMessage('Delayed snip cancelled.');
        } else if (isSettingsOpen) {
          setIsSettingsOpen(false);
          setShortcutError(null);
        } else if (tool === AnnotationTool.Crop) {
          setTool(AnnotationTool.Pen);
          setCropRect(null);
        }
        return;
      }

      const editable = isEditableElement(e.target as HTMLElement | null);
      const shortcutEval = evaluateCaptureShortcut(e, {
        printScreenEnabled,
        customShortcut,
        isEditableTarget: editable,
        isRecordingShortcut,
        isCapturing,
        isCountingDown: countdownLeft !== null,
      });

      if (shortcutEval.triggered && shortcutEval.matchedTrigger) {
        e.preventDefault();
        e.stopPropagation();
        if (shortcutEval.matchedTrigger === 'PrintScreen') {
          lastPrintScreenKeyDownAtRef.current = Date.now();
        }
        triggerSnipCapture(shortcutEval.matchedTrigger);
        return;
      }

      if (editable) {
        return;
      }

      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z' && !e.shiftKey) {
        e.preventDefault();
        handleUndo();
      } else if (
        ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'y') ||
        ((e.ctrlKey || e.metaKey) && e.shiftKey && e.key.toLowerCase() === 'z')
      ) {
        e.preventDefault();
        handleRedo();
      } else if (
        baseCanvas &&
        !isCapturing &&
        (e.ctrlKey || e.metaKey) &&
        !e.shiftKey &&
        !e.altKey &&
        e.key.toLowerCase() === 'c' &&
        !window.getSelection()?.toString()
      ) {
        e.preventDefault();
        void handleCopySnip();
      } else if (
        baseCanvas &&
        !isCapturing &&
        (e.ctrlKey || e.metaKey) &&
        !e.shiftKey &&
        !e.altKey &&
        e.key.toLowerCase() === 's'
      ) {
        e.preventDefault();
        handleSavePng();
      }
    };

    // Fallback for Linux/OS window managers that intercept PrintScreen keydown and only emit keyup
    const onKeyUp = (e: KeyboardEvent) => {
      if (
        !isRecordingShortcut &&
        !isCapturing &&
        countdownLeft === null &&
        printScreenEnabled &&
        isPrintScreenKey(e.key, e.code) &&
        !e.ctrlKey &&
        !e.metaKey &&
        !e.altKey &&
        !e.shiftKey
      ) {
        if (Date.now() - lastPrintScreenKeyDownAtRef.current > 500) {
          e.preventDefault();
          triggerSnipCapture('PrintScreen');
        }
      }
    };

    window.addEventListener('keydown', onKeyDown, true);
    window.addEventListener('keyup', onKeyUp, true);
    return () => {
      window.removeEventListener('keydown', onKeyDown, true);
      window.removeEventListener('keyup', onKeyUp, true);
    };
  }, [
    isRecordingShortcut,
    isCapturing,
    countdownLeft,
    isSettingsOpen,
    tool,
    printScreenEnabled,
    customShortcut,
    baseCanvas,
    triggerSnipCapture,
    handleUndo,
    handleRedo,
    handleCopySnip,
    handleSavePng,
  ]);

  // Render the main annotation editor canvas whenever state changes
  useEffect(() => {
    const canvas = editorCanvasRef.current;
    if (!canvas || !baseCanvas) return;

    const imgW = baseCanvas.width;
    const imgH = baseCanvas.height;
    canvas.width = Math.max(1, Math.round(imgW * zoom));
    canvas.height = Math.max(1, Math.round(imgH * zoom));
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    ctx.save();
    ctx.scale(zoom, zoom);
    ctx.clearRect(0, 0, imgW, imgH);

    // Subtle transparency checkerboard for freeform snips
    const tile = 12;
    for (let y = 0; y < imgH; y += tile) {
      for (let x = 0; x < imgW; x += tile) {
        ctx.fillStyle = ((x / tile + y / tile) & 1) === 0 ? '#ffffff' : '#f1f5f9';
        ctx.fillRect(x, y, tile, tile);
      }
    }

    ctx.drawImage(baseCanvas, 0, 0);

    for (const op of annotations) {
      drawAnnotationOnContext(ctx, op, baseCanvas);
    }
    if (activeOp) {
      drawAnnotationOnContext(ctx, activeOp, baseCanvas);
    }

    // Ruler overlay
    if (ruler.visible) {
      ctx.save();
      ctx.translate(ruler.x, ruler.y);
      ctx.rotate((ruler.angleDeg * Math.PI) / 180);
      const halfL = ruler.length / 2;
      const halfT = ruler.halfThickness;

      ctx.fillStyle = 'rgba(248, 250, 252, 0.9)';
      ctx.strokeStyle = '#475569';
      ctx.lineWidth = 1.5;
      ctx.fillRect(-halfL, -halfT, ruler.length, halfT * 2);
      ctx.strokeRect(-halfL, -halfT, ruler.length, halfT * 2);

      ctx.strokeStyle = '#64748b';
      ctx.lineWidth = 1;
      for (let tx = -halfL + 10; tx <= halfL - 10; tx += 10) {
        const tickLen = tx % 50 === 0 ? 11 : 5;
        ctx.beginPath();
        ctx.moveTo(tx, -halfT);
        ctx.lineTo(tx, -halfT + tickLen);
        ctx.moveTo(tx, halfT);
        ctx.lineTo(tx, halfT - tickLen);
        ctx.stroke();
      }

      ctx.fillStyle = '#0f172a';
      ctx.font = '600 12px "JetBrains Mono", monospace';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      const normalizedDeg = ((Math.round(ruler.angleDeg) % 180) + 180) % 180;
      ctx.fillText(`Ruler · ${normalizedDeg}° (drag to move)`, 0, 0);
      ctx.restore();
    }

    // Crop overlay
    if (tool === AnnotationTool.Crop && cropRect) {
      ctx.save();
      ctx.fillStyle = 'rgba(15, 23, 42, 0.52)';
      ctx.fillRect(0, 0, imgW, cropRect.y);
      ctx.fillRect(0, cropRect.y + cropRect.height, imgW, imgH - (cropRect.y + cropRect.height));
      ctx.fillRect(0, cropRect.y, cropRect.x, cropRect.height);
      ctx.fillRect(
        cropRect.x + cropRect.width,
        cropRect.y,
        imgW - (cropRect.x + cropRect.width),
        cropRect.height
      );

      ctx.strokeStyle = '#2563eb';
      ctx.lineWidth = 2;
      ctx.setLineDash([6, 4]);
      ctx.strokeRect(cropRect.x, cropRect.y, cropRect.width, cropRect.height);
      ctx.restore();
    }

    ctx.restore();
  }, [baseCanvas, annotations, activeOp, ruler, tool, cropRect, zoom]);

  // Render the capture overlay canvas when snipping is active
  useEffect(() => {
    if (!isCapturing) return;
    const canvas = captureCanvasRef.current;
    if (!canvas) return;

    canvas.width = DESKTOP_WIDTH;
    canvas.height = DESKTOP_HEIGHT;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    ctx.drawImage(simulatedDesktop, 0, 0);

    // Dimmed overlay scrim
    ctx.fillStyle = 'rgba(15, 23, 42, 0.52)';
    ctx.fillRect(0, 0, DESKTOP_WIDTH, DESKTOP_HEIGHT);

    if (snipMode === SnipMode.Rectangle && captureStart && captureCurrent) {
      const rect = normalizeRect(captureStart, captureCurrent, DESKTOP_WIDTH, DESKTOP_HEIGHT);
      if (rect.width > 0 && rect.height > 0) {
        ctx.drawImage(
          simulatedDesktop,
          rect.x,
          rect.y,
          rect.width,
          rect.height,
          rect.x,
          rect.y,
          rect.width,
          rect.height
        );
        ctx.strokeStyle = '#38bdf8';
        ctx.lineWidth = 2;
        ctx.strokeRect(rect.x, rect.y, rect.width, rect.height);

        const label = `${rect.width} × ${rect.height} px`;
        ctx.font = '600 12px "JetBrains Mono", monospace';
        const tw = ctx.measureText(label).width + 16;
        const ly = Math.max(8, rect.y - 28);
        ctx.fillStyle = '#0f172a';
        ctx.fillRect(rect.x, ly, tw, 22);
        ctx.fillStyle = '#f8fafc';
        ctx.fillText(label, rect.x + 8, ly + 15);
      }
    } else if (snipMode === SnipMode.Window && hoveredWindow) {
      ctx.drawImage(
        simulatedDesktop,
        hoveredWindow.x,
        hoveredWindow.y,
        hoveredWindow.width,
        hoveredWindow.height,
        hoveredWindow.x,
        hoveredWindow.y,
        hoveredWindow.width,
        hoveredWindow.height
      );
      ctx.strokeStyle = '#38bdf8';
      ctx.lineWidth = 2.5;
      ctx.strokeRect(
        hoveredWindow.x,
        hoveredWindow.y,
        hoveredWindow.width,
        hoveredWindow.height
      );
    } else if (snipMode === SnipMode.Freeform && lassoPoints.length >= 2) {
      ctx.save();
      ctx.beginPath();
      ctx.moveTo(lassoPoints[0].x, lassoPoints[0].y);
      for (let i = 1; i < lassoPoints.length; i++) {
        ctx.lineTo(lassoPoints[i].x, lassoPoints[i].y);
      }
      ctx.closePath();
      ctx.clip();
      ctx.drawImage(simulatedDesktop, 0, 0);
      ctx.restore();

      ctx.strokeStyle = '#38bdf8';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(lassoPoints[0].x, lassoPoints[0].y);
      for (let i = 1; i < lassoPoints.length; i++) {
        ctx.lineTo(lassoPoints[i].x, lassoPoints[i].y);
      }
      ctx.stroke();
    }
  }, [
    isCapturing,
    simulatedDesktop,
    snipMode,
    captureStart,
    captureCurrent,
    hoveredWindow,
    lassoPoints,
  ]);

  const startNewSnip = () => {
    triggerSnipCapture();
  };

  const completeWindowCapture = (win: SimulatedWindow) => {
    const captured = extractSubCanvas(simulatedDesktop, win.x, win.y, win.width, win.height);
    setBaseCanvas(captured);
    setAnnotations([]);
    setUndoStack([]);
    setRedoStack([]);
    setCropRect(null);
    setZoom(1);
    setIsCapturing(false);
    setStatusMessage(`Captured window "${win.title}" (${win.width} × ${win.height} px).`);
  };

  const getPointerCoordsInCanvas = (
    e: React.PointerEvent<HTMLCanvasElement>,
    canvas: HTMLCanvasElement
  ): Point2D => {
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    return {
      x: (e.clientX - rect.left) * scaleX,
      y: (e.clientY - rect.top) * scaleY,
    };
  };

  // Capture overlay pointer handlers
  const onCapturePointerDown = (e: React.PointerEvent<HTMLCanvasElement>) => {
    const canvas = captureCanvasRef.current;
    if (!canvas) return;
    const pt = getPointerCoordsInCanvas(e, canvas);

    if (snipMode === SnipMode.Window) {
      const hit = SIMULATED_WINDOWS.find(
        (w) => pt.x >= w.x && pt.x <= w.x + w.width && pt.y >= w.y && pt.y <= w.y + w.height
      );
      if (hit) {
        completeWindowCapture(hit);
      }
      return;
    }

    e.currentTarget.setPointerCapture(e.pointerId);
    setCaptureStart(pt);
    setCaptureCurrent(pt);
    if (snipMode === SnipMode.Freeform) {
      setLassoPoints([pt]);
    }
  };

  const onCapturePointerMove = (e: React.PointerEvent<HTMLCanvasElement>) => {
    const canvas = captureCanvasRef.current;
    if (!canvas) return;
    const pt = getPointerCoordsInCanvas(e, canvas);

    if (snipMode === SnipMode.Window) {
      const hit =
        SIMULATED_WINDOWS.find(
          (w) => pt.x >= w.x && pt.x <= w.x + w.width && pt.y >= w.y && pt.y <= w.y + w.height
        ) || null;
      setHoveredWindow(hit);
      return;
    }

    if (!captureStart) return;
    setCaptureCurrent(pt);
    if (snipMode === SnipMode.Freeform) {
      setLassoPoints((prev) => [...prev, pt]);
    }
  };

  const onCapturePointerUp = (e: React.PointerEvent<HTMLCanvasElement>) => {
    const canvas = captureCanvasRef.current;
    if (!canvas || !captureStart) return;
    const pt = getPointerCoordsInCanvas(e, canvas);

    if (snipMode === SnipMode.Rectangle) {
      const rect = normalizeRect(captureStart, pt, DESKTOP_WIDTH, DESKTOP_HEIGHT);
      setCaptureStart(null);
      setCaptureCurrent(null);
      if (rect.width >= 12 && rect.height >= 12) {
        const sub = extractSubCanvas(simulatedDesktop, rect.x, rect.y, rect.width, rect.height);
        setBaseCanvas(sub);
        setAnnotations([]);
        setUndoStack([]);
        setRedoStack([]);
        setCropRect(null);
        setZoom(1);
        setIsCapturing(false);
        setStatusMessage(`Captured rectangular snip (${rect.width} × ${rect.height} px).`);
      }
      return;
    }

    if (snipMode === SnipMode.Freeform) {
      const finalPts = [...lassoPoints, pt];
      const bounds = computePolygonBounds(finalPts, DESKTOP_WIDTH, DESKTOP_HEIGHT);
      setCaptureStart(null);
      setCaptureCurrent(null);
      setLassoPoints([]);
      if (bounds) {
        const out = document.createElement('canvas');
        out.width = bounds.width;
        out.height = bounds.height;
        const ctx = out.getContext('2d');
        if (ctx) {
          ctx.beginPath();
          ctx.moveTo(finalPts[0].x - bounds.x, finalPts[0].y - bounds.y);
          for (let i = 1; i < finalPts.length; i++) {
            ctx.lineTo(finalPts[i].x - bounds.x, finalPts[i].y - bounds.y);
          }
          ctx.closePath();
          ctx.clip();
          ctx.drawImage(
            simulatedDesktop,
            bounds.x,
            bounds.y,
            bounds.width,
            bounds.height,
            0,
            0,
            bounds.width,
            bounds.height
          );
        }
        setBaseCanvas(out);
        setAnnotations([]);
        setUndoStack([]);
        setRedoStack([]);
        setCropRect(null);
        setZoom(1);
        setIsCapturing(false);
        setStatusMessage(`Captured freeform lasso snip (${bounds.width} × ${bounds.height} px).`);
      }
    }
  };

  // Editor canvas pointer handlers
  const isPointInsideRuler = (pt: Point2D): boolean => {
    if (!ruler.visible) return false;
    const rad = (ruler.angleDeg * Math.PI) / 180;
    const dx = Math.cos(rad);
    const dy = Math.sin(rad);
    const nx = -dy;
    const ny = dx;
    const relX = pt.x - ruler.x;
    const relY = pt.y - ruler.y;
    const along = relX * dx + relY * dy;
    const perp = relX * nx + relY * ny;
    return Math.abs(along) <= ruler.length / 2 && Math.abs(perp) <= ruler.halfThickness - 2;
  };

  const getEditorCoords = (
    e: React.PointerEvent<HTMLCanvasElement>,
    canvas: HTMLCanvasElement
  ): Point2D => {
    const rect = canvas.getBoundingClientRect();
    if (!baseCanvas || rect.width === 0 || rect.height === 0) {
      return { x: 0, y: 0 };
    }
    return {
      x: ((e.clientX - rect.left) / rect.width) * baseCanvas.width,
      y: ((e.clientY - rect.top) / rect.height) * baseCanvas.height,
    };
  };

  const onEditorPointerDown = (e: React.PointerEvent<HTMLCanvasElement>) => {
    const canvas = editorCanvasRef.current;
    if (!canvas || !baseCanvas) return;
    const pt = getEditorCoords(e, canvas);
    e.currentTarget.setPointerCapture(e.pointerId);

    if (isPointInsideRuler(pt)) {
      setDraggingRulerOffset({ x: pt.x - ruler.x, y: pt.y - ruler.y });
      return;
    }

    if (tool === AnnotationTool.Crop) {
      setCropDragStart(pt);
      setCropRect({ x: Math.round(pt.x), y: Math.round(pt.y), width: 1, height: 1 });
      return;
    }

    if (tool === AnnotationTool.Eraser) {
      const hitIdx = [...annotations].reverse().findIndex((op) => hitTestAnnotation(pt, op, 14));
      if (hitIdx !== -1) {
        const actualIdx = annotations.length - 1 - hitIdx;
        pushUndoSnapshot();
        setAnnotations((prev) => prev.filter((_, idx) => idx !== actualIdx));
        setStatusMessage('Erased stroke.');
      }
      return;
    }

    const snapped = snapPointToRuler(pt, ruler);
    setActiveOp({
      id: `op-${Date.now()}`,
      tool,
      color: inkColor,
      strokeWidth,
      points: [snapped],
    });
  };

  const onEditorPointerMove = (e: React.PointerEvent<HTMLCanvasElement>) => {
    const canvas = editorCanvasRef.current;
    if (!canvas || !baseCanvas) return;
    const pt = getEditorCoords(e, canvas);

    if (draggingRulerOffset) {
      setRuler((prev) => ({
        ...prev,
        x: pt.x - draggingRulerOffset.x,
        y: pt.y - draggingRulerOffset.y,
      }));
      return;
    }

    if (tool === AnnotationTool.Crop && cropDragStart) {
      setCropRect(normalizeRect(cropDragStart, pt, baseCanvas.width, baseCanvas.height));
      return;
    }

    if (tool === AnnotationTool.Eraser && e.buttons === 1) {
      const hitIdx = [...annotations].reverse().findIndex((op) => hitTestAnnotation(pt, op, 14));
      if (hitIdx !== -1) {
        const actualIdx = annotations.length - 1 - hitIdx;
        pushUndoSnapshot();
        setAnnotations((prev) => prev.filter((_, idx) => idx !== actualIdx));
      }
      return;
    }

    if (!activeOp) return;
    const snapped = snapPointToRuler(pt, ruler);
    if (activeOp.tool === AnnotationTool.Pen || activeOp.tool === AnnotationTool.Highlighter) {
      setActiveOp((prev) => (prev ? { ...prev, points: [...prev.points, snapped] } : null));
    } else {
      setActiveOp((prev) => (prev ? { ...prev, points: [prev.points[0], snapped] } : null));
    }
  };

  const onEditorPointerUp = () => {
    if (draggingRulerOffset) {
      setDraggingRulerOffset(null);
      return;
    }
    if (cropDragStart) {
      setCropDragStart(null);
      return;
    }
    if (activeOp) {
      const validShape =
        activeOp.tool === AnnotationTool.Pen ||
        activeOp.tool === AnnotationTool.Highlighter ||
        activeOp.points.length >= 2;
      if (validShape) {
        pushUndoSnapshot();
        setAnnotations((prev) => [...prev, activeOp]);
      }
      setActiveOp(null);
    }
  };

  const selectTool = (nextTool: AnnotationTool) => {
    setTool(nextTool);
    if (nextTool === AnnotationTool.Crop && baseCanvas) {
      const mx = Math.max(12, Math.round(baseCanvas.width * 0.1));
      const my = Math.max(12, Math.round(baseCanvas.height * 0.1));
      setCropRect({
        x: mx,
        y: my,
        width: Math.max(24, baseCanvas.width - mx * 2),
        height: Math.max(24, baseCanvas.height - my * 2),
      });
    } else {
      setCropRect(null);
    }
  };

  const applyCrop = () => {
    if (!baseCanvas || !cropRect || cropRect.width < 12 || cropRect.height < 12) {
      setStatusMessage('Select a crop region of at least 12 × 12 px.');
      return;
    }
    const flattened = buildFlattenedCanvas();
    if (!flattened) return;

    pushUndoSnapshot();
    const cropped = extractSubCanvas(
      flattened,
      cropRect.x,
      cropRect.y,
      cropRect.width,
      cropRect.height
    );
    setBaseCanvas(cropped);
    setAnnotations([]);
    setCropRect(null);
    setTool(AnnotationTool.Pen);
    setStatusMessage(`Applied crop — new size ${cropped.width} × ${cropped.height} px.`);
  };

  const handleClearAllInk = () => {
    if (annotations.length === 0) return;
    pushUndoSnapshot();
    setAnnotations([]);
    setStatusMessage('Cleared all annotations.');
  };

  const handleFileUpload = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = () => {
      const img = new Image();
      img.onload = () => {
        const c = document.createElement('canvas');
        c.width = img.naturalWidth;
        c.height = img.naturalHeight;
        const ctx = c.getContext('2d');
        if (ctx) {
          ctx.drawImage(img, 0, 0);
          setBaseCanvas(c);
          setAnnotations([]);
          setUndoStack([]);
          setRedoStack([]);
          setCropRect(null);
          setZoom(1);
          setIsSettingsOpen(false);
          setStatusMessage(`Opened ${file.name} (${c.width} × ${c.height} px).`);
        }
      };
      img.src = String(reader.result);
    };
    reader.readAsDataURL(file);
    e.target.value = '';
  };

  const widthPx = baseCanvas?.width ?? 0;
  const heightPx = baseCanvas?.height ?? 0;
  const editorDims = useMemo(
    () => computeSnipWindowDimensions(widthPx, heightPx, 1280, 820),
    [widthPx, heightPx]
  );

  const shortcutHint = printScreenEnabled
    ? `PrintScreen or ${formattedCustomShortcut}`
    : formattedCustomShortcut;

  const renderShortcutSettingsPopover = () => (
    <div
      role="dialog"
      aria-label="Capture keyboard shortcuts"
      className="absolute right-0 top-full mt-1.5 w-76 sm:w-80 rounded-xl bg-white border border-slate-200 shadow-xl p-3.5 z-30 text-xs text-left"
    >
      <div className="flex items-center justify-between gap-2 pb-2 mb-2.5 border-b border-slate-100">
        <div className="inline-flex items-center gap-1.5 font-semibold text-slate-800">
          <Keyboard className="w-3.5 h-3.5 text-blue-600 shrink-0" aria-hidden="true" />
          <span>Capture Shortcuts</span>
        </div>
        <button
          type="button"
          aria-label="Close shortcut settings"
          onClick={() => {
            setIsSettingsOpen(false);
            setIsRecordingShortcut(false);
            setShortcutError(null);
          }}
          className="p-0.5 text-slate-400 hover:text-slate-700 rounded cursor-pointer"
        >
          <X className="w-3.5 h-3.5" aria-hidden="true" />
        </button>
      </div>

      <div className="space-y-2.5">
        <label
          htmlFor="printscreen-shortcut-toggle"
          className="flex items-center justify-between gap-2 text-slate-700 cursor-pointer select-none"
        >
          <span>Bind PrintScreen key</span>
          <input
            id="printscreen-shortcut-toggle"
            type="checkbox"
            checked={printScreenEnabled}
            onChange={(e) => {
              const enabled = e.target.checked;
              setPrintScreenEnabled(enabled);
              setStatusMessage(
                enabled
                  ? 'PrintScreen capture shortcut enabled.'
                  : 'PrintScreen capture shortcut disabled.'
              );
            }}
            className="rounded border-slate-300 text-blue-600 focus:ring-blue-500 cursor-pointer"
          />
        </label>

        <div>
          <label
            htmlFor="custom-shortcut-select"
            className="block text-[11px] font-medium text-slate-500 mb-1"
          >
            Custom Capture Shortcut
          </label>
          <select
            id="custom-shortcut-select"
            aria-label="Custom Capture Shortcut"
            value={formattedCustomShortcut}
            onChange={(e) => {
              const parsed = parseShortcutString(e.target.value);
              if (parsed) {
                setCustomShortcut(parsed);
                setIsRecordingShortcut(false);
                setShortcutError(null);
                setStatusMessage(`Selected capture shortcut: ${formatShortcutBinding(parsed)}.`);
              }
            }}
            className="w-full px-2.5 py-1.5 text-xs font-mono font-medium text-slate-800 bg-slate-50 border border-slate-300 rounded-md hover:bg-slate-100 transition-colors cursor-pointer"
          >
            {!SHORTCUT_PRESETS.some(
              (preset) => formatShortcutBinding(preset.binding) === formattedCustomShortcut
            ) && (
              <option value={formattedCustomShortcut}>
                {formattedCustomShortcut} (Custom)
              </option>
            )}
            {SHORTCUT_PRESETS.map((preset) => {
              const val = formatShortcutBinding(preset.binding);
              return (
                <option key={val} value={val}>
                  {preset.label}
                </option>
              );
            })}
          </select>
        </div>

        <div className="flex items-center justify-between gap-2 pt-0.5">
          <button
            type="button"
            aria-pressed={isRecordingShortcut}
            onClick={() => {
              setShortcutError(null);
              setIsRecordingShortcut((prev) => {
                const next = !prev;
                setStatusMessage(
                  next
                    ? 'Press your desired key combination now (or Esc to cancel)…'
                    : 'Shortcut recording cancelled.'
                );
                return next;
              });
            }}
            className={`px-2.5 py-1 font-medium rounded-md border transition-colors whitespace-nowrap shrink-0 cursor-pointer ${
              isRecordingShortcut
                ? 'bg-blue-600 text-white border-blue-600'
                : 'bg-white text-slate-700 border-slate-300 hover:bg-slate-50'
            }`}
          >
            {isRecordingShortcut ? 'Press keys… (Esc)' : 'Record Shortcut'}
          </button>

          {formattedCustomShortcut !== formatShortcutBinding(DEFAULT_CUSTOM_SHORTCUT) &&
            !isRecordingShortcut && (
              <button
                type="button"
                onClick={() => {
                  setCustomShortcut(DEFAULT_CUSTOM_SHORTCUT);
                  setShortcutError(null);
                  setStatusMessage(
                    `Reset custom shortcut to ${formatShortcutBinding(DEFAULT_CUSTOM_SHORTCUT)}.`
                  );
                }}
                className="text-slate-500 hover:text-slate-900 underline underline-offset-2 whitespace-nowrap shrink-0 cursor-pointer"
              >
                Reset default
              </button>
            )}
        </div>

        <div className="pt-2 border-t border-slate-100 font-mono tabular-nums text-[11px] text-slate-500">
          {shortcutError ? (
            <span role="alert" className="font-sans text-red-600 font-medium">
              {shortcutError}
            </span>
          ) : (
            <span>
              Active:{' '}
              {printScreenEnabled
                ? `PrintScreen · ${formattedCustomShortcut}`
                : formattedCustomShortcut}
            </span>
          )}
        </div>
      </div>
    </div>
  );

  return (
    <section id="studio" className="pt-8 pb-12">
      <div className="max-w-[1280px] mx-auto px-6">
        <input
          ref={fileInputRef}
          type="file"
          accept="image/*"
          onChange={handleFileUpload}
          className="hidden"
          aria-label="Upload local image file"
        />

        {/* Intro Header */}
        <div className="flex flex-col lg:flex-row lg:items-end lg:justify-between gap-4 mb-6">
          <div>
            <h1 className="text-2xl sm:text-3xl font-bold tracking-tight text-slate-900 [text-wrap:balance]">
              Windows Snipping Tool for Debian Linux
            </h1>
            <p className="mt-1.5 text-sm sm:text-base text-slate-600 max-w-2xl">
              Native X11 and Wayland screenshot utility packaged as an installable{' '}
              <code className="font-mono text-xs text-slate-800">{debFilename}</code> release. Test
              all capture modes, delay timers, ruler snapping, redaction, and annotations live below.
            </p>
          </div>

          <div className="flex flex-wrap items-center gap-2 text-xs text-slate-500 font-mono tabular-nums">
            <span>Version {version}</span>
            <span aria-hidden="true">·</span>
            <span>Session: {sessionInfo.protocol.toUpperCase()}</span>
            <span aria-hidden="true">·</span>
            <span>{sessionInfo.desktopEnvironment}</span>
          </div>
        </div>

        {/* Subtle Desktop Workspace Stage */}
        <div className="rounded-2xl border border-slate-200/90 bg-gradient-to-b from-slate-100 to-slate-200/60 px-4 py-8 sm:px-8 sm:py-10">
          {isCapturing ? (
            /* 1. Capture Overlay Mode */
            <div className="max-w-[1120px] mx-auto bg-slate-900 border border-slate-800 rounded-xl shadow-xl p-4 flex flex-col items-center">
              {/* Floating Windows 11 Capture Mode Switcher Pill */}
              <div
                role="toolbar"
                aria-label="Live capture mode switcher"
                className="mb-3 flex flex-wrap items-center gap-1 p-1.5 bg-slate-800 border border-slate-700 rounded-lg text-xs shadow-md"
              >
                {(
                  [
                    { mode: SnipMode.Rectangle, label: 'Rectangle' },
                    { mode: SnipMode.Window, label: 'Window' },
                    { mode: SnipMode.Fullscreen, label: 'Fullscreen' },
                    { mode: SnipMode.Freeform, label: 'Freeform' },
                  ] as const
                ).map((item) => (
                  <button
                    key={item.mode}
                    type="button"
                    onClick={() => {
                      if (item.mode === SnipMode.Fullscreen) {
                        const full = cloneCanvas(simulatedDesktop);
                        setBaseCanvas(full);
                        setAnnotations([]);
                        setUndoStack([]);
                        setRedoStack([]);
                        setCropRect(null);
                        setZoom(0.75);
                        setIsCapturing(false);
                        setStatusMessage(
                          `Captured Fullscreen snip (${full.width} × ${full.height} px).`
                        );
                      } else {
                        setSnipMode(item.mode);
                      }
                    }}
                    className={`px-3 py-1.5 font-semibold rounded-md transition-colors whitespace-nowrap shrink-0 cursor-pointer ${
                      snipMode === item.mode
                        ? 'bg-blue-600 text-white'
                        : 'text-slate-300 hover:bg-slate-700 hover:text-white'
                    }`}
                  >
                    {item.label}
                  </button>
                ))}
                <button
                  type="button"
                  onClick={() => {
                    setIsCapturing(false);
                    setStatusMessage('Snip cancelled.');
                  }}
                  className="px-3 py-1.5 font-medium text-slate-300 hover:bg-slate-700 hover:text-white rounded-md whitespace-nowrap shrink-0 cursor-pointer"
                >
                  Cancel (Esc)
                </button>
              </div>

              <p className="text-xs text-slate-300 mb-3 text-center">
                {snipMode === SnipMode.Rectangle &&
                  'Drag across any region of the simulated Debian desktop to capture a rectangular snip.'}
                {snipMode === SnipMode.Window &&
                  'Click any open Debian window below to snap-capture its exact window frame.'}
                {snipMode === SnipMode.Freeform &&
                  'Drag a freehand lasso loop around any part of the screen to clip a custom shape.'}
              </p>

              <div className="relative w-full max-w-[1120px]">
                <canvas
                  ref={captureCanvasRef}
                  data-testid="capture-overlay-canvas"
                  onPointerDown={onCapturePointerDown}
                  onPointerMove={onCapturePointerMove}
                  onPointerUp={onCapturePointerUp}
                  className="w-full h-auto rounded-lg border border-slate-700 cursor-crosshair touch-none block"
                />

                {/* Accessible Window Buttons when in Window Snip Mode */}
                {snipMode === SnipMode.Window && (
                  <div className="mt-3 flex flex-wrap items-center justify-center gap-2">
                    {SIMULATED_WINDOWS.map((win) => (
                      <button
                        key={win.id}
                        type="button"
                        onMouseEnter={() => setHoveredWindow(win)}
                        onClick={() => completeWindowCapture(win)}
                        className="px-3 py-1.5 text-xs font-medium text-slate-200 bg-slate-800 border border-slate-700 rounded-md hover:border-sky-400 hover:text-white transition-colors whitespace-nowrap shrink-0 cursor-pointer"
                      >
                        Capture Window: {win.title.split('—')[0].split(':')[0].trim()} ({win.width}×
                        {win.height})
                      </button>
                    ))}
                  </div>
                )}
              </div>
            </div>
          ) : !baseCanvas ? (
            /* 2. Compact Microsoft Snipping Tool Launcher Mode (460 × 200 px) */
            <div
              data-testid="snip-launcher-window"
              style={{
                width: '100%',
                maxWidth: `${LAUNCHER_WINDOW_WIDTH}px`,
                minHeight: `${LAUNCHER_WINDOW_HEIGHT}px`,
              }}
              className="mx-auto bg-white border border-slate-300 rounded-xl shadow-lg flex flex-col relative"
            >
              {/* Compact Title Bar (32px) */}
              <div className="h-8 px-3 bg-slate-100 border-b border-slate-200 rounded-t-xl flex items-center justify-between gap-2 text-xs">
                <div className="flex items-center gap-1.5 min-w-0">
                  <Scissors className="w-3.5 h-3.5 text-blue-600 shrink-0" aria-hidden="true" />
                  <span className="font-semibold text-slate-800 truncate">
                    SnipClip v{version}
                  </span>
                  <span
                    title={`Auto-detected via ${sessionInfo.detectionSource}`}
                    className="px-1.5 py-0.5 rounded bg-slate-200/85 text-[10px] font-mono font-semibold text-slate-700 shrink-0"
                  >
                    {sessionInfo.protocol.toUpperCase()}
                  </span>
                </div>
                <span className="text-[11px] font-mono tabular-nums text-slate-500 shrink-0">
                  {LAUNCHER_WINDOW_WIDTH}×{LAUNCHER_WINDOW_HEIGHT}
                </span>
              </div>

              {/* Compact Single-Row Windows 11 Command Bar (48px) */}
              <div className="h-12 px-2.5 bg-white border-b border-slate-200 flex items-center justify-between gap-1.5">
                <div className="flex items-center gap-1.5 min-w-0">
                  <button
                    type="button"
                    onClick={startNewSnip}
                    disabled={countdownLeft !== null}
                    title={`Start a new snip (${shortcutHint})`}
                    className="inline-flex items-center gap-1 px-2.5 py-1.5 text-xs font-semibold text-white bg-blue-600 rounded-md hover:bg-blue-700 disabled:opacity-60 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
                  >
                    <Plus className="w-3.5 h-3.5 shrink-0" aria-hidden="true" />
                    <span>{countdownLeft !== null ? `${countdownLeft}s…` : 'New'}</span>
                  </button>

                  <label className="sr-only" htmlFor="snip-mode-select">
                    Snipping Mode
                  </label>
                  <select
                    id="snip-mode-select"
                    aria-label="Snipping Mode"
                    value={snipMode}
                    onChange={(e) => setSnipMode(e.target.value as SnipMode)}
                    className="px-2 py-1.5 text-xs font-medium text-slate-800 bg-slate-50 border border-slate-300 rounded-md hover:bg-slate-100 transition-colors cursor-pointer"
                  >
                    <option value={SnipMode.Rectangle}>Rectangle</option>
                    <option value={SnipMode.Window}>Window</option>
                    <option value={SnipMode.Fullscreen}>Fullscreen</option>
                    <option value={SnipMode.Freeform}>Freeform</option>
                  </select>

                  <label className="sr-only" htmlFor="snip-delay-select">
                    Delay Timer
                  </label>
                  <select
                    id="snip-delay-select"
                    aria-label="Delay Timer"
                    value={delaySeconds}
                    onChange={(e) => setDelaySeconds(Number(e.target.value))}
                    className="px-2 py-1.5 text-xs font-medium text-slate-800 bg-slate-50 border border-slate-300 rounded-md hover:bg-slate-100 transition-colors cursor-pointer"
                  >
                    <option value={0}>No delay</option>
                    <option value={3}>3s</option>
                    <option value={5}>5s</option>
                    <option value={10}>10s</option>
                  </select>
                </div>

                <div ref={settingsContainerRef} className="relative flex items-center gap-1 shrink-0">
                  <button
                    type="button"
                    aria-label="Open Image"
                    title="Open local image file"
                    onClick={() => fileInputRef.current?.click()}
                    className="p-1.5 text-slate-700 bg-white border border-slate-200 rounded-md hover:bg-slate-100 transition-colors cursor-pointer"
                  >
                    <FolderOpen className="w-4 h-4" aria-hidden="true" />
                  </button>

                  <button
                    type="button"
                    aria-label="Capture shortcut settings"
                    aria-expanded={isSettingsOpen}
                    title={`Capture shortcuts (${shortcutHint})`}
                    onClick={() => setIsSettingsOpen((prev) => !prev)}
                    className={`p-1.5 border rounded-md transition-colors cursor-pointer ${
                      isSettingsOpen
                        ? 'bg-slate-100 text-blue-600 border-slate-300'
                        : 'bg-white text-slate-700 border-slate-200 hover:bg-slate-100'
                    }`}
                  >
                    <MoreHorizontal className="w-4 h-4" aria-hidden="true" />
                  </button>

                  {isSettingsOpen && renderShortcutSettingsPopover()}
                </div>
              </div>

              {/* Compact Launcher Body (~118px -> 460 × 200 px total window frame) */}
              <div className="flex-1 min-h-[118px] px-4 py-3 flex flex-col items-center justify-center text-center bg-slate-50/70 rounded-b-xl">
                <p className="text-xs font-medium text-slate-700">
                  {countdownLeft !== null
                    ? `Capturing ${snipMode} snip in ${countdownLeft}s… (Esc to cancel)`
                    : "Press PrintScreen or click '+ New' to snip"}
                </p>
                <button
                  type="button"
                  onClick={handleLoadSampleWindow}
                  className="mt-2 text-[11px] font-medium text-blue-600 hover:text-blue-800 underline underline-offset-2 cursor-pointer"
                >
                  Load sample window snip
                </button>
              </div>
            </div>
          ) : (
            /* 3. Dynamic Editor Mode (Sized Naturally to Captured Snip) */
            <div
              data-testid="snip-editor-window"
              style={{ width: '100%', maxWidth: `${editorDims.width}px` }}
              className="mx-auto bg-white border border-slate-300 rounded-xl shadow-lg flex flex-col relative transition-[max-width] duration-150"
            >
              {/* Compact Editor Title Bar */}
              <div className="px-3 py-1.5 bg-slate-100 border-b border-slate-200 rounded-t-xl flex items-center justify-between gap-2 text-xs">
                <div className="flex items-center gap-2 min-w-0">
                  <Scissors className="w-3.5 h-3.5 text-blue-600 shrink-0" aria-hidden="true" />
                  <span className="font-semibold text-slate-800 truncate">
                    SnipClip v{version}
                  </span>
                  <span
                    title={`Auto-detected via ${sessionInfo.detectionSource}`}
                    className="px-1.5 py-0.5 rounded bg-slate-200/85 text-[10px] font-mono font-semibold text-slate-700 shrink-0"
                  >
                    {sessionInfo.protocol.toUpperCase()}
                  </span>
                  <span className="hidden sm:inline text-[11px] font-mono tabular-nums text-slate-500">
                    {widthPx}×{heightPx}px
                  </span>
                </div>

                <div className="flex items-center gap-2 shrink-0">
                  <span className="hidden md:inline text-[11px] font-mono tabular-nums text-slate-400">
                    {editorDims.width}×{editorDims.height}
                  </span>
                  <button
                    type="button"
                    aria-label="Close snip"
                    title="Close snip and return to compact 460×200 launcher"
                    onClick={handleResetToLauncher}
                    className="inline-flex items-center gap-1 px-2 py-0.5 text-[11px] font-medium text-slate-600 hover:text-slate-900 bg-white border border-slate-200 rounded hover:bg-slate-50 transition-colors cursor-pointer"
                  >
                    <X className="w-3 h-3" aria-hidden="true" />
                    <span>Close snip</span>
                  </button>
                </div>
              </div>

              {/* Sleek Windows 11 Editor Command & Annotation Header */}
              <div className="flex flex-wrap items-center justify-between gap-1.5 px-2.5 py-2 bg-white border-b border-slate-200">
                {/* Capture Controls */}
                <div className="flex items-center gap-1.5">
                  <button
                    type="button"
                    onClick={startNewSnip}
                    disabled={countdownLeft !== null}
                    title={`Start a new snip (${shortcutHint})`}
                    className="inline-flex items-center gap-1 px-2.5 py-1.5 text-xs font-semibold text-white bg-blue-600 rounded-md hover:bg-blue-700 disabled:opacity-60 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
                  >
                    <Plus className="w-3.5 h-3.5 shrink-0" aria-hidden="true" />
                    <span>{countdownLeft !== null ? `${countdownLeft}s…` : 'New'}</span>
                  </button>

                  <label className="sr-only" htmlFor="snip-mode-select">
                    Snipping Mode
                  </label>
                  <select
                    id="snip-mode-select"
                    aria-label="Snipping Mode"
                    value={snipMode}
                    onChange={(e) => setSnipMode(e.target.value as SnipMode)}
                    className="px-2 py-1.5 text-xs font-medium text-slate-800 bg-slate-50 border border-slate-300 rounded-md hover:bg-slate-100 transition-colors cursor-pointer"
                  >
                    <option value={SnipMode.Rectangle}>Rectangle</option>
                    <option value={SnipMode.Window}>Window</option>
                    <option value={SnipMode.Fullscreen}>Fullscreen</option>
                    <option value={SnipMode.Freeform}>Freeform</option>
                  </select>

                  <label className="sr-only" htmlFor="snip-delay-select">
                    Delay Timer
                  </label>
                  <select
                    id="snip-delay-select"
                    aria-label="Delay Timer"
                    value={delaySeconds}
                    onChange={(e) => setDelaySeconds(Number(e.target.value))}
                    className="px-2 py-1.5 text-xs font-medium text-slate-800 bg-slate-50 border border-slate-300 rounded-md hover:bg-slate-100 transition-colors cursor-pointer"
                  >
                    <option value={0}>No delay</option>
                    <option value={3}>3s</option>
                    <option value={5}>5s</option>
                    <option value={10}>10s</option>
                  </select>
                </div>

                {/* Annotation Tool Strip */}
                <div
                  role="toolbar"
                  aria-label="Annotation tools"
                  className="flex flex-wrap items-center gap-0.5 p-0.5 bg-slate-100 rounded-lg"
                >
                  {(
                    [
                      { id: AnnotationTool.Pen, label: 'Pen', Icon: PenTool },
                      { id: AnnotationTool.Highlighter, label: 'Highlighter', Icon: Highlighter },
                      { id: AnnotationTool.Eraser, label: 'Eraser', Icon: Eraser },
                      { id: AnnotationTool.Rect, label: 'Rectangle', Icon: Square },
                      { id: AnnotationTool.Ellipse, label: 'Ellipse', Icon: Circle },
                      { id: AnnotationTool.Line, label: 'Line', Icon: Minus },
                      { id: AnnotationTool.Arrow, label: 'Arrow', Icon: ArrowUpRight },
                      { id: AnnotationTool.Redact, label: 'Redact', Icon: EyeOff },
                      { id: AnnotationTool.Pixelate, label: 'Pixelate', Icon: Grid },
                      { id: AnnotationTool.Crop, label: 'Crop', Icon: Crop },
                    ] as const
                  ).map(({ id, label, Icon }) => {
                    const active = !isCapturing && tool === id;
                    return (
                      <button
                        key={id}
                        type="button"
                        title={label}
                        aria-label={label}
                        aria-pressed={active}
                        onClick={() => selectTool(id)}
                        className={`inline-flex items-center justify-center p-1.5 text-xs font-medium rounded-md transition-colors shrink-0 cursor-pointer ${
                          active
                            ? 'bg-white text-blue-700 shadow-xs'
                            : 'text-slate-600 hover:text-slate-900'
                        }`}
                      >
                        <Icon className="w-3.5 h-3.5 shrink-0" aria-hidden="true" />
                      </button>
                    );
                  })}

                  <button
                    type="button"
                    title="Toggle Ruler"
                    aria-label="Toggle Ruler"
                    aria-pressed={ruler.visible}
                    onClick={() => setRuler((prev) => ({ ...prev, visible: !prev.visible }))}
                    className={`inline-flex items-center justify-center p-1.5 text-xs font-medium rounded-md transition-colors shrink-0 cursor-pointer ${
                      ruler.visible
                        ? 'bg-white text-blue-700 shadow-xs'
                        : 'text-slate-600 hover:text-slate-900'
                    }`}
                  >
                    <Ruler className="w-3.5 h-3.5 shrink-0" aria-hidden="true" />
                  </button>
                </div>

                {/* History, Export, & Shortcut Popover Actions */}
                <div ref={settingsContainerRef} className="relative flex items-center gap-1">
                  <button
                    type="button"
                    aria-label="Undo"
                    title="Undo (Ctrl+Z)"
                    disabled={undoStack.length === 0}
                    onClick={handleUndo}
                    className="p-1.5 text-slate-700 bg-white border border-slate-200 rounded-md hover:bg-slate-50 disabled:opacity-40 transition-colors cursor-pointer"
                  >
                    <Undo2 className="w-3.5 h-3.5" aria-hidden="true" />
                  </button>
                  <button
                    type="button"
                    aria-label="Redo"
                    title="Redo (Ctrl+Y)"
                    disabled={redoStack.length === 0}
                    onClick={handleRedo}
                    className="p-1.5 text-slate-700 bg-white border border-slate-200 rounded-md hover:bg-slate-50 disabled:opacity-40 transition-colors cursor-pointer"
                  >
                    <Redo2 className="w-3.5 h-3.5" aria-hidden="true" />
                  </button>

                  <button
                    type="button"
                    title="Copy PNG to clipboard (Ctrl+C)"
                    onClick={() => void handleCopySnip()}
                    className="inline-flex items-center gap-1 px-2.5 py-1.5 text-xs font-medium text-slate-800 bg-white border border-slate-300 rounded-md hover:bg-slate-50 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
                  >
                    {copyFeedback ? (
                      <Check className="w-3.5 h-3.5 text-emerald-600 shrink-0" aria-hidden="true" />
                    ) : (
                      <Copy className="w-3.5 h-3.5 shrink-0" aria-hidden="true" />
                    )}
                    <span>{copyFeedback ? 'Copied' : 'Copy'}</span>
                  </button>

                  <button
                    type="button"
                    title="Save PNG (Ctrl+S)"
                    onClick={handleSavePng}
                    className="inline-flex items-center gap-1 px-2.5 py-1.5 text-xs font-medium text-slate-800 bg-white border border-slate-300 rounded-md hover:bg-slate-50 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
                  >
                    <Download className="w-3.5 h-3.5 shrink-0" aria-hidden="true" />
                    <span>Save PNG</span>
                  </button>

                  <button
                    type="button"
                    aria-label="Open Image"
                    title="Open local image file"
                    onClick={() => fileInputRef.current?.click()}
                    className="p-1.5 text-slate-700 bg-white border border-slate-200 rounded-md hover:bg-slate-50 transition-colors cursor-pointer"
                  >
                    <FolderOpen className="w-3.5 h-3.5" aria-hidden="true" />
                  </button>

                  <button
                    type="button"
                    aria-label="Capture shortcut settings"
                    aria-expanded={isSettingsOpen}
                    title={`Capture shortcuts (${shortcutHint})`}
                    onClick={() => setIsSettingsOpen((prev) => !prev)}
                    className={`p-1.5 border rounded-md transition-colors cursor-pointer ${
                      isSettingsOpen
                        ? 'bg-slate-100 text-blue-600 border-slate-300'
                        : 'bg-white text-slate-700 border-slate-200 hover:bg-slate-50'
                    }`}
                  >
                    <MoreHorizontal className="w-3.5 h-3.5" aria-hidden="true" />
                  </button>

                  {isSettingsOpen && renderShortcutSettingsPopover()}
                </div>
              </div>

              {/* Compact Ink Style, Ruler Angle, & Zoom Sub-Strip */}
              <div className="flex flex-wrap items-center justify-between gap-2.5 px-3 py-1.5 bg-slate-50 border-b border-slate-200 text-xs">
                <div className="flex flex-wrap items-center gap-3">
                  {/* Color Swatches */}
                  <div className="flex items-center gap-1.5" role="group" aria-label="Ink color">
                    {INK_COLORS.map((c) => (
                      <button
                        key={c.hex}
                        type="button"
                        aria-label={`Select ${c.name} ink`}
                        onClick={() => setInkColor(c.hex)}
                        className={`w-4 h-4 rounded-full transition-transform cursor-pointer ${c.swatchClass} ${
                          inkColor === c.hex
                            ? 'ring-2 ring-offset-1 ring-blue-600 scale-110'
                            : 'hover:scale-105'
                        }`}
                      />
                    ))}
                  </div>

                  {/* Stroke Width */}
                  <div className="flex items-center gap-1.5">
                    <label htmlFor="stroke-width-range" className="text-slate-500 font-medium">
                      Size
                    </label>
                    <input
                      id="stroke-width-range"
                      type="range"
                      min={1}
                      max={14}
                      value={strokeWidth}
                      onChange={(e) => setStrokeWidth(Number(e.target.value))}
                      className="w-16 accent-blue-600 cursor-pointer"
                    />
                    <span className="font-mono tabular-nums text-slate-700 w-6">
                      {strokeWidth}px
                    </span>
                  </div>

                  {/* Ruler Angle Controls when Ruler is active */}
                  {ruler.visible && (
                    <div className="flex items-center gap-1.5 pl-2 border-l border-slate-200">
                      <label htmlFor="ruler-angle-range" className="text-slate-600 font-medium">
                        Angle
                      </label>
                      <input
                        id="ruler-angle-range"
                        type="range"
                        min={0}
                        max={179}
                        value={ruler.angleDeg}
                        onChange={(e) =>
                          setRuler((prev) => ({ ...prev, angleDeg: Number(e.target.value) }))
                        }
                        className="w-20 accent-blue-600 cursor-pointer"
                      />
                      <span className="font-mono tabular-nums text-slate-800 w-8">
                        {ruler.angleDeg}°
                      </span>
                      <button
                        type="button"
                        onClick={() => setRuler((prev) => ({ ...prev, angleDeg: 0 }))}
                        className="px-1.5 py-0.5 text-[11px] text-slate-600 hover:text-slate-900 bg-white border border-slate-200 rounded whitespace-nowrap shrink-0 cursor-pointer"
                      >
                        0°
                      </button>
                    </div>
                  )}

                  {/* Clear All Ink */}
                  {annotations.length > 0 && (
                    <button
                      type="button"
                      onClick={handleClearAllInk}
                      className="inline-flex items-center gap-1 text-slate-600 hover:text-red-600 font-medium transition-colors whitespace-nowrap shrink-0 cursor-pointer"
                    >
                      <Trash2 className="w-3.5 h-3.5" aria-hidden="true" />
                      <span>Clear ({annotations.length})</span>
                    </button>
                  )}
                </div>

                {/* Zoom Controls */}
                <div className="flex items-center gap-1 font-mono tabular-nums">
                  <button
                    type="button"
                    aria-label="Zoom out"
                    onClick={() => setZoom((z) => Math.max(0.25, Number((z - 0.25).toFixed(2))))}
                    className="p-0.5 text-slate-600 hover:text-slate-900 rounded cursor-pointer"
                  >
                    <ZoomOut className="w-3.5 h-3.5" aria-hidden="true" />
                  </button>
                  <span className="text-slate-700 w-10 text-center">
                    {Math.round(zoom * 100)}%
                  </span>
                  <button
                    type="button"
                    aria-label="Zoom in"
                    onClick={() => setZoom((z) => Math.min(3, Number((z + 0.25).toFixed(2))))}
                    className="p-0.5 text-slate-600 hover:text-slate-900 rounded cursor-pointer"
                  >
                    <ZoomIn className="w-3.5 h-3.5" aria-hidden="true" />
                  </button>
                  {zoom !== 1 && (
                    <button
                      type="button"
                      aria-label="Reset zoom to 100%"
                      onClick={() => setZoom(1)}
                      className="p-0.5 text-slate-500 hover:text-slate-900 rounded cursor-pointer"
                    >
                      <RotateCcw className="w-3.5 h-3.5" aria-hidden="true" />
                    </button>
                  )}
                </div>
              </div>

              {/* Contextual Crop Action Bar */}
              {tool === AnnotationTool.Crop && cropRect && (
                <div className="flex flex-wrap items-center justify-between gap-2 px-3 py-2 bg-blue-50 border-b border-blue-200 text-xs">
                  <div className="text-blue-950 font-medium font-mono tabular-nums">
                    Crop: {cropRect.width} × {cropRect.height} px (at {cropRect.x}, {cropRect.y})
                  </div>
                  <div className="flex items-center gap-1.5">
                    <button
                      type="button"
                      onClick={applyCrop}
                      className="px-2.5 py-1 font-semibold text-white bg-blue-600 rounded hover:bg-blue-700 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
                    >
                      Apply Crop
                    </button>
                    <button
                      type="button"
                      onClick={() => {
                        setCropRect(null);
                        setTool(AnnotationTool.Pen);
                      }}
                      className="px-2.5 py-1 font-medium text-slate-700 bg-white border border-slate-300 rounded hover:bg-slate-50 transition-colors whitespace-nowrap shrink-0 cursor-pointer"
                    >
                      Cancel Crop
                    </button>
                  </div>
                </div>
              )}

              {/* Editor Canvas Viewport */}
              <div
                style={{
                  minHeight: '220px',
                  maxHeight: `${Math.max(260, editorDims.height - 120)}px`,
                }}
                className="bg-slate-100/90 overflow-auto p-4 flex"
              >
                <div className="relative m-auto shrink-0 shadow-sm border border-slate-300 bg-white">
                  <canvas
                    ref={editorCanvasRef}
                    data-testid="snip-editor-canvas"
                    data-width={widthPx}
                    data-height={heightPx}
                    data-annotations={annotations.length}
                    onPointerDown={onEditorPointerDown}
                    onPointerMove={onEditorPointerMove}
                    onPointerUp={onEditorPointerUp}
                    className="max-w-none block cursor-crosshair touch-none"
                  />
                </div>
              </div>

              {/* Slim Editor Status Footer */}
              <div className="flex flex-wrap items-center justify-between gap-2 px-3 py-1.5 bg-white border-t border-slate-200 rounded-b-xl text-[11px] text-slate-500">
                <span className="truncate max-w-[65%]">{statusMessage}</span>
                <div className="flex items-center gap-1.5 font-mono tabular-nums shrink-0">
                  <span>
                    {widthPx} × {heightPx} px
                  </span>
                  <span aria-hidden="true">·</span>
                  <span>{annotations.length} ink</span>
                </div>
              </div>
            </div>
          )}
        </div>
      </div>
    </section>
  );
};
