#include <HAL/SCREEN/Screen.hpp>
#include <HAL/SCREEN/Font.hpp>
#include <HAL/IDT/Panic.hpp>
#include <HAL/MEM/FMEM.hpp>

#include <Library/debug.hpp>
#include <Library/string.h>

namespace HAL::SCREEN {
    int screen_w, screen_h, screen_p;
    uint32_t *screen_addr{};
    uint32_t *backbuffer{};
    uint32_t *debug_layer{};

    bool has_damage{false};
    int damage_x1{0};
    int damage_y1{0};
    int damage_x2{0};
    int damage_y2{0};

    void add_damage(int x, int y, int w, int h) {
        if (w <= 0 || h <= 0) return;

        int x2 = x + w;
        int y2 = y + h;

        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x2 > screen_w) x2 = screen_w;
        if (y2 > screen_h) y2 = screen_h;

        if (x >= x2 || y >= y2) return;

        if (!has_damage) {
            damage_x1 = x;
            damage_y1 = y;
            damage_x2 = x2;
            damage_y2 = y2;
            has_damage = true;
        } else {
            if (x < damage_x1) damage_x1 = x;
            if (y < damage_y1) damage_y1 = y;
            if (x2 > damage_x2) damage_x2 = x2;
            if (y2 > damage_y2) damage_y2 = y2;
        }
    }

    void add_damage(rect r) {
        add_damage(r.x, r.y, r.width, r.height);
    }

    void initialize(limine_framebuffer_response *response) {
        Debug::krnl_print("SCRN", Debug::LOG_INFO, "Initialize");
        if (response->framebuffer_count == 0) {
            panic(PanicReasons::HAL_FAILED_INITIALIZATION);
        }

        screen_addr = static_cast<uint32_t *>(response->framebuffers[0]->address);
        screen_w = response->framebuffers[0]->width;
        screen_h = response->framebuffers[0]->height;
        screen_p = response->framebuffers[0]->pitch;

        backbuffer = new uint32_t[screen_w * screen_h];
        has_damage = false;
        return;
    }

    void repaint() {
        if (!backbuffer || !has_damage) return;

        int dest_pitch_pixels = screen_p / sizeof(uint32_t);
        int copy_width = damage_x2 - damage_x1;
        size_t copy_bytes = copy_width * sizeof(uint32_t);

        for (int y = damage_y1; y < damage_y2; y++) {
            uint32_t* src_row = backbuffer + (y * screen_w) + damage_x1;
            uint32_t* dest_row = screen_addr + (y * dest_pitch_pixels) + damage_x1;

            HAL::MEM::FMEM::FastCopy(dest_row, src_row, copy_bytes);
        }

        has_damage = false;
    }

    void flip_buffer() {
        int dest_pitch_pixels = screen_p / sizeof(uint32_t); 

        for (int y = 0; y < screen_h; y++) {
            uint32_t* src_row = backbuffer + (y * screen_w);

            uint32_t* dest_row = screen_addr + (y * dest_pitch_pixels);

            HAL::MEM::FMEM::FastCopy(dest_row, src_row, screen_w * sizeof(uint32_t));
        }

        has_damage = false;
    }

    void fill_screen(uint32_t col) {
        if (!backbuffer) return;
        HAL::MEM::FMEM::FastFill32(backbuffer, col, screen_w * screen_h);
        add_damage(0, 0, screen_w, screen_h);
    }

    void fill_screen(COL col) {
        if (!backbuffer) return;
        HAL::MEM::FMEM::FastFill32(backbuffer, static_cast<uint32_t>(col), screen_w * screen_h);
        add_damage(0, 0, screen_w, screen_h);
    }

    void set_pixel(int x, int y, uint32_t col) {
        if (!backbuffer) return;
        if (x < 0 || x >= screen_w || y < 0 || y >= screen_h) {
            return;
        }
        
        backbuffer[x + (y * screen_w)] = col;
        add_damage(x, y, 1, 1);
    }

    void draw_char(char c, int x, int y, uint32_t col) {
        if (!backbuffer) return;

        for (auto ly{0}; ly < Font::HEIGHT; ly++) {
            for (auto lx{0}; lx < Font::WIDTH; lx++) {

                if (Font::get_pixel(c, lx, ly)) {
                    int draw_x = x + lx;
                    int draw_y = y + ly;

                    set_pixel(draw_x, draw_y, col);
                }
            }
        }
        add_damage(x, y, Font::WIDTH, Font::HEIGHT);
    }

    uint32_t *get_buffer() {
        return backbuffer;
    }

    void draw_char(char c, int x, int y, COL col) {
        if (!backbuffer) return;

        for (auto ly{0}; ly < Font::HEIGHT; ly++) {
            for (auto lx{0}; lx < Font::WIDTH; lx++) {

                if (Font::get_pixel(c, lx, ly)) {
                    int draw_x = x + lx;
                    int draw_y = y + ly;

                    set_pixel(draw_x, draw_y, static_cast<uint32_t>(col));
                }
            }
        }
        add_damage(x, y, Font::WIDTH, Font::HEIGHT);
    }

    screen_dim get_dim() {
        if (!backbuffer) return {};
        screen_dim s;
        s.height = screen_h;
        s.width = screen_w;
        s.pitch = screen_p;
        return s;
    }

    void scroll_screen(int shift_x, int shift_y) {
        if (!backbuffer) return;
        if (shift_x == 0 && shift_y == 0) return;
        if (shift_y != 0) {
            if (abs(shift_y) >= screen_h) {
                HAL::MEM::FMEM::FastFill32(backbuffer, 0, screen_w * screen_h);
            } else if (shift_y > 0) {
                int rows_to_copy = screen_h - shift_y;
                uint32_t* dest = backbuffer;
                uint32_t* src = backbuffer + (shift_y * screen_w);

                HAL::MEM::FMEM::FastCopy(dest, src, rows_to_copy * screen_w * sizeof(uint32_t));

                uint32_t* clear_start = backbuffer + (rows_to_copy * screen_w);
                HAL::MEM::FMEM::FastFill32(clear_start, 0, shift_y * screen_w);
            } else {
                int shift_abs = -shift_y;
                int rows_to_copy = screen_h - shift_abs;
                uint32_t* dest = backbuffer + (shift_abs * screen_w);
                uint32_t* src = backbuffer;
                HAL::MEM::FMEM::FastCopy(dest, src, rows_to_copy * screen_w * sizeof(uint32_t));
                HAL::MEM::FMEM::FastFill32(backbuffer, 0, shift_abs * screen_w);
            }
        }
        if (shift_x != 0) {
            if (abs(shift_x) >= screen_w) {
                HAL::MEM::FMEM::FastFill32(backbuffer, 0, screen_w * screen_h);
                return;
            }
            for (int y = 0; y < screen_h; y++) {
                uint32_t* row_start = backbuffer + (y * screen_w);

                if (shift_x > 0) {
                    int pixels_to_copy = screen_w - shift_x;
                    HAL::MEM::FMEM::FastCopy(row_start, row_start + shift_x, pixels_to_copy * sizeof(uint32_t));

                    HAL::MEM::FMEM::FastFill32(row_start + pixels_to_copy, 0, shift_x);
                } else {
                    int shift_abs = -shift_x;
                    int pixels_to_copy = screen_w - shift_abs;
                    HAL::MEM::FMEM::FastCopy(row_start + shift_abs, row_start, pixels_to_copy * sizeof(uint32_t));

                    HAL::MEM::FMEM::FastFill32(row_start, 0, shift_abs);
                }
            }
        }
        add_damage(0, 0, screen_w, screen_h);
    }
}