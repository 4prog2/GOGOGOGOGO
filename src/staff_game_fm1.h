#ifndef STAFF_GAME_FM1_H
#define STAFF_GAME_FM1_H

#include "fm1.h"

/* Call after fm1_controls_read() in the FM1 Open control task.
 * Returns nonzero while the game owns the display and controls. */
int staff_game_fm1_control(const struct fm1_controls *controls);

/* Consume incoming MIDI note messages as game keys. Other messages return 0. */
int staff_game_fm1_midi_received(const uint8_t *message, size_t bytes);

#endif
