/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_ADAPTER_H
#define FM1_ADAPTER_H

/*
 * FM1 Open V14's SDK API is used by staff_game_fm1.c. The SDK exposes held
 * physical note bits (0..26 = F3..G5), button bits, encoder detents, a wrapping
 * millisecond clock, RGB565 screen fills, and local MIDI posting. The adapter
 * derives note/button press edges from successive fm1_controls snapshots.
 *
 * The original input map was cross-checked against FM1 Quest's public
 * fm1_input.h; the shipped game adapter uses the official FM1 Open API.
 */
#include "staff_game.h"

#endif
