export interface DebReleasePayload {
  packageName: string;
  version: string;
  releaseTag: string;
  architecture: string;
  debFilename: string;
  downloadPath: string;
  sha256: string;
  sha256sumLine?: string;
  sizeBytes: number;
  installedSizeKb: number;
  builtAtIso: string;
  depends: string;
  recommends: string;
  debBase64?: string;
}

const AR_ARCHIVE_MAGIC = [0x21, 0x3c, 0x61, 0x72, 0x63, 0x68, 0x3e, 0x0a]; // "!<arch>\n"

export function decodeBase64ToBytes(base64: string): Uint8Array {
  const clean = base64.replace(/\s+/g, '');
  if (typeof globalThis.atob === 'function') {
    const binaryStr = globalThis.atob(clean);
    const out = new Uint8Array(binaryStr.length);
    for (let i = 0; i < binaryStr.length; i++) {
      out[i] = binaryStr.charCodeAt(i);
    }
    return out;
  }
  // Node.js test runner fallback
  const buf = Buffer.from(clean, 'base64');
  return new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength);
}

export function isValidDebianArchiveBytes(bytes: Uint8Array): boolean {
  if (bytes.byteLength < 72) {
    return false;
  }
  for (let i = 0; i < AR_ARCHIVE_MAGIC.length; i++) {
    if (bytes[i] !== AR_ARCHIVE_MAGIC[i]) {
      return false;
    }
  }
  // First ar member in a Debian v2.0 binary package must start with "debian-binary" at offset 8
  const memberHeader = new TextDecoder('ascii').decode(bytes.subarray(8, 21));
  return memberHeader === 'debian-binary';
}

function triggerBrowserBlobDownload(blob: Blob, filename: string): void {
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = filename;
  anchor.rel = 'noopener';
  document.body.appendChild(anchor);
  anchor.click();
  document.body.removeChild(anchor);
  window.setTimeout(() => URL.revokeObjectURL(url), 10000);
}

export async function downloadVerifiedDebPackage(release: DebReleasePayload): Promise<void> {
  if (release.debBase64) {
    const decoded = decodeBase64ToBytes(release.debBase64);
    if (isValidDebianArchiveBytes(decoded)) {
      const arrayBuf = decoded.buffer.slice(
        decoded.byteOffset,
        decoded.byteOffset + decoded.byteLength
      ) as ArrayBuffer;
      const blob = new Blob([arrayBuf], { type: 'application/vnd.debian.binary-package' });
      triggerBrowserBlobDownload(blob, release.debFilename);
      return;
    }
  }

  const response = await fetch(release.downloadPath, { credentials: 'same-origin' });
  if (!response.ok) {
    throw new Error(`Failed to fetch ${release.debFilename} (${response.status})`);
  }
  const arrayBuffer = await response.arrayBuffer();
  const bytes = new Uint8Array(arrayBuffer);
  if (!isValidDebianArchiveBytes(bytes)) {
    throw new Error('Downloaded payload did not match Debian binary archive signature.');
  }
  const blob = new Blob([arrayBuffer], { type: 'application/vnd.debian.binary-package' });
  triggerBrowserBlobDownload(blob, release.debFilename);
}

export function downloadVerifiedSha256Sums(release: DebReleasePayload): void {
  const line = release.sha256sumLine || `${release.sha256}  ${release.debFilename}`;
  const blob = new Blob([`${line}\n`], { type: 'text/plain;charset=utf-8' });
  triggerBrowserBlobDownload(blob, 'SHA256SUMS');
}
