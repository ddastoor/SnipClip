import { cleanup } from '@testing-library/react';
import { afterEach, beforeEach, expect, vi, type MockInstance } from 'vitest';

/**
 * jsdom ships no 2D canvas implementation (getContext() returns null and logs a
 * "Not implemented" error). SnipStudio and desktopScene draw heavily onto canvases, so install a
 * deterministic, side-effect-free CanvasRenderingContext2D stand-in: every drawing method is a
 * no-op, style properties are stored, and the few value-returning APIs return sane shapes.
 */
type CanvasStub = Record<string | symbol, unknown>;

function createContext2DStub(canvas: HTMLCanvasElement): CanvasRenderingContext2D {
  const state: CanvasStub = {
    canvas,
    fillStyle: '#000000',
    strokeStyle: '#000000',
    lineWidth: 1,
    globalAlpha: 1,
    font: '10px sans-serif',
    textAlign: 'start',
    textBaseline: 'alphabetic',
    lineCap: 'butt',
    lineJoin: 'miter',
    imageSmoothingEnabled: true,
    measureText: (text: string) => ({ width: String(text).length * 7 }),
    createLinearGradient: () => ({ addColorStop: () => undefined }),
    createRadialGradient: () => ({ addColorStop: () => undefined }),
    createPattern: () => null,
    getImageData: (_x: number, _y: number, w: number, h: number) => ({
      width: w,
      height: h,
      data: new Uint8ClampedArray(Math.max(0, w * h * 4)),
    }),
    getLineDash: () => [],
    isPointInPath: () => false,
    isPointInStroke: () => false,
  };
  const noop = () => undefined;
  const proxy = new Proxy(state, {
    get(target, prop) {
      if (prop in target) return target[prop];
      return noop;
    },
    set(target, prop, value) {
      target[prop] = value;
      return true;
    },
  });
  return proxy as unknown as CanvasRenderingContext2D;
}

const contexts = new WeakMap<HTMLCanvasElement, CanvasRenderingContext2D>();

Object.defineProperty(HTMLCanvasElement.prototype, 'getContext', {
  configurable: true,
  writable: true,
  value: function getContext(this: HTMLCanvasElement, kind: string) {
    if (kind !== '2d') return null;
    let ctx = contexts.get(this);
    if (!ctx) {
      ctx = createContext2DStub(this);
      contexts.set(this, ctx);
    }
    return ctx;
  },
});

Object.defineProperty(HTMLCanvasElement.prototype, 'toDataURL', {
  configurable: true,
  writable: true,
  value: function toDataURL(type?: string) {
    return `data:${type ?? 'image/png'};base64,AAAA`;
  },
});

Object.defineProperty(HTMLCanvasElement.prototype, 'toBlob', {
  configurable: true,
  writable: true,
  value: function toBlob(callback: BlobCallback, type?: string) {
    callback(new Blob([new Uint8Array([0x89, 0x50, 0x4e, 0x47])], { type: type ?? 'image/png' }));
  },
});

// Pointer capture is not implemented by jsdom; the editor/overlay call it on pointerdown.
for (const method of ['setPointerCapture', 'releasePointerCapture'] as const) {
  if (typeof Element.prototype[method] !== 'function') {
    Object.defineProperty(Element.prototype, method, {
      configurable: true,
      writable: true,
      value: () => undefined,
    });
  }
}
if (typeof Element.prototype.hasPointerCapture !== 'function') {
  Object.defineProperty(Element.prototype, 'hasPointerCapture', {
    configurable: true,
    writable: true,
    value: () => false,
  });
}

// Fail any test that triggers a console.error (React warnings, jsdom "Not implemented", etc.).
let consoleErrorSpy: MockInstance<typeof console.error>;

beforeEach(() => {
  window.localStorage.clear();
  consoleErrorSpy = vi.spyOn(console, 'error');
});

afterEach(() => {
  cleanup();
  const calls = consoleErrorSpy.mock.calls;
  consoleErrorSpy.mockRestore();
  vi.restoreAllMocks();
  vi.unstubAllGlobals();
  expect(calls, `console.error was called:\n${calls.map((c) => c.map(String).join(' ')).join('\n')}`).toEqual([]);
});
