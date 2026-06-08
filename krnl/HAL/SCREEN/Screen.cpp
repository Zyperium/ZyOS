#include <HAL/SCREEN/Screen.hpp>
#include <HAL/SCREEN/Font.hpp>
#include <HAL/IDT/Panic.hpp>
#include <HAL/MEM/FMEM.hpp>

#include <Library/debug.hpp>
#include <Library/string.h>

namespace HAL::SCREEN {
    int screen_w, screen_h, screen_p;
    uint32_t *screen_addr;
    uint32_t *backbuffer;

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
        return;
    }

    void flip_buffer() {
        int dest_pitch_pixels = screen_p / sizeof(uint32_t); 

        for (int y = 0; y < screen_h; y++) {
            uint32_t* src_row = backbuffer + (y * screen_w);

            uint32_t* dest_row = screen_addr + (y * dest_pitch_pixels);

            FMEM::FastCopy(dest_row, src_row, screen_w * sizeof(uint32_t));
        }
    }

    void fill_screen(uint32_t col) {
        FMEM::FastFill32(backbuffer, col, screen_w * screen_h);
    }

    void fill_screen(COL col) {
        FMEM::FastFill32(backbuffer, static_cast<uint32_t>(col), screen_w * screen_h);
    }

    void set_pixel(int x, int y, uint32_t col) {
        if (x < 0 || x >= screen_w || y < 0 || y >= screen_h) {
            return;
        }
        
        backbuffer[x + (y * screen_w)] = col;
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
    }
}