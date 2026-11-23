#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

echo "=== [1/4] Running Native C11 / GTK 3 Unit Test Suite ==="
make -C native test

echo ""
echo "=== [2/4] Running Compiled ELF Binary & Multi-Distro .deb Integration Suite ==="
python3 -m unittest discover -s linux/tests -p "test_*.py" -v

echo ""
echo "=== [3/4] Running TypeScript Engine & Cross-Stack Parity Test Suite ==="
npx --yes tsx --test src/utils/snipEngine.test.ts

echo ""
echo "=== [4/4] Running TypeScript Compiler Verification ==="
npm run lint

echo ""
echo "All SnipClip native C, ELF/.deb package, and TypeScript test suites passed."
