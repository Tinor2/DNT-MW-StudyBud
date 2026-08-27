# Focus Friend — Getting Started on a New Machine

This guide covers how to run the two software components of Focus Friend from a fresh clone of the repository: the web companion app and the LVGL simulator.

It is written for a **Windows machine with no programming tools pre-installed**. macOS and Linux equivalents are noted where applicable.

---

# Part 1: Running the Web App

The web companion is a Svelte 4 single-page application served by Vite. It connects to the ESP32 device via WebSocket but can also run in standalone demo mode without any hardware.

## Installing Node.js

The web app only requires **Node.js** (which includes npm).

1. Download the **LTS** installer from https://nodejs.org/ (version 20 or later).
2. Run the installer with default settings.
3. Verify by opening a new terminal:

```
node --version
npm --version
```

No other tools are needed for the web app.

## Step 1 — Clone the repository

Open a terminal (Git Bash, PowerShell, or Command Prompt with `git` on the PATH):

```bash
git clone https://github.com/Tinor2/DNT-MW-StudyBud.git
cd DNT-MW-StudyBud
```

> If Git is not installed, download it from https://git-scm.com/download/win and run the installer with default settings.

## Step 2 — Navigate to the web app directory

```bash
cd web-app
```

## Step 3 — Install dependencies

```bash
npm install
```

This installs three dev dependencies — Svelte, Vite, and the Svelte Vite plugin. There are zero runtime dependencies.

## Step 4 — Start the development server

```bash
npm run dev
```

Vite starts on **http://localhost:5173** with hot module replacement enabled.

## Step 5 — Open in browser

Navigate to **http://localhost:5173**.

The app loads in disconnected state by default. It is fully navigable in demo mode — all tabs render with seeded demo data stored in `localStorage`.

## Connecting to the Physical Device

If the ESP32 is on the same network, the Vite dev server proxies WebSocket and API requests to it automatically.

Set the device IP by creating a `.env` file in the `web-app/` directory:

```bash
echo ESP32_IP=192.168.1.XXX > .env
```

If no `.env` file is created, the default IP `192.168.0.200` is used.

## URL Parameters

| Parameter | Effect |
|-----------|--------|
| `?debug` | Enables console logging of all WebSocket messages |
| `?demo` | Auto-connects to the device on page load |
| `?reset` | Clears all localStorage-seeded demo data |

Example: `http://localhost:5173?debug`

## Build for Production

```bash
npm run build
```

Outputs optimised static assets to `dist/`. These can be served from the ESP32's TF card as a PWA.

## Preview Production Build

```bash
npm run preview
```

Serves the `dist/` directory locally for testing the production build.

## Web App Troubleshooting

- **Port 5173 already in use** — Either kill the occupying process or start on a different port: `npx vite --port 3000`.
- **"node_modules" missing** — Run `npm install` before starting the dev server.
- **Device not connecting** — Confirm the ESP32 and your machine are on the same Wi-Fi network, and that `ESP32_IP` in `.env` matches the device's IP address.

---

# Part 2: Running the LVGL Simulator

The simulator reuses the same LVGL screen code as the physical ESP32 device but renders to an SDL2 window on your desktop. It requires additional build tools beyond Node.js.

## Installing the Required Tools

### Git

Download and install from https://git-scm.com/download/win.

Use the default settings during installation. This provides `git` from the command line and a Git Bash terminal.

### C Compiler (MinGW-w64)

The simulator needs a C compiler. The simplest option on Windows is MSYS2.

1. Download MSYS2 from https://www.msys2.org/ and install it.
2. Open the **MSYS2 UCRT64** terminal and run:
   ```
   pacman -S mingw-w64-ucrt-x86_64-gcc
   ```
3. Add `C:\msys64\ucrt64\bin` to your system **PATH**.
4. Verify by opening a new terminal and running:
   ```
   gcc --version
   ```

### CMake

Download the Windows installer from https://cmake.org/download/.

During installation, select **"Add CMake to the system PATH"** so it is available from any terminal.

Verify:
```
cmake --version
```

### SDL2

Download the Visual C++ development library from https://libsdl.org/download-2.0.php (choose **SDL2-devel-2.x.x-VC.zip**).

Extract the zip to a known location, for example:

```
C:\SDL2-2.x.x
```

This folder should contain `include/`, `lib/`, and `cmake/` subdirectories.

## Step 1 — Clone the repository

```bash
git clone https://github.com/Tinor2/DNT-MW-StudyBud.git
cd DNT-MW-StudyBud
```

## Step 2 — Generate the build system

```bash
cd simulator
cmake -B build -DSDL2_DIR="C:/SDL2-2.x.x/cmake"
```

Replace `C:/SDL2-2.x.x` with the actual path where you extracted SDL2.

If CMake cannot find SDL2 automatically, you can also set the environment variable before running cmake:

```bash
set SDL2_DIR=C:\SDL2-2.x.x\cmake
cmake -B build
```

This reads `simulator/CMakeLists.txt`, locates SDL2, globs the vendored LVGL source tree, and produces a build system inside `simulator/build/`.

## Step 3 — Build

**MinGW / MSYS2:**

```bash
cmake --build build
```

**Visual Studio Build Tools:**

```bash
cmake --build build --config Release
```

This compiles four targets:
- `lvgl` — static library from the vendored LVGL v8 source
- `studybud_screens` — shared screen code from `main/display/` with simulator persistence stubs
- `sdl_driver` — SDL2 display driver and keyboard-based encoder stub
- `studybud_sim` — final executable linking everything together

## Step 4 — Run

**MinGW / MSYS2:**
```bash
./build/studybud_sim.exe
```

**Visual Studio:**
```bash
./build/Release/studybud_sim.exe
```

A 480×480 window appears displaying the Focus Friend interface.

## Keyboard Controls

| Key | Action |
|-----|--------|
| Left Arrow | Rotate encoder counter-clockwise (scroll up) |
| Right Arrow | Rotate encoder clockwise (scroll down) |
| Enter | Encoder press (select/confirm) |
| Escape | Quit |
| Q | Quit |
| S | Save screenshot to a BMP file |

## Headless / Scripted Mode

The simulator supports scripted input sequences and headless execution (no window):

```bash
./build/studybud_sim.exe --headless
./build/studybud_sim.exe --script="hrrrrrrrrpws"
./build/studybud_sim.exe --headless --script="hrrrrrrrrpws"
```

Script characters: `r` = rotate CW, `l` = rotate CCW, `p` = press, `h` = long press, `s` = screenshot, `w` = wait 1500ms.

## Simulator Troubleshooting

- **"SDL2 not found"** — Make sure the `-DSDL2_DIR` path points to the `cmake/` subfolder inside your SDL2 extract. The path must use forward slashes or escaped backslashes.
- **"gcc not found"** — Ensure MinGW's `bin/` directory is on your system PATH, or use the MSYS2 UCRT64 terminal.
- **Build fails on first clone** — The `build/` directory may contain stale paths. Delete it and regenerate: `rmdir /s build && cmake -B build`.
- **SDL2 DLL not found at runtime** — Copy `SDL2.dll` from `SDL2-2.x.x/lib/x64/` into the same folder as `studybud_sim.exe`, or add that `lib/x64/` folder to your system PATH.

---

# Running Both Simultaneously

To develop with both the web app and simulator running at the same time:

**Terminal 1 — Web App:**
```bash
cd web-app && npm run dev
```

**Terminal 2 — Simulator:**
```bash
cd simulator && cmake --build build && ./build/studybud_sim.exe
```

The web app and simulator operate independently — they do not communicate with each other. Both share the same screen codebase from `main/display/`, so UI changes are visible in both when rebuilt.

---

# Quick Reference — What to Install

| Tool | Purpose | Required For | Download |
|------|---------|-------------|----------|
| Node.js (v20 LTS) | JavaScript runtime and npm | Web app | https://nodejs.org/ |
| Git | Clone the repository | Both | https://git-scm.com/download/win |
| MSYS2 + MinGW-w64 | C compiler | Simulator | https://www.msys2.org/ |
| CMake | Build system generator | Simulator | https://cmake.org/download/ |
| SDL2 | Display library | Simulator | https://libsdl.org/download-2.0.php |
