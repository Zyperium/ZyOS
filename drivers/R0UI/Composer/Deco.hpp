#pragma once
#include "r0ui_protocol.hpp"

namespace R0UI {
    class DecoRoot {
    public:
        bool active;
        void Paint(int x, int y, int w, uint32_t *scr);
        void Realloc(int width);
        void Clean();
        uint32_t bufwidth;
    private:
        uint32_t stashed_pages;
        uint32_t *pixels;
        DecoEvent *r3_ref; // points to a ring 3 controlled structure!
    };
}