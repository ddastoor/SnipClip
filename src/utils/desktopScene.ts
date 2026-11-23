import { DisplayProtocol } from './snipEngine';

export interface SimulatedWindow {
  id: string;
  title: string;
  x: number;
  y: number;
  width: number;
  height: number;
}

export const DESKTOP_WIDTH = 1120;
export const DESKTOP_HEIGHT = 620;

export const SIMULATED_WINDOWS: SimulatedWindow[] = [
  {
    id: 'win-terminal',
    title: 'Software Center (Discover) — SnipClip Installed',
    x: 44,
    y: 58,
    width: 590,
    height: 356,
  },
  {
    id: 'win-monitor',
    title: 'Debian Display & Compositor Inspector',
    x: 662,
    y: 58,
    width: 414,
    height: 256,
  },
  {
    id: 'win-notes',
    title: 'Package Manifest — snipclip_2.1.0_amd64.deb',
    x: 468,
    y: 344,
    width: 608,
    height: 238,
  },
];

function drawRoundedRect(
  ctx: CanvasRenderingContext2D,
  x: number,
  y: number,
  w: number,
  h: number,
  r: number
): void {
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.lineTo(x + w - r, y);
  ctx.quadraticCurveTo(x + w, y, x + w, y + r);
  ctx.lineTo(x + w, y + h - r);
  ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
  ctx.lineTo(x + r, y + h);
  ctx.quadraticCurveTo(x, y + h, x, y + h - r);
  ctx.lineTo(x, y + r);
  ctx.quadraticCurveTo(x, y, x + r, y);
  ctx.closePath();
}

function drawWindowChrome(
  ctx: CanvasRenderingContext2D,
  win: SimulatedWindow,
  darkBody: boolean
): void {
  ctx.save();
  ctx.shadowColor = 'rgba(15, 23, 42, 0.24)';
  ctx.shadowBlur = 18;
  ctx.shadowOffsetY = 8;

  drawRoundedRect(ctx, win.x, win.y, win.width, win.height, 10);
  ctx.fillStyle = darkBody ? '#0f172a' : '#ffffff';
  ctx.fill();
  ctx.restore();

  // Header bar
  ctx.save();
  drawRoundedRect(ctx, win.x, win.y, win.width, win.height, 10);
  ctx.clip();

  ctx.fillStyle = darkBody ? '#1e293b' : '#f1f5f9';
  ctx.fillRect(win.x, win.y, win.width, 34);

  ctx.strokeStyle = darkBody ? '#334155' : '#cbd5e1';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(win.x, win.y + 34);
  ctx.lineTo(win.x + win.width, win.y + 34);
  ctx.stroke();

  // Window buttons (minimize/maximize/close)
  const btnColors = ['#cbd5e1', '#cbd5e1', '#ef4444'];
  for (let i = 0; i < 3; i++) {
    ctx.beginPath();
    ctx.arc(win.x + win.width - 54 + i * 18, win.y + 17, 5, 0, Math.PI * 2);
    ctx.fillStyle = btnColors[i];
    ctx.fill();
  }

  ctx.fillStyle = darkBody ? '#e2e8f0' : '#0f172a';
  ctx.font = '600 12px "Plus Jakarta Sans", sans-serif';
  ctx.fillText(win.title, win.x + 14, win.y + 21);
  ctx.restore();

  ctx.save();
  drawRoundedRect(ctx, win.x, win.y, win.width, win.height, 10);
  ctx.strokeStyle = darkBody ? '#334155' : '#cbd5e1';
  ctx.lineWidth = 1;
  ctx.stroke();
  ctx.restore();
}

export function renderSimulatedDebianDesktop(
  version: string,
  protocol: DisplayProtocol,
  desktopName: string,
  captureDriver: string
): HTMLCanvasElement {
  const canvas = document.createElement('canvas');
  canvas.width = DESKTOP_WIDTH;
  canvas.height = DESKTOP_HEIGHT;
  const ctx = canvas.getContext('2d');
  if (!ctx) {
    return canvas;
  }

  // Subtle Debian 12 Bookworm geometric slate wallpaper
  const bg = ctx.createLinearGradient(0, 0, DESKTOP_WIDTH, DESKTOP_HEIGHT);
  bg.addColorStop(0, '#1e293b');
  bg.addColorStop(0.55, '#0f172a');
  bg.addColorStop(1, '#172554');
  ctx.fillStyle = bg;
  ctx.fillRect(0, 0, DESKTOP_WIDTH, DESKTOP_HEIGHT);

  // Subtle architectural grid lines
  ctx.strokeStyle = 'rgba(148, 163, 184, 0.07)';
  ctx.lineWidth = 1;
  for (let gx = 0; gx < DESKTOP_WIDTH; gx += 56) {
    ctx.beginPath();
    ctx.moveTo(gx, 0);
    ctx.lineTo(gx, DESKTOP_HEIGHT);
    ctx.stroke();
  }
  for (let gy = 0; gy < DESKTOP_HEIGHT; gy += 56) {
    ctx.beginPath();
    ctx.moveTo(0, gy);
    ctx.lineTo(DESKTOP_WIDTH, gy);
    ctx.stroke();
  }

  // Top Debian GNOME/XFCE system panel
  ctx.fillStyle = '#090d16';
  ctx.fillRect(0, 0, DESKTOP_WIDTH, 32);
  ctx.fillStyle = '#f8fafc';
  ctx.font = '600 12px "Plus Jakarta Sans", sans-serif';
  ctx.fillText('Activities   SnipClip', 16, 20);
  ctx.textAlign = 'center';
  ctx.fillText('Sun Nov 22  10:08', DESKTOP_WIDTH / 2, 20);
  ctx.textAlign = 'right';
  ctx.fillStyle = '#94a3b8';
  ctx.font = '500 11px "JetBrains Mono", monospace';
  ctx.fillText(`${protocol.toUpperCase()} · ${desktopName}`, DESKTOP_WIDTH - 16, 20);
  ctx.textAlign = 'left';

  // Window 2: Display & Compositor Inspector
  const winMonitor = SIMULATED_WINDOWS[1];
  drawWindowChrome(ctx, winMonitor, false);
  ctx.fillStyle = '#0f172a';
  ctx.font = '600 13px "Plus Jakarta Sans", sans-serif';
  ctx.fillText('Active Session Telemetry', winMonitor.x + 18, winMonitor.y + 62);

  const shortDriver =
    captureDriver.length > 25 ? `${captureDriver.slice(0, 24).trimEnd()}…` : captureDriver;
  const monitorRows = [
    ['Distribution', 'Debian GNU/Linux 12 (bookworm)'],
    ['Display Server', protocol.toUpperCase()],
    ['Desktop Env', desktopName],
    ['Capture Driver', shortDriver],
    ['Resolution', '1120 × 620 @ 60 Hz'],
  ];
  ctx.font = '500 12px "JetBrains Mono", monospace';
  monitorRows.forEach(([k, v], idx) => {
    const ry = winMonitor.y + 92 + idx * 30;
    ctx.fillStyle = '#64748b';
    ctx.fillText(k, winMonitor.x + 18, ry);
    ctx.fillStyle = '#0f172a';
    ctx.fillText(v, winMonitor.x + 148, ry);
  });

  // Window 3: Package Manifest
  const winNotes = {
    ...SIMULATED_WINDOWS[2],
    title: `Package Manifest — snipclip_${version}_amd64.deb`,
  };
  drawWindowChrome(ctx, winNotes, false);
  ctx.fillStyle = '#0f172a';
  ctx.font = '600 13px "Plus Jakarta Sans", sans-serif';
  ctx.fillText(`Native ELF Binary Package: snipclip_${version}_amd64.deb`, winNotes.x + 18, winNotes.y + 62);

  const manifestLines = [
    `Package: snipclip   Version: ${version}   Architecture: amd64`,
    'Depends: libc6 (>= 2.35), libgtk-3-0t64 | libgtk-3-0, libx11-6',
    'Recommends: xdg-desktop-portal, wl-clipboard, xclip, grim',
    'Binary: /usr/bin/snipclip (Compiled C11 + GTK 3 + Cairo ELF)',
    'Modes: Rectangle · Window · Fullscreen · Freeform + 0/3/5/10s Delay',
  ];
  ctx.font = '400 12px "JetBrains Mono", monospace';
  manifestLines.forEach((line, idx) => {
    ctx.fillStyle = idx === 0 ? '#1d4ed8' : '#334155';
    ctx.fillText(line, winNotes.x + 18, winNotes.y + 94 + idx * 27);
  });

  // Window 1 (Foreground): Graphical Software Center / Discover Window
  const winInstaller = {
    ...SIMULATED_WINDOWS[0],
    title: `Software Center (Discover) — snipclip_${version}_amd64.deb`,
  };
  drawWindowChrome(ctx, winInstaller, false);

  // App header card inside GUI installer
  ctx.fillStyle = '#eff6ff';
  ctx.fillRect(winInstaller.x + 16, winInstaller.y + 50, winInstaller.width - 32, 74);
  drawRoundedRect(ctx, winInstaller.x + 28, winInstaller.y + 63, 48, 48, 10);
  ctx.fillStyle = '#0f172a';
  ctx.fill();
  ctx.strokeStyle = '#38bdf8';
  ctx.lineWidth = 2;
  ctx.stroke();

  ctx.fillStyle = '#0f172a';
  ctx.font = '700 15px "Plus Jakarta Sans", sans-serif';
  ctx.fillText(`SnipClip v${version}`, winInstaller.x + 90, winInstaller.y + 82);
  ctx.fillStyle = '#475569';
  ctx.font = '500 12px "Plus Jakarta Sans", sans-serif';
  ctx.fillText('Installed via Double-Click · Zero CLI Required', winInstaller.x + 90, winInstaller.y + 104);

  // Graphical "Launch" button inside Software Center
  drawRoundedRect(ctx, winInstaller.x + winInstaller.width - 124, winInstaller.y + 70, 94, 34, 7);
  ctx.fillStyle = '#2563eb';
  ctx.fill();
  ctx.fillStyle = '#ffffff';
  ctx.font = '600 12px "Plus Jakarta Sans", sans-serif';
  ctx.fillText('Launch App', winInstaller.x + winInstaller.width - 110, winInstaller.y + 91);

  const guiDetails = [
    `Status: Installed (${version}) — Ready in Utilities & Graphics Menus`,
    `GUI Startup Auto-Detect: ${protocol.toUpperCase()} (${desktopName})`,
    `Active Capture Pipeline: ${captureDriver.slice(0, 44)}`,
    'Capture Modes: Rectangle · Window · Fullscreen · Freeform',
    'Shortcuts: PrintScreen · Custom Keybind · 0s / 3s / 5s / 10s Delay',
  ];

  ctx.font = '500 12px "JetBrains Mono", monospace';
  guiDetails.forEach((line, idx) => {
    ctx.fillStyle = idx === 0 ? '#15803d' : '#334155';
    ctx.fillText(line, winInstaller.x + 20, winInstaller.y + 156 + idx * 34);
  });

  return canvas;
}

export function extractSubCanvas(
  source: HTMLCanvasElement,
  x: number,
  y: number,
  width: number,
  height: number
): HTMLCanvasElement {
  const out = document.createElement('canvas');
  out.width = Math.max(1, Math.round(width));
  out.height = Math.max(1, Math.round(height));
  const ctx = out.getContext('2d');
  if (ctx) {
    ctx.drawImage(source, x, y, width, height, 0, 0, out.width, out.height);
  }
  return out;
}
