#ifndef TILESET_LZ77_H
#define TILESET_LZ77_H

#include <stdint.h>

/* One sequential byte stream with a shared 256-byte history/output buffer.
 * Copy/upload returned bytes before reading again; do not modify the buffer. */
void lz77_init(const uint8_t* data, uint16_t size);
uint8_t* lz77_buffer(void);
/* Returns 1..256 decoded bytes, 0 at EOF, or -1 for an invalid stream.
 * Commands cannot cross a 256-byte output boundary. No pixel-format knowledge. */
int16_t lz77_read(void);

#endif
