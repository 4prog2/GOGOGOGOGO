#ifndef FM1_ADAPTER_H
#define FM1_ADAPTER_H

/*
 * FM-1 HAL adapter notes (based on the public FM1 Quest HAL conventions):
 *
 *   uint32_t pressed = fm1_input_note_edges();
 *   int8_t turn = (int8_t)fm1_enc_take(0);    // encoder 1, clockwise positive
 *   uint32_t buttons = fm1_input_edges(0);
 *   staff_game_update(&game, fm1_ms, pressed, (buttons >> B_PLAY) & 1,
 *                     (buttons >> B_HOME) & 1, turn, &platform);
 *
 * fm1_input_note_edges() returns bit 0..26 for F3..G5 press edges. Pass those
 * bits directly: staff_game_expected_key() returns the same bit index. Poll
 * this once per main-loop frame, and use fm1_ms (or an equivalent wrap-safe
 * millisecond clock). Implement StaffPlatform drawing operations with the
 * firmware's 240x240 LCD primitives and tone() with its audio/synth API.
 *
 * The exact button enum names and LCD/audio calls depend on the firmware base;
 * this repository intentionally does not vendor somebody else's firmware.
 */
#include "staff_game.h"

#endif
