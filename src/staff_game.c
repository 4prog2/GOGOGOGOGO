/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "staff_game.h"

#define WHITE 0xFFFFu
#define BLACK 0x0000u
#define CYAN  0x07FFu
#define GOLD  0xFEA0u
#define RED   0xF800u
#define GREEN 0x07E0u
#define HIT_X 52
#define NOTE_START_X 222
#define STAFF_BOTTOM 168
#define STEP_PIXELS 12
#define Q8(v) ((uint32_t)(v) << 8)

/* Step number is a diatonic position from each clef's bottom line. */
static const int8_t songs[3][16] = {
    {0, 2, 4, 2, 1, 3, 5, 3, 0, 2, 4, 6, 5, 3, 1, 0},
    {4, 6, 7, 6, 3, 5, 7, 5, 2, 4, 6, 7, 7, 5, 3, 2},
    {7, 6, 4, 7, 5, 3, 6, 4, 2, 5, 3, 1, 4, 2, 0, 4}
};
static const char *song_names[4] = {"Morning Steps", "River Waltz", "Little March", "Daughter's Love"};
static const char *letters[7] = {"C", "D", "E", "F", "G", "A", "B"};
static const uint8_t natural_semitones[7] = {0, 2, 4, 5, 7, 9, 11};
static const uint16_t key_frequency_hz[27] = {
    175, 185, 196, 208, 220, 233, 247, 262, 277,
    294, 311, 330, 349, 370, 392, 415, 440, 466,
    494, 523, 554, 587, 622, 659, 698, 740, 784
};

static __attribute__((minsize)) int natural_midi(int midi)
{
    static const int natural[12] = {0, 0, 2, 2, 4, 5, 5, 7, 7, 9, 9, 11};
    return midi - (midi % 12) + natural[midi % 12];
}

static __attribute__((minsize)) int staff_base_midi(StaffClef clef)
{
    return clef == STAFF_TREBLE ? 64 : 43; /* E4 / G2, bottom staff lines */
}

static __attribute__((minsize)) int step_midi(StaffClef clef, int step)
{
    int base = staff_base_midi(clef);
    int base_pc = base % 12;
    int letter = 0, octave = base / 12;
    if (clef == STAFF_BASS) step += 6; /* keep practice notes in the FM-1 F3..G5 range */
    while (letter < 7 && natural_semitones[letter] != base_pc) ++letter;
    if (letter == 7) return base;
    while (step-- > 0) if (++letter == 7) { letter = 0; ++octave; }
    return natural_midi(octave * 12 + natural_semitones[letter]);
}

__attribute__((minsize)) uint8_t staff_game_expected_key(StaffClef clef, uint8_t song, uint8_t note_index)
{
    int midi;
    if (song >= 3 || note_index >= 16) return 0xFFu;
    midi = step_midi(clef, songs[song][note_index]);
    /* FM-1 natural-key positions are chromatic semitones beginning at F3 (MIDI 53). */
    if (midi < 53 || midi > 79) return 0xFFu;
    return (uint8_t)(midi - 53);
}

const char *staff_game_note_name(StaffClef clef, uint8_t song, uint8_t note_index)
{
    static char result[4];
    int midi;
    if (song >= 3 || note_index >= 16) return "?";
    midi = step_midi(clef, songs[song][note_index]);
    result[0] = letters[(midi % 12 == 0) ? 0 : (midi % 12 == 2) ? 1 : (midi % 12 == 4) ? 2 :
                        (midi % 12 == 5) ? 3 : (midi % 12 == 7) ? 4 : (midi % 12 == 9) ? 5 : 6][0];
    result[1] = (char)('0' + midi / 12 - 1);
    result[2] = '\0';
    return result;
}

__attribute__((minsize)) void staff_game_init(StaffGame *g, uint32_t now_ms)
{
    g->screen = STAFF_TITLE; g->clef = STAFF_TREBLE; g->song = 0;
    g->note_index = 0; g->menu_index = 0; g->error = 0;
    g->note_mask = 0; g->last_ms = now_ms; g->note_x_q8 = Q8(NOTE_START_X);
}

static __attribute__((minsize)) void error_tone(const StaffPlatform *p)
{
    if (p && p->tone) p->tone(110, 120);
}

__attribute__((minsize)) void staff_game_update(StaffGame *g, uint32_t now_ms, uint32_t notes,
                       uint8_t play, uint8_t home, int8_t encoder,
                       const StaffPlatform *p)
{
    uint32_t elapsed = now_ms - g->last_ms;
    uint32_t expected_mask;
    g->last_ms = now_ms; g->note_mask = notes;
    if (g->screen == STAFF_TITLE) {
        if (play) { g->screen = STAFF_CLEF_MENU; g->menu_index = g->clef; }
        return;
    }
    if (g->screen == STAFF_CLEF_MENU) {
        if (encoder) g->menu_index ^= 1u;
        if (play) { g->clef = (StaffClef)g->menu_index; g->screen = STAFF_SONG_MENU; g->menu_index = 0; }
        if (home) g->screen = STAFF_TITLE;
        return;
    }
    if (g->screen == STAFF_SONG_MENU) {
        if (encoder > 0) g->menu_index = (uint8_t)((g->menu_index + 1u) % 4u);
        if (encoder < 0) g->menu_index = (uint8_t)((g->menu_index + 3u) % 4u);
        if (play) {
            g->song = g->menu_index; g->note_index = 0; g->error = 0;
            g->note_x_q8 = Q8(NOTE_START_X); g->screen = STAFF_PLAYING;
        }
        if (home) g->screen = STAFF_CLEF_MENU;
        return;
    }
    if (g->screen == STAFF_DONE) {
        if (play || home) g->screen = STAFF_SONG_MENU;
        return;
    }
    if (home) { g->screen = STAFF_SONG_MENU; g->menu_index = g->song; return; }

    /* The MIDI-backed fourth song is scheduled by the FM-1 adapter. */
    if (g->song == 3u) return;

    expected_mask = 1u << staff_game_expected_key(g->clef, g->song, g->note_index);
    if (g->note_x_q8 > Q8(HIT_X)) {
        /* 42 px/s; elapsed time bounds prevent jumps after a suspended frame. */
        if (elapsed > 100u) elapsed = 100u;
        g->note_x_q8 -= (elapsed * 42u * 256u) / 1000u;
        if (g->note_x_q8 < Q8(HIT_X)) g->note_x_q8 = Q8(HIT_X);
    }
    if (g->note_x_q8 <= Q8(HIT_X) && notes) {
        if (notes & expected_mask) {
            g->error = 0;
            if (p && p->tone) p->tone(key_frequency_hz[staff_game_expected_key(g->clef, g->song, g->note_index)], 100);
            if (++g->note_index >= 16u) g->screen = STAFF_DONE;
            else g->note_x_q8 = Q8(NOTE_START_X);
        } else if (!g->error) { g->error = 1; error_tone(p); }
    }
}

static __attribute__((minsize)) int note_y(const StaffGame *g)
{
    int step = songs[g->song][g->note_index] + (g->clef == STAFF_BASS ? 6 : 0);
    return STAFF_BOTTOM - step * STEP_PIXELS;
}

static __attribute__((minsize)) int staff_bottom(const StaffGame *g) { (void)g; return STAFF_BOTTOM; }

__attribute__((minsize)) void staff_game_draw(const StaffGame *g, const StaffPlatform *p)
{
    int i, y;
    char label[24];
    if (!p || !p->clear || !p->line || !p->circle || !p->text) return;
    p->clear(BLACK);
    if (g->screen == STAFF_TITLE) {
        p->text(30, 70, "STAFF RUNNER", CYAN);
        p->text(30, 108, "Learn notes on FM-1", WHITE);
        p->text(30, 158, "PLAY: START", GOLD);
        return;
    }
    if (g->screen == STAFF_CLEF_MENU) {
        p->text(28, 54, "CHOOSE CLEF", CYAN);
        p->text(38, 105, g->menu_index == 0 ? "> TREBLE" : "  TREBLE", g->menu_index == 0 ? GOLD : WHITE);
        p->text(38, 137, g->menu_index == 1 ? "> BASS" : "  BASS", g->menu_index == 1 ? GOLD : WHITE);
        p->text(28, 202, "KNOB / PLAY", WHITE);
        return;
    }
    if (g->screen == STAFF_SONG_MENU) {
        p->text(32, 38, g->clef == STAFF_TREBLE ? "TREBLE CLEF" : "BASS CLEF", CYAN);
        p->text(32, 70, "CHOOSE SONG", WHITE);
        for (i = 0; i < 4; ++i) p->text(30, 100 + i * 25, song_names[i], (i == g->menu_index) ? GOLD : WHITE);
        p->text(32, 209, "KNOB / PLAY", CYAN);
        return;
    }
    /* Five clear lines occupy 40% of the screen. Spaces and note positions are 12 px. */
    for (i = 0; i < 5; ++i) p->line(24, staff_bottom(g) - i * 24, 232, staff_bottom(g) - i * 24, 0x7BEFu);
    p->line(HIT_X, 32, HIT_X, 224, CYAN);
    p->text(8, 4, g->clef == STAFF_TREBLE ? "TREBLE" : "BASS", CYAN);
    p->text(145, 28, song_names[g->song], WHITE);
    if (g->song < 3 && g->note_index < 16) {
        y = note_y(g);
        if ((songs[g->song][g->note_index] + (g->clef == STAFF_BASS ? 6 : 0)) % 2 == 0)
            p->line((int)(g->note_x_q8 >> 8) - 13, y, (int)(g->note_x_q8 >> 8) + 13, y, WHITE);
        p->circle((int)(g->note_x_q8 >> 8), y, 7, g->error ? RED : GOLD, 1);
        label[0] = ' '; label[1] = staff_game_note_name(g->clef, g->song, g->note_index)[0]; label[2] = '\0';
        p->text(184, 210, label, g->error ? RED : WHITE);
    }
    if (g->error) p->text(77, 194, "PLAY THE RIGHT NOTE", RED);
    else if (g->screen == STAFF_DONE) p->text(62, 194, "SONG COMPLETE!", GREEN);
    else p->text(84, 194, "PLAY THE NOTE", WHITE);
}
