#include <Services/TTY/BootTTY.hpp>
// Fine, because it doesn't rely on anything needing initialization
#include <HAL/SCREEN/Font.hpp>
#include <Library/string.h>
#include <Library/debug.hpp>

/**
 The point of this is to provide onscreen logging capabilities BEFORE 
 most kernel services are ready. Think PMM, VMM, PMEM, KMEM, scheduling, etc.
*/



namespace TTY::BOOT {
    uint32_t *framebuffer{};
    uint32_t scrw{}, scrh{}, scrp{};
    uint32_t logy{};
    bool active{true};

    void initialize(limine_framebuffer_response *response) {
        if (response->framebuffer_count == 0) {
            // Panic later, during screen initialization.
            return;
        }
        framebuffer = static_cast<uint32_t *>(response->framebuffers[0]->address);
        scrw = response->framebuffers[0]->width;
        scrh = response->framebuffers[0]->height;
        scrp = response->framebuffers[0]->pitch;

        Debug::krnl_print("BTTY", Debug::LOG_INFO, "Boot TTY initialized!");
        Debug::krnl_print("BTTY", Debug::LOG_INFO, "Screen dimensions: %ix%i, p: %i", scrw, scrh, scrp);
        return;
    }

    bool is_active() {
        return active;
    }

    void disable() {
        active = false;
    }

    int logx{};

    uint32_t global_col = 0;

    void putc(char c) {
        if (c == ' ') {
            logx++;
            return;
        }
        for (auto ly{0}; ly < Font::HEIGHT; ly++) {
            for (auto lx{0}; lx < Font::WIDTH; lx++) {
                if (Font::get_pixel(c, lx, ly)) {
                    int draw_x = (logx * Font::WIDTH) + lx;
                    int draw_y = (logy * Font::HEIGHT) + ly;
                    framebuffer[draw_x + (draw_y * scrw)] = global_col;
                }
            }
        }

        logx++;
    }

    void show_log(const char *lclass, const char *level, const char *log) {
        if (!active) return;

        if (lclass[0] == 'I' && level[0] == 'I' && log[0] == 'P') {
            logy = 0;
        }

        if (level[0] == 'I') {
            global_col = 0xFF00FF00;
        }
        else if (level[0] == 'W') {
            global_col = 0xFFFFFF00;
        }
        else if (level[0] == 'E') {
            global_col = 0xFFFF0000;
        }

        logx = 0;
        int cl{0};

        putc('[');
        while (lclass[cl]) {
            putc(lclass[cl]);
            cl++;
        }
        putc(']');
        putc(' ');

        cl = 0;
        putc('<');
        while (level[cl]) {
            putc(level[cl]);
            cl++;
        }
        putc('>');
        putc(' ');

        cl = 0;
        while (log[cl]) {
            putc(log[cl]);
            cl++;
        }

        logy++;

        if (logy >= 69) {
            logy = 0;
            memset(framebuffer, 0, scrh * scrp);
        }
    }
}