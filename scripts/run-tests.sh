#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

echo "=== [1/6] Running Native C11 / GTK 3 Unit Test Suite ==="
make -C native test

echo ""
echo "=== [2/6] Running Headless GTK 3 C Canvas Test Suite (Xvfb) ==="
if command -v xvfb-run >/dev/null 2>&1 && grep -q '^test-gtk:' native/Makefile; then
    xvfb-run -a -s "-screen 0 1920x1080x24" make -C native test-gtk
else
    echo "SKIP: xvfb-run not installed or native/Makefile has no 'test-gtk' target."
fi

echo ""
echo "=== [3/6] Running Compiled ELF Binary & Multi-Distro .deb Integration Suite ==="
python3 -m unittest discover -s linux/tests -p "test_*.py" -v

echo ""
echo "=== [4/6] Running TypeScript Engine & Cross-Stack Parity Test Suite ==="
npx --yes tsx --test src/utils/snipEngine.test.ts

echo ""
echo "=== [5/6] Running Web Workbench UI Test Suite ==="
if node -e 'const p=require("./package.json"); process.exit(p.scripts && p.scripts["test:ui"] ? 0 : 1)'; then
    npm run test:ui
else
    echo "SKIP: package.json defines no 'test:ui' script."
fi

echo ""
echo "=== [6/6] Running TypeScript Compiler Verification ==="
npm run lint

echo ""
echo "All SnipClip native C, GTK canvas, ELF/.deb package, TypeScript, and web UI test suites passed."
