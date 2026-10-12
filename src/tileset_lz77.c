#include "tileset_lz77.h"

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
    const uint8_t* source = data;
    uint16_t i = 0;
    uint8_t pos = 0;
    if (invalid) return -1;

    while (i < remaining) {
        uint8_t cmd = source[i++];
        uint8_t len;
        if (cmd & 0x80) {
            if (cmd & 0x40) {
                uint8_t offset = source[i++] + 1;
                len = (cmd & 63) + 3;
                while (len--) {
                    buffer[pos] = buffer[(uint8_t)(pos - offset)];
                    pos++;
                }
            } else {
                uint8_t value = source[i++];
                len = (cmd & 63) + 1;
                while (len--)
                    buffer[pos++] = value;
            }
        } else {
            len = cmd + 1;
            while (len--)
                buffer[pos++] = source[i++];
        }
        if (!pos) {
            data += i;
            remaining -= i;
            return 256;
        }
    }
    data += i;
    remaining = 0;
    return pos;
}
