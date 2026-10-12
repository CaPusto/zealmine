#include "tileset_lz77.h"
#include <core.h>

static uint8_t buffer[256];
static const uint8_t* data;
static uint16_t remaining;
static uint8_t invalid;

/* Validate command framing once, outside the byte-copying hot loop. */
static uint8_t valid_stream(const uint8_t* source, uint16_t size)
{
    uint16_t i = 0;
    uint8_t pos = 0, history = 0;
    while (i < size) {
        uint8_t cmd = source[i++];
        uint8_t len;
        if (cmd & 0x80) {
            len = (cmd & 63) + 1;
            if (cmd & 0x40) len += 2;
            if (i == size) return 0;
            if ((cmd & 0x40) && !history && source[i] >= pos) return 0;
            i++;
        } else {
            len = cmd + 1;
            if (size - i < len) return 0;
            i += len;
        }
        if ((uint8_t)(pos + len) && (uint8_t)(pos + len) < pos) return 0;
        pos += len;
        if (!pos) history = 1;
    }
    return 1;
}

void lz77_init(const uint8_t* source, uint16_t size)
{
    data = source;
    remaining = size;
    invalid = !valid_stream(source, size);
}

uint8_t* lz77_buffer(void)
{
    return buffer;
}

int16_t lz77_read(void)
{
    if (invalid) return -1;
    if (!remaining) return 0;
    const uint8_t* source = data;
    const uint8_t* end = data + remaining;
    uint8_t* output = buffer;

    while (source < end) {
        uint8_t cmd = *source++;
        uint8_t len;
        if (cmd & 0x80) {
            if (cmd & 0x40) {
                uint8_t offset = *source++ + 1;
                uint8_t back = (uint8_t)(output - buffer) - offset;
                len = (cmd & 63) + 3;
                /* Circular and overlapping references require forward copying. */
                while (len--)
                    *output++ = buffer[back++];
            } else {
                uint8_t value = *source++;
                len = (cmd & 63) + 1;
                mem_set(output, value, len);
                output += len;
            }
        } else {
            len = cmd + 1;
            mem_cpy(output, source, len);
            output += len;
            source += len;
        }
        if (output == buffer + sizeof(buffer)) break;
    }
    remaining -= source - data;
    data = source;
    return output - buffer;
}
