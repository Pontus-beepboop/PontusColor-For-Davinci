#!/bin/bash
# ============================================================================
#  PontusColor - macOS build & install  (double-click this file in Finder)
#  Produces a universal (Apple Silicon + Intel) PontusColor.ofx.bundle
# ============================================================================
set -e
cd "$(dirname "$0")"

echo "==> Checking tools"
if ! command -v cmake >/dev/null 2>&1; then
    echo "CMake not found. Install it with:  brew install cmake"
    echo "(or download from https://cmake.org/download/ )"
    exit 1
fi
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode command line tools not found. Run:  xcode-select --install"
    exit 1
fi

echo "==> Configuring"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"

echo "==> Building"
cmake --build build --config Release -j

BUNDLE="build/PontusColor.ofx.bundle"
echo "==> Built: $BUNDLE"

read -p "Install to /Library/OFX/Plugins now? (asks for your password) [y/N] " ans
if [[ "$ans" == "y" || "$ans" == "Y" ]]; then
    sudo mkdir -p /Library/OFX/Plugins
    sudo rm -rf "/Library/OFX/Plugins/PontusColor.ofx.bundle"
    sudo cp -R "$BUNDLE" /Library/OFX/Plugins/
    # clear the quarantine flag so Resolve will load it
    sudo xattr -dr com.apple.quarantine "/Library/OFX/Plugins/PontusColor.ofx.bundle" 2>/dev/null || true
    echo "==> Installed. Restart DaVinci Resolve, then find PontusColor in"
    echo "    the Color page -> Effects (OpenFX) -> PontusColor."
else
    echo "Skipped install. To install manually, copy $BUNDLE into"
    echo "/Library/OFX/Plugins/ and restart DaVinci Resolve."
fi
