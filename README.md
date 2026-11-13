# KEYLINE — GUI Typing Tutor

A Windows desktop **typing tutor written in C99 with raylib 5.5** for a Programming Fundamentals group project. This repository contains the editable application code, core and GUI regression tests, and the Windows build workflow—not a precompiled release.

## Features

- Real-time typing feedback, speed (WPM), accuracy, mistakes and configurable timers
- Separate local profiles, saved performance history and leaderboard rankings
- Achievements, settings, and a multi-screen graphical interface
- Native Windows window controls and profile input support

## Build on Windows (recommended)

1. Install **MSYS2 UCRT64 GCC (64-bit)**. In the MSYS2 UCRT64 terminal run:
   ```sh
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc
   ```
2. Add `C:\msys64\ucrt64\bin` to your Windows PATH, reopen PowerShell, and confirm that `gcc --version` works.
3. From the repository root run:
   ```powershell
   .\build.bat
   .\run.bat --data-dir build/student-demo
   ```
   `run.bat` rebuilds from the current source before launching. Close the running program before rebuilding. Generated binaries and test data go into `build/` (ignored by Git).

This repository includes a minimal **Windows/MinGW raylib 5.5 static library and header**, plus raylib's original license, in `vendor/` to make the primary build possible without downloading raylib. A compatible Windows compiler is still required. Optional CMake support is in `CMakeLists.txt`; see `BUILD_DEPENDENCIES.txt` for dependency details.

## Tests

Run the non-GUI core tests on Windows:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -TestOnly
```

Run Windows graphical checks (requires a working desktop and OpenGL):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify-gui.ps1
```

The **GitHub Actions workflow** runs just the platform-independent core tests on Linux. Passing it does **not** verify native Windows GUI interaction or DPI behavior. Test the actual application on the presentation laptop.

## Project structure

| Path | Purpose |
|---|---|
| `src/` | The C99 typing tutor implementation and headers |
| `tests/` | Core, GUI and window/input regression tests |
| `scripts/`, `build.bat`, `run.bat` | Windows building, launching and checks |
| `vendor/` | Minimal raylib 5.5 Windows build dependency and its license |
| `learning/trace_typing.c` | Small source-based teaching trace |
| `CMakeLists.txt` | Optional CMake build |
| `TEAM_WORKLOG.md` | Template for recording genuine contributions and tests |

## Development and attribution

The project was developed with **AI coding assistance (Codex)**. Team members should inspect, test, understand and independently modify the implementation, and should accurately record their own changes in the work log and Git commit history. Do not fabricate historical commits or misrepresent automatically generated work as independently written. Follow your instructor's rules for permitted AI assistance.

**This repository alone is not proof of who wrote each line.** An authentic history of commits, pull requests, bug fixes, screenshots of tested runs, and the ability to explain code are stronger evidence of involvement.

The raylib library has its own included zlib license (`vendor/raylib-5.5_win64_mingw-w64/LICENSE`). No separate license for the group's application source was included in the supplied package; the team should agree on licensing before presenting it as an open-source release.
