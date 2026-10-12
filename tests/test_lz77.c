/* The byte decoder builds without graphics headers or the SDK. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tileset_lz77.h"

int main(int argc, char** argv)
{
    static uint8_t packed[65536], expected[65536];
    FILE* f;
    size_t compressed, total, pos = 0;
    int16_t count;
    assert(argc == 3);
    f = fopen(argv[1], "rb"); assert(f);
    compressed = fread(packed, 1, sizeof(packed), f); fclose(f);
    f = fopen(argv[2], "rb"); assert(f);
    total = fread(expected, 1, sizeof(expected), f); fclose(f);
    lz77_init(packed, compressed);
    while ((count = lz77_read()) > 0) {
        assert(count <= 256 && pos + count <= total);
        assert(memcmp(lz77_buffer(), expected + pos, count) == 0);
        pos += count;
    }
    assert(count == 0 && pos == total);
    {
        /* Two literal runs fill a block; the next block has a repeat followed
           by a reference that wraps history and overlaps its own output. */
        uint8_t stream[262];
        const uint8_t tail[] = {0x5a, 0x5a, 254, 255, 0x5a, 0x5a, 254, 255};
        stream[0] = stream[129] = 127;
        for (unsigned i = 0; i < 128; i++) {
            stream[1 + i] = (uint8_t)i;
            stream[130 + i] = (uint8_t)(128 + i);
        }
        stream[258] = 0x81;
        stream[259] = 0x5a;
        stream[260] = 0xc3;
        stream[261] = 3;
        lz77_init(stream, sizeof(stream));
        assert(lz77_read() == 256);
        for (unsigned i = 0; i < 256; i++) assert(lz77_buffer()[i] == i);
        assert(lz77_read() == sizeof(tail));
        assert(memcmp(lz77_buffer(), tail, sizeof(tail)) == 0);
        assert(lz77_read() == 0);
        /* Offset byte 255 means distance 256, not zero history. */
        stream[258] = 0xc0;
        stream[259] = 255;
        lz77_init(stream, 260);
        assert(lz77_read() == 256);
        assert(lz77_read() == 3);
        for (unsigned i = 0; i < 3; i++) assert(lz77_buffer()[i] == i);
        assert(lz77_read() == 0);
    }
    {
        const uint8_t truncated_literal[] = {2, 1};
        const uint8_t truncated_repeat[] = {0x80};
        const uint8_t invalid_reference[] = {0xc0, 0};
        const uint8_t boundary_crossing[] = {0xbf, 1, 0xbf, 1, 0xbf, 1, 0xbe, 1, 0x81, 1};
        const uint8_t* bad[] = {truncated_literal, truncated_repeat, invalid_reference, boundary_crossing};
        const uint16_t sizes[] = {2, 1, 2, 10};
        for (unsigned i = 0; i < 4; i++) {
            lz77_init(bad[i], sizes[i]);
            assert(lz77_read() == -1);
        }
    }
    return 0;
}
