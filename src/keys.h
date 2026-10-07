#ifndef KEYS_H
#define KEYS_H

#include <stdint.h>

/* Folds keyboard events into the same 16-bit button mask the gamepad uses, so
   the board only ever deals with one kind of input.

   zgdk's keyboard_read() cannot be used for this: it maps letters onto spare
   gamepad buttons (Q to L, W to R) and silently drops every other key, so there
   is no way to reach the restart key. */

/* Puts the keyboard driver in raw mode and clears any remembered key state.
   Must be called before keys_read(). Returns 0 on success. */
int keys_init(void);

/* Hands the keyboard back in cooked mode for whoever starts us next. */
void keys_deinit(void);

uint16_t keys_read(void);

#endif