#!/bin/bash
set -e

BASEDIR="$(cd "$(dirname "$0")" && pwd)"
VERSIONLOG="$BASEDIR/Progress-Reports/Version-Log"
SIMDIR="$BASEDIR/simulator"
HEADLESS_DIR="/tmp/headless_patches"
BUILDLOG="/tmp/sim_build.log"

# Temp dir to hold our headless patch files
mkdir -p "$HEADLESS_DIR"

save_headless_patches() {
    cp "$SIMDIR/SDL_Driver.c" "$HEADLESS_DIR/SDL_Driver.c"
    cp "$SIMDIR/SDL_Driver.h" "$HEADLESS_DIR/SDL_Driver.h"
    cp "$SIMDIR/main.c" "$HEADLESS_DIR/main.c"
}

apply_headless_patches() {
    cp "$HEADLESS_DIR/SDL_Driver.c" "$SIMDIR/SDL_Driver.c"
    cp "$HEADLESS_DIR/SDL_Driver.h" "$SIMDIR/SDL_Driver.h"
    cp "$HEADLESS_DIR/main.c" "$SIMDIR/main.c"
}

build_sim() {
    cd "$SIMDIR"
    rm -rf CMakeCache.txt CMakeFiles Makefile build
    cmake -B build -S . > /dev/null 2>&1
    cmake --build build -j4 > "$BUILDLOG" 2>&1
    if [ $? -ne 0 ]; then
        echo "BUILD FAILED at $(git -C "$BASEDIR" rev-parse --short HEAD)"
        tail -20 "$BUILDLOG"
        return 1
    fi
    echo "Build OK"
}

run_sim() {
    local script="$1"
    local timeout_sec="${2:-15}"
    rm -f /tmp/studybud_*.bmp
    cd "$SIMDIR"
    timeout "$timeout_sec" ./build/studybud_sim --headless --script="$script" > /dev/null 2>&1
    # Collect all BMPs produced
    for f in /tmp/studybud_*.bmp; do
        [ -f "$f" ] && echo "$f"
    done
}

bmp_to_png() {
    local bmp="$1"
    local png="${bmp%.bmp}.png"
    sips -s format png "$bmp" --out "$png" > /dev/null 2>&1
    rm -f "$bmp"
    echo "$png"
}

copy_screenshots() {
    local destdir="$1"
    shift
    mkdir -p "$destdir"
    for bmp in "$@"; do
        local png
        png=$(bmp_to_png "$bmp")
        cp "$png" "$destdir/"
        rm -f "$png"
        echo "  -> $(basename "$png")"
    done
}

# ── Save the headless patches from current HEAD ──
save_headless_patches

echo "=== Capturing version log screenshots ==="
echo

# ── Milestone 2: 54aa33c - Todo + Periwinkle theme + simulator debut ──
echo ">>> Milestone 2: 54aa33c (Todo + Periwinkle theme)"
git -C "$BASEDIR" checkout 54aa33c 2>/dev/null
apply_headless_patches
build_sim

FILES=()
# Home screen
FILES+=($(run_sim "s" 10))
# Menu screen (long press from home)
FILES+=($(run_sim "hws" 10))
# Todos (from menu: rotate right 2x to Todos, press)
FILES+=($(run_sim "hwrrps" 10))

copy_screenshots "$VERSIONLOG/02-todo-periwinkle-theme" "${FILES[@]}"
echo

# ── Milestone 3: e27ac4c - Breathing app ──
echo ">>> Milestone 3: e27ac4c (Breathing app)"
git -C "$BASEDIR" checkout e27ac4c 2>/dev/null
apply_headless_patches
build_sim

FILES=()
FILES+=($(run_sim "s" 10))
FILES+=($(run_sim "hws" 10))
# Breathing (from menu: rotate right 4x to Breathing, press)
FILES+=($(run_sim "hwrrrrps" 10))

copy_screenshots "$VERSIONLOG/03-breathing-app" "${FILES[@]}"
echo

# ── Milestone 4: 093e68f - Idle background + original art ──
echo ">>> Milestone 4: 093e68f (Idle background)"
git -C "$BASEDIR" checkout 093e68f 2>/dev/null
apply_headless_patches
build_sim

FILES=()
FILES+=($(run_sim "s" 10))
FILES+=($(run_sim "hws" 10))
# Backgrounds (from menu: rotate right 5x to Backgrounds, press)
FILES+=($(run_sim "hwrrrrrps" 10))

copy_screenshots "$VERSIONLOG/04-idle-background" "${FILES[@]}"
echo

# ── Milestone 5: b23bf63 - Sleep tracker ──
echo ">>> Milestone 5: b23bf63 (Sleep tracker)"
git -C "$BASEDIR" checkout b23bf63 2>/dev/null
apply_headless_patches
build_sim

FILES=()
FILES+=($(run_sim "s" 10))
FILES+=($(run_sim "hws" 10))
# Sleep (from menu: rotate right 6x to Sleep, press)
FILES+=($(run_sim "hwrrrrrrps" 10))

copy_screenshots "$VERSIONLOG/05-sleep-tracker" "${FILES[@]}"
echo

# ── Milestone 6: 9dbab40 - New logos in LVLGL ──
echo ">>> Milestone 6: 9dbab40 (New logos)"
git -C "$BASEDIR" checkout 9dbab40 2>/dev/null
apply_headless_patches
build_sim

FILES=()
FILES+=($(run_sim "s" 10))
# Menu with logos
FILES+=($(run_sim "hws" 10))

copy_screenshots "$VERSIONLOG/06-new-logos" "${FILES[@]}"
echo

# ── Milestone 7: 5bf5a79 - Timer app ──
echo ">>> Milestone 7: 5bf5a79 (Timer app)"
git -C "$BASEDIR" checkout 5bf5a79 2>/dev/null
apply_headless_patches
build_sim

FILES=()
FILES+=($(run_sim "s" 10))
FILES+=($(run_sim "hws" 10))
# Timer (from menu: rotate right 1x to Timer, press)
FILES+=($(run_sim "hwps" 10))

copy_screenshots "$VERSIONLOG/07-timer-app" "${FILES[@]}"
echo

# ── Milestone 10: Latest HEAD - Final state ──
echo ">>> Milestone 10: Latest (all screens)"
git -C "$BASEDIR" checkout fix_timer_LVLGL 2>/dev/null
# Re-save headless patches from latest
save_headless_patches
apply_headless_patches
build_sim

FILES=()
# Home
FILES+=($(run_sim "s" 10))
# Menu
FILES+=($(run_sim "hws" 10))
# Tamagotchi (menu item 1)
FILES+=($(run_sim "hwrps" 10))
# Breathing (menu item 2)
FILES+=($(run_sim "hwrrps" 10))
# Water (menu item 3)
FILES+=($(run_sim "hwrrrps" 10))
# Stretch Break (menu item 4)
FILES+=($(run_sim "hwrrrrps" 10))
# Sleep (menu item 5)
FILES+=($(run_sim "hwrrrrrps" 10))
# Timer (menu item 6)
FILES+=($(run_sim "hwrrrrrrps" 10))
# Todos (menu item 7)
FILES+=($(run_sim "hwrrrrrrrps" 10))
# Backgrounds (menu item 8)
FILES+=($(run_sim "hwrrrrrrrrps" 10))

copy_screenshots "$VERSIONLOG/10-latest-final-state" "${FILES[@]}"
echo

# ── Return to working branch ──
echo ">>> Returning to fix_timer_LVLGL"
git -C "$BASEDIR" checkout fix_timer_LVLGL 2>/dev/null

# Clean up temp files
rm -rf "$HEADLESS_DIR"

echo
echo "=== Done! Screenshots saved to $VERSIONLOG ==="
