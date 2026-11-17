#!/bin/bash
# Builds PrusaSlicer with the Expanded Metal infill on this Mac and puts
# "PrusaSlicer ExpandedMetal.app" in ~/Applications.
# Safe to re-run: it picks up where it left off and reuses the built libraries.
set -euo pipefail

SRC="$HOME/src/PrusaSlicer-ExpandedMetal"
BRANCH="expanded-metal"
CMAKE_VER="3.27.9"
OSX_TARGET="$(sw_vers -productVersion | cut -d. -f1).0"
JOBS="$(sysctl -n hw.ncpu)"
APP="$HOME/Applications/PrusaSlicer ExpandedMetal.app"

say() { printf '\n\033[1;35m==> %s\033[0m\n' "$*"; }

# 1. Apple command line tools
if ! xcode-select -p >/dev/null 2>&1; then
    say "Installing Apple command line tools. Finish the popup, then run this script again."
    xcode-select --install || true
    exit 1
fi

# 2. Homebrew and build tools
if ! command -v brew >/dev/null 2>&1; then
    for b in /opt/homebrew/bin/brew /usr/local/bin/brew; do
        [ -x "$b" ] && eval "$("$b" shellenv)" && break
    done
fi
if ! command -v brew >/dev/null 2>&1; then
    say "Homebrew not found. Install it from https://brew.sh then run this script again."
    exit 1
fi
say "Installing build tools with Homebrew"
brew install automake gettext libtool texinfo m4 git >/dev/null
export PATH="$(brew --prefix m4)/bin:$PATH"

# 3. CMake 3.27 (newer versions break PrusaSlicer's dependency build), kept local
mkdir -p "$HOME/src"
CMAKE_DIR="$HOME/src/cmake-$CMAKE_VER-macos-universal"
if [ ! -x "$CMAKE_DIR/CMake.app/Contents/bin/cmake" ]; then
    say "Downloading CMake $CMAKE_VER"
    curl -fsSL "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VER/cmake-$CMAKE_VER-macos-universal.tar.gz" \
        | tar xz -C "$HOME/src"
fi
export PATH="$CMAKE_DIR/CMake.app/Contents/bin:$PATH"

# 4. Source
if [ ! -d "$SRC/.git" ]; then
    say "Downloading source"
    git clone --depth 1 --branch "$BRANCH" https://github.com/siddely/PrusaSlicer.git "$SRC"
else
    say "Updating source"
    git -C "$SRC" pull --ff-only
fi

# 5. Libraries (the long part, about 45 to 60 min the first time)
if [ ! -f "$SRC/deps/build/.done" ]; then
    say "Building libraries (first time only, go make a coffee)"
    mkdir -p "$SRC/deps/build" && cd "$SRC/deps/build"
    cmake .. -DCMAKE_OSX_DEPLOYMENT_TARGET="$OSX_TARGET"
    make -j"$JOBS"
    touch .done
fi

# 6. PrusaSlicer itself
say "Building PrusaSlicer"
mkdir -p "$SRC/build" && cd "$SRC/build"
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_OSX_DEPLOYMENT_TARGET="$OSX_TARGET" \
         -DCMAKE_PREFIX_PATH="$SRC/deps/build/destdir/usr/local" \
         -DSLIC3R_BUILD_TESTS=OFF
make -j"$JOBS" PrusaSlicer

# 7. App bundle
say "Making the app"
mkdir -p "$HOME/Applications"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$SRC/build/src/PrusaSlicer" "$APP/Contents/MacOS/PrusaSlicer"
cp "$SRC/build/src/Info.plist" "$APP/Contents/Info.plist"
cp -R "$SRC/resources/" "$APP/Contents/Resources/"
cp "$SRC"/resources/icons/*.icns "$APP/Contents/Resources/"
codesign --force --deep --sign - "$APP"

say "Done: $APP"
open "$APP"
