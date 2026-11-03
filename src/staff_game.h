/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef STAFF_GAME_H
#define STAFF_GAME_H

#include <stdint.h>

typedef enum { STAFF_TREBLE = 0, STAFF_BASS = 1 } StaffClef;
typedef enum { STAFF_TITLE, STAFF_CLEF_MENU, STAFF_MODE_MENU, STAFF_SONG_MENU, STAFF_PLAYING, STAFF_DONE } StaffScreen;
typedef enum { STAFF_MODE_PRACTICE, STAFF_MODE_MISS } StaffMode;

typedef struct {
    void (*clear)(uint16_t color);
    void (*line)(int x1, int y1, int x2, int y2, uint16_t color);
    void (*circle)(int x, int y, int radius, uint16_t color, int filled);
    void (*text)(int x, int y, const char *text, uint16_t color);
    void (*tone)(uint16_t frequency_hz, uint16_t duration_ms);
} StaffPlatform;

typedef struct {
    StaffScreen screen;
    StaffClef clef;
    StaffMode mode;
    uint8_t song;
    uint8_t note_index;
    uint8_t menu_index;
    uint8_t error;
    uint16_t miss_count;
    uint32_t note_mask; /* FM-1 note press-edge bitset: bit 0 = F3, bit 26 = G5 */
    uint32_t last_ms;
    uint32_t note_x_q8;
    uint8_t note_played;
} StaffGame;

void staff_game_init(StaffGame *game, uint32_t now_ms);
void staff_game_update(StaffGame *game, uint32_t now_ms, uint32_t note_pressed_edges,
                       uint8_t play_pressed, uint8_t home_pressed,
                       int8_t encoder_steps, const StaffPlatform *platform);
void staff_game_draw(const StaffGame *game, const StaffPlatform *platform);

/* Public to make the clef/key relationship explicit for an FM-1 adapter. */
uint8_t staff_game_expected_key(StaffClef clef, uint8_t song, uint8_t note_index);
const char *staff_game_note_name(StaffClef clef, uint8_t song, uint8_t note_index);

#endif
