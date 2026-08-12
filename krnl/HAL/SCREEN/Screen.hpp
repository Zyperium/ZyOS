#pragma once
#include <stdint.h>
#include <limine.h>

namespace HAL::SCREEN {
    enum class COL : uint32_t {
        WHITE = 0xFFFFFFFF,
        BLACK = 0xFF000000,
        RED = 0xFFFF0000,
        GREEN = 0xFF00FF00,
        BLUE = 0xFF0000FF,
        YELLOW = 0xFFFFFF00
    };

    struct rect {
        int x;
        int y;
        int width;
        int height;
    };

    struct SCRN {
        uint16_t width;
        uint16_t height;
        uint32_t pitch;
    };

    void initialize(limine_framebuffer_response *response);
    void flip_buffer();
    void scroll_screen(int x, int y);
    void fill_screen(uint32_t col);
    void fill_screen(COL col);
    void set_pixel(int x, int y, uint32_t col);
    void draw_char(char c, int x, int y, uint32_t col);
    void draw_char(char c, int x, int y, COL col);
    void add_damage(int x, int y, int w, int h);
    void add_damage(rect r);
    void repaint();
    SCRN get_scrdata();
    uint32_t *fetch_buffer();
}