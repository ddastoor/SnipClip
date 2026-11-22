#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

echo "=== [1/3] Running Linux Python & Debian Package Test Suite ==="
python3 -m unittest discover -s linux/tests -p "test_*.py" -v

echo ""
echo "=== [2/3] Running TypeScript Engine & Cross-Stack Parity Test Suite ==="
npx --yes tsx --test src/utils/snipEngine.test.ts

echo ""
echo "=== [3/3] Running TypeScript Compiler Verification ==="
npm run lint

echo ""
echo "All SnipClip test suites passed."
