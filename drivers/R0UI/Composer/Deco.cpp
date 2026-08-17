#include "Deco.hpp"
#include "Composer.hpp"
#include "r0ui_protocol.hpp"
#include <HAL.hpp>

using namespace HAL::MEM;

namespace R0UI {
    void DecoRoot::Clean() {
        bufwidth = 0;

        if (pixels)
            PMEM::free_pages(pixels, stashed_pages);
        pixels = nullptr;
        stashed_pages = 0;
        active = false;
    }

    void DecoRoot::Realloc(int width) {
        if ((uint32_t)width == bufwidth) return;
        bufwidth = width;
        uint32_t t_bytes = width * DECO_HEIGHT;

        if (pixels)
            PMEM::free_pages(pixels, stashed_pages);
    
        stashed_pages = ((t_bytes  * sizeof(uint32_t)) + (VMM::SIZE_OF_PAGE - 1 )) / VMM::SIZE_OF_PAGE;
        
        pixels = (uint32_t *)PMEM::alloc_pages(stashed_pages, VMM::PTE_PRESENT | VMM::PTE_WRITABLE | VMM::PTE_NX);

        FMEM::FastFill32(pixels, 0xFF00FF00, t_bytes);
    }

    void DecoRoot::Paint(int x, int y, int w, uint32_t *screen) {
        int32_t clip_len = y + DECO_HEIGHT;
        if (y < 0) y = 0;

        if (clip_len < y)
            return;

        int32_t clip_left   = (x < 0) ? 0 : x;

        for (auto ny{y}; ny < clip_len; ++ny) {
            int32_t win_y = ny - y;

            uint32_t dest_offset = clip_left + (ny * Composer::width);
            uint32_t src_offset  = x + (win_y * bufwidth);

            HAL::MEM::FMEM::FastCopy(
                &screen[dest_offset],
                &pixels[src_offset],
                w * sizeof(uint32_t)
            );
        }
    }
}