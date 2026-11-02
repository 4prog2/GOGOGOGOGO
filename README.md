# Staff Runner (FM-1)

A compact C99 five-line staff rhythm game for the 240×240 M-VAVE FM-1. It has separate treble and bass clef modes, three short original note sequences, a fixed left hit line, and strict note checking: a wrong key plays an error cue while the note and game clock wait for the correct key.

## Controls

- Press **PLAY** on the title screen to choose a clef. Turn encoder 1 to switch between treble and bass, then press **PLAY**.
- Turn encoder 1 to choose one of three songs and press **PLAY** to start.
- Press the physical note key matching the note at the hit line. A correct pitch advances to the next note and sounds its tone; an incorrect pitch pauses at the hit line until corrected.
- Press **HOME** to return to song selection. Press **PLAY** after a song ends to choose another.

The score uses natural notes. FM-1 note bit 0 is F3 and bit 26 is G5; the clef-specific mapping is calculated from the standard bottom staff lines (E4 for treble, G2 for bass). The bass exercises place notes in the FM-1's playable register and draw ledger lines above the staff when needed.

## Build and integration

The repository starts empty and does not include an FM-1 firmware SDK or an existing firmware base, so it cannot produce a flashable image on its own. `src/staff_game.c` is platform-independent game logic. Implement its `StaffPlatform` callbacks with the target firmware's 240×240 display and audio primitives, and call `staff_game_update()` once per frame. See [`src/fm1_adapter.h`](src/fm1_adapter.h) for the FM-1 key bitmap and input edge conventions.

The input reference is [FM1 Quest's `fm1_input.h`](https://github.com/hericdk/fm1-quest/blob/main/firmware/hal/fm1_input.h). The game does not copy its art, characters, or game code. Compile the module with the FM-1 project's JieLi toolchain after adding it to that project. No hardware is flashed here.
