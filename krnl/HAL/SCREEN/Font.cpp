#include <HAL/SCREEN/Font.hpp>
#include <stdint.h>

namespace Font
{
    bool get_pixel(char c, int local_x, int local_y) {
        if (c < 32) return false;

        if (local_y < PAD_Y || local_y >= PAD_Y + GLYPH_HEIGHT) return false;
        if (local_x < PAD_X || local_x >= PAD_X + GLYPH_WIDTH) return false;

        int row_idx = local_y - PAD_Y;
        uint8_t row = font_data[c - 32][row_idx];

        int bit_idx = (GLYPH_WIDTH - 1) - (local_x - PAD_X);
        return (row & (1 << bit_idx));
    }
}
