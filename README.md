# Staff Runner (FM-1)

A compact C99 five-line staff rhythm game for the 240×240 M-VAVE FM-1. It has separate treble and bass clef modes, three short original note sequences, a fixed left hit line, and strict note checking: a wrong key plays an error cue while the note and game clock wait for the correct key.

## Controls

- Press **PLAY** on the title screen to choose a clef. Turn encoder 1 to switch between treble and bass, then press **PLAY**.
- Turn encoder 1 to choose one of three songs and press **PLAY** to start.
- Press the physical note key matching the note at the hit line. A correct pitch advances to the next note and sounds its tone; an incorrect pitch pauses at the hit line until corrected.
- Press **HOME** to return to song selection. Press **PLAY** after a song ends to choose another.

The score uses natural notes. FM-1 note bit 0 is F3 and bit 26 is G5; the clef-specific mapping is calculated from the standard bottom staff lines (E4 for treble, G2 for bass). The bass exercises place notes in the FM-1's playable register and draw ledger lines above the staff when needed.

## Build and FM-1 Open integration

This game targets the official [FM1 Open V14 SDK](https://github.com/m-vave/fm1_open), whose `fm1.h` exposes the panel, display, clock, and local MIDI APIs. Copy `src/staff_game.c`, `src/staff_game.h`, `src/staff_game_fm1.c`, and `src/staff_game_fm1.h` into the `app/` directory of an FM1 Open V14 source checkout. Its build script compiles C sources under `app/` automatically.

Add `#include "staff_game_fm1.h"` to `app/screen.c`. In `groove_screen_control()`, immediately after `fm1_controls_read(&controls)`, add `if (staff_game_fm1_control(&controls)) return;`. This lets the game take over the display and input while preserving the normal FM1 screen when it is inactive. Hold **HOME** and press **PLAY** to enter; hold **HOME** and press **REC** to leave. The adapter uses the official panel mapping (HOME 8, PLAY 12, REC 13), press-edge detection, RGB565 screen fills, and `fm1_midi_post()` for note audio.

Install JieLi Windows toolchain 2.5.3 as described in the FM1 Open README, then run `./sdk/Build.ps1 -FirmwareOnly -Toolchain 'C:\JL\pi32'` from the FM1 Open source root. Build and flash only after reviewing the firmware changes on your device. This repository does not flash hardware.

The original input reference was [FM1 Quest's `fm1_input.h`](https://github.com/hericdk/fm1-quest/blob/main/firmware/hal/fm1_input.h); the concrete integration now uses the official FM1 Open public SDK instead.

The compact 5×7 glyph table in `src/staff_game_fm1.c` is adapted from FM1 Open's `app/screen.c`; its source project is GPL-3.0 with the FM1 SDK linking exception. The game modules carry the same SPDX license identifier.
