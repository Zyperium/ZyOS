#pragma once

#include <stdint.h>
#include <stddef.h>

namespace R0UI {
    struct Point {
        int32_t x;
        int32_t y;
    };

    struct Rect {
        int32_t x;
        int32_t y;
        uint32_t width;
        uint32_t height;

        [[nodiscard]] constexpr bool contains(Point p) const noexcept {
            return (p.x >= x && p.x < x + static_cast<int32_t>(width) &&
                    p.y >= y && p.y < y + static_cast<int32_t>(height));
        }
    };

    enum class EventType : uint8_t {
        None = 0,
        MouseMove,
        MouseDown,
        MouseUp,
        KeyDown,
        KeyUp,
        WindowResize,
        WindowFocus
    };

    struct alignas(16) Event {
        EventType type;
        uint8_t flags;
        uint16_t reserved0;
        uint32_t timestamp;

        union {
            struct {
                int32_t rel_x;
                int32_t rel_y;
                uint8_t buttons;
            } mouse;

            struct {
                uint32_t keycode;
                uint16_t modifiers;
                uint8_t pressed;
            } key;

            Rect new_bounds;
        } data;

        uint8_t reserved1[8];
    };
    static_assert(sizeof(Event) == 32, "Event must be exactly 32 bytes");

    constexpr size_t EVENT_QUEUE_CAPACITY = 112;

    struct EventQueue {
        volatile uint32_t head;
        volatile uint32_t tail;
        Event ring[EVENT_QUEUE_CAPACITY];
    };
    static_assert(sizeof(EventQueue) == 3600, "EventQueue size mismatch");

    struct WindowFlags {
        static constexpr uint32_t Active    = 1 << 0;
        static constexpr uint32_t Focused   = 1 << 1;
        static constexpr uint32_t Resizable = 1 << 2;
        static constexpr uint32_t Dirty     = 1 << 3;
        static constexpr uint32_t HasDecor  = 1 << 4;
    };

    struct alignas(4096) WinControl {
        int32_t x, y;
        uint32_t width, height;
        uint32_t z_index;
        volatile uint32_t flags;

        uint32_t *usr_pix_buf;
        uint32_t scrnw, scrnh;
        Point mouse_pos;

        EventQueue events;
    };
    static_assert(sizeof(WinControl) == 0x1000, "WinControl MUST be exactly 4096 bytes");

    struct alignas(4096) WindowView {
        volatile uint32_t generation;
        volatile uint32_t valid;
        uint32_t *pix_buf;
        uint32_t width, height;
        int32_t x, y;
        uint32_t scrnw, scrnh;
    };
    static_assert(sizeof(WindowView) <= 4096, "WindowView must fit in a single page");

    enum class R0UICall : uint64_t {
        Redraw      = 0,
        OpenWindow  = 1,
        PushRef     = 2,
        WatchWindow = 3,
    };
}