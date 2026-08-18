#include <string.h>
#include <winlib/r0ui_protocol.hpp>
#include <winlib/shrink.hpp>

using R0UI::Event;
using R0UI::EventType;
using R0UI::WinControl;
using R0UI::R0UICall;

extern "C" int main() {
    WinControl *taskwin = (WinControl *)r0ui_call(R0UICall::OpenWindow, (uint64_t)"Task Watcher");

    taskwin->x = 100;
    taskwin->y = 100;
    taskwin->width = 300;
    taskwin->height = 220;
    taskwin->flags |= R0UI::WindowFlags::HasDecor;

    r0ui_call(R0UI::R0UICall::PushRef, 0);

    memset(taskwin->usr_pix_buf, 0xFF, taskwin->width * taskwin->height * 4);

    constexpr uint64_t CLOSE_BTN_WIDTH = 70;
    constexpr uint64_t CLOSE_BTN_HEIGHT = 30;
    constexpr uint64_t CLOSE_BTN_RADIUS = 5;
    constexpr uint64_t CLOSE_BTN_PADDING = 5;

    draw_rounded_rect_avx2(taskwin->usr_pix_buf, taskwin->width, taskwin->width - CLOSE_BTN_WIDTH - CLOSE_BTN_PADDING, taskwin->height - CLOSE_BTN_HEIGHT - CLOSE_BTN_PADDING, CLOSE_BTN_WIDTH, CLOSE_BTN_HEIGHT, CLOSE_BTN_RADIUS, 0xFFAA7623);

    r0ui_call(R0UI::R0UICall::RedrawMyWindows, 0);
    for (;;) {
        uint32_t h_idx = taskwin->events.head;
        uint32_t t_idx = taskwin->events.tail;

        while (t_idx != h_idx) {
            Event ev = taskwin->events.ring[t_idx];
            asm volatile("" ::: "memory");

            switch (ev.type) {
                case EventType::MouseMove:

                case EventType::MouseDown:

                case EventType::MouseUp:
                    
                case EventType::KeyDown:

                case EventType::KeyUp:

                default:
                    break;
            }

            t_idx = (t_idx + 1) % R0UI::EVENT_QUEUE_CAPACITY;
        }
        taskwin->events.tail = t_idx;

        yield();
    }
    return 0;
}