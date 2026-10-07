#include <stdint.h>
#include <string.h>

#include <zos_errors.h>
#include <zos_keyboard.h>
#include <zos_sys.h>
#include <zos_vfs.h>

#include <zgdk/input/button_map.h>

#include "keys.h"
#include "zealmine.h"

/* The driver must be in raw mode, set up by keys_init(). In the default cooked
   mode it buffers input until Enter flushes it and drops release events
   entirely, so a key that was tapped once stays marked down forever and the
   cursor walks to the edge of the board.

   The blocking flag is a separate field of the same ioctl argument, so raw mode
   on its own would leave read() waiting for a key that may never come. */

/* zgdk gives the driver a 32 byte aligned buffer, taken from a padded array and
   narrowed at run time because a static initializer cannot do that arithmetic.

   The padding only helps if the start is rounded *up*: the array is sized
   KEYS_BUFFER_SIZE + (KEYS_BUFFER_SIZE - 1), so an aligned start rounded up
   stays inside it, while rounding down would aim the driver at whatever sits
   before the array and hand it to the keyboard interrupt to overwrite. */
#define KEYS_BUFFER_SIZE 32
static uint8_t storage[KEYS_BUFFER_SIZE + (KEYS_BUFFER_SIZE - 1)];
static uint8_t* buffer;

static void align_buffer(void)
{
    buffer = (uint8_t*)(
        ((uintptr_t)storage + (KEYS_BUFFER_SIZE - 1))
        & ~(uintptr_t)(KEYS_BUFFER_SIZE - 1));
}

/* Which physical keys are down, indexed by the code the driver reports.

   Deriving the button mask from this rather than setting and clearing button
   bits keeps releases exact: two keys that share a button, such as an arrow and
   its WASD twin, cannot clear each other, and a release that arrives in the same
   read as its marker cannot be mistaken for a fresh press. */
static uint8_t held[32];

/* Kept across reads: the driver may split a release marker from the key it
   refers to at a read boundary. */
static uint8_t released;

/* Button bits for every key that went down since the last keys_read().

   A tap can arrive pressed and released inside a single read, and a read can
   even carry several taps at once. Levels alone would never show those, so the
   press is latched here and reported once. Without this a quick tap of X or Q
   is silently dropped, which is what makes the keys feel unpredictable. */
static uint16_t pending;

/* Every key the board reacts to, and the button bit it stands for. Two keys may
   share a bit, such as an arrow and its WASD twin. */
typedef struct {
    uint8_t key;
    uint16_t bit;
} Binding;

static const Binding bindings[] = {
    { KB_LEFT_ARROW, BUTTON_LEFT }, { KB_KEY_A, BUTTON_LEFT },
    { KB_RIGHT_ARROW, BUTTON_RIGHT }, { KB_KEY_D, BUTTON_RIGHT },
    { KB_UP_ARROW, BUTTON_UP }, { KB_KEY_W, BUTTON_UP },
    { KB_DOWN_ARROW, BUTTON_DOWN }, { KB_KEY_S, BUTTON_DOWN },
    { KB_KEY_Z, BUTTON_B }, { KB_KEY_SPACE, BUTTON_B },
    { KB_KEY_X, BUTTON_A },
    { KB_KEY_Q, KEY_QUIT },
    { KB_KEY_R, KEY_RESTART },
};

#define BINDING_COUNT (uint8_t)(sizeof(bindings) / sizeof(bindings[0]))

int keys_init(void)
{
    const zos_err_t err =
        ioctl(DEV_STDIN, KB_CMD_SET_MODE, (void*)(KB_MODE_RAW | KB_READ_NON_BLOCK));

    if (err != ERR_SUCCESS)
        return 1;

    memset(held, 0, sizeof(held));
    released = 0;
    pending = 0;

    return 0;
}

void keys_deinit(void)
{
    /* Whoever starts us expects the line editor back. */
    ioctl(DEV_STDIN, KB_CMD_SET_MODE, (void*)(KB_MODE_COOKED | KB_READ_NON_BLOCK));
    memset(held, 0, sizeof(held));
    released = 0;
    pending = 0;
}

static int is_down(uint8_t key)
{
    return (held[key >> 3] >> (key & 7)) & 1;
}

static void set_down(uint8_t key, uint8_t down)
{
    if (down)
        held[key >> 3] |= (uint8_t)(1 << (key & 7));
    else
        held[key >> 3] &= (uint8_t)~(1 << (key & 7));
}

static int any_down(void)
{
    uint8_t i;

    for (i = 0; i < (uint8_t)sizeof(held); i++) {
        if (held[i])
            return 1;
    }
    return 0;
}

static uint16_t bit_of(uint8_t key)
{
    uint8_t i;

    for (i = 0; i < BINDING_COUNT; i++) {
        if (bindings[i].key == key)
            return bindings[i].bit;
    }
    return 0;
}

static uint16_t mask_of(void)
{
    uint16_t mask = 0;
    uint8_t i;

    for (i = 0; i < BINDING_COUNT; i++) {
        if (is_down(bindings[i].key))
            mask |= bindings[i].bit;
    }
    if (any_down())
        mask |= KEY_ANY;
    return mask;
}

uint16_t keys_read(void)
{
    if (buffer == 0)
        align_buffer();

    for (;;) {
        uint16_t size = KEYS_BUFFER_SIZE;
        const zos_err_t err = read(DEV_STDIN, buffer, &size);

        if (err != ERR_SUCCESS)
            break;

        if (size == 0)
            break;

        for (uint8_t i = 0; i < size; i++) {
            if (buffer[i] == KB_RELEASED) {
                released = 1;
                continue;
            }
            if (!released)
                pending |= (uint16_t)(bit_of(buffer[i]) | KEY_ANY);
            set_down(buffer[i], (uint8_t)!released);
            released = 0;
        }
    }

    {
        const uint16_t mask = (uint16_t)(mask_of() | pending);
        pending = 0;
        return mask;
    }
}