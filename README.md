# Staff Runner (FM-1)

[English](README.md) | [简体中文](README.zh-CN.md)

A compact C99 five-line staff rhythm game for the 240×240 M-VAVE FM-1. It has separate treble and bass clef modes, three short original note sequences, and the MIDI song “Daughter's Love.” Choose Practice to wait for the right note, or Miss Mode to keep the music and notes moving while wrong or missed notes increase the MISS count. The MIDI song plays drums, intro, and bass as backing, with melody notes shown on the staff for the player.

<img width="737" height="476" alt="image" src="https://github.com/user-attachments/assets/95cdcd1b-7ba8-4cd5-8574-8454f9e68995" />


## Controls

- Press **PLAY** on the title screen to choose a clef. Turn encoder 1 to switch between treble and bass, then press **PLAY**.
- Choose **Practice** or **Miss Mode** with encoder 1 and press **PLAY**.
- Turn encoder 1 to choose a song and press **PLAY** to start.
- Press the physical note key matching the note at the hit line. Practice waits for the correct pitch. Miss Mode continues playback and records wrong or missed notes.
- A connected MIDI keyboard can also play note-on/note-off pitches F3–G5 on any MIDI channel. Press **PLAY** and use the emulator or FM-1 panel buttons to choose clef and song; MIDI notes control the gameplay pitches.
- Press **HOME** to return to song selection. Press **PLAY** after a song ends to choose another. Hold **HOME** and press **REC** to restart at the title screen.

### FM-1 browser emulator controls

- Click **Start** first. Click the virtual **PLAY** and **HOME** buttons on the FM-1 panel, or use **Space** for PLAY and **7** for HOME. **R** is REC; hold **7** and press **R** together to reset to the title screen.
- Use the mouse wheel over encoder 1, drag encoder 1, or press **← / →** to move through clef, mode, and song choices. **9 / ARP** goes back one choice and **0 / SEQ** goes forward one choice, so the menus also work from the emulator keyboard without the mouse.
- Piano keys can be played by clicking the virtual keys or using the emulator's computer-keyboard piano layout. Click the emulator panel once if computer shortcuts are not reaching it.

The score uses natural notes. FM-1 note bit 0 is F3 and bit 26 is G5; the clef-specific mapping is calculated from the standard bottom staff lines (E4 for treble, G2 for bass). The bass exercises place notes in the FM-1's playable register and draw ledger lines above the staff when needed.

## Build and FM-1 Open integration

This game targets the official [FM1 Open source](https://github.com/m-vave/fm1_open). `fm1.h` supplies the panel, display, clock and local MIDI APIs. The installer copies the game modules and MIDI chart data into `app/` and changes `app/screen.c` to launch the game directly. The original FM1 Open screen editor is removed from this game build to make room in flash; the FM1 audio engine, panel scanner, MIDI and validated SDK hooks remain.

Run the installer against an extracted FM1 Open source folder:

~~~powershell
.\install_fm1_game.ps1 'C:\path\to\fm1_open-main'
~~~

The game title appears on boot. Press **PLAY** to choose a clef, mode, then song, using encoder 1 to move through each menu. The adapter uses the official panel mapping (HOME 8, PLAY 12, REC 13), press-edge detection, 16-row asynchronous RGB565 display updates, and local MIDI playback for song and note audio.

Install JieLi Windows toolchain 2.5.3 as described in the FM1 Open README, then run `./sdk/Build.ps1 -FirmwareOnly -Toolchain 'C:\JL\pi32'` from the FM1 Open source root. This repository does not flash hardware.

The original input reference was [FM1 Quest's `fm1_input.h`](https://github.com/hericdk/fm1-quest/blob/main/firmware/hal/fm1_input.h); the concrete integration now uses the official FM1 Open public SDK instead.

The compact 5×7 glyph table in `src/staff_game_fm1.c` is adapted from FM1 Open's `app/screen.c`; its source project is GPL-3.0 with the FM1 SDK linking exception. The game modules carry the same SPDX license identifier.
