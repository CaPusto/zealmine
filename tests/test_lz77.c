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
