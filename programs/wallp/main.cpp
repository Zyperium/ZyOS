#include <winlib/stb_impl.hpp>
#include <winlib/blur.hpp>
#include <winlib/r0ui_protocol.hpp>
#include <winlib/acryl.hpp>
#include <kalloc.h>
#include <klibkrnl.h>
#include <string.h>
#include <ksyscall.h>

/**
This is a mess of magic numbers. Should be cleaned up later I guess.
*/

using R0UI::Event;
using R0UI::EventType;
using R0UI::WinControl;
using R0UI::R0UICall;

extern "C" int main() {
    klog("Starting wallpaper program");

    WinControl *taskbar = (WinControl *)r0ui_call(R0UICall::OpenWindow, (uint64_t)"Taskbar");

    klog("Screen dims are %ix%i", taskbar->scrnw, taskbar->scrnh);

    taskbar->x = 0;
    taskbar->y = taskbar->scrnh - 48;
    taskbar->width = taskbar->scrnw;
    taskbar->height = 48;
    r0ui_call(R0UICall::PushRef, 0);

    uint32_t *wallpaper = (uint32_t *)r0ui_call(R0UICall::RequestWallpaper, 0);
    if (!wallpaper) {
        klog("Failed to acquire wallpaper backbuffer");
        return -1;
    }

    memset32(wallpaper, 0x0, taskbar->scrnw * taskbar->scrnh);
    r0ui_call(R0UICall::Redraw, 0);

    klog("Wallpaper backbuffer @ %p", wallpaper);

    int w, h;
    uint32_t *ptrx = load_png("A:/WALLPA~1.PNG", &w, &h);
    klog("Loaded image");
    uint32_t *nbuf = resize_image(ptrx, w, h, taskbar->scrnw, taskbar->scrnh);
    klog("Resized image");
    free(ptrx);

    asm volatile("" ::: "memory");
    memcpy(wallpaper, nbuf, taskbar->scrnw * taskbar->scrnh * 4);
    free(nbuf);

    r0ui_call(R0UICall::Redraw, 0);

    memcpy(taskbar->usr_pix_buf, &wallpaper[taskbar->scrnw * taskbar->scrnh - (taskbar->scrnw * 48)], taskbar->scrnw * 48 * 4);
    apply_blur(taskbar->usr_pix_buf, taskbar->scrnw, 48, 16, 3);
    apply_acrylic_finish(taskbar->usr_pix_buf, taskbar->scrnw, 48, 1);

    uint32_t *logobtn = load_png("A:/SYSTEM/MAIN.PNG", &w, &h);
    int offset = (48 - w) / 2;

    klog("Image dimensions are %ix%i", w, h);
    for (auto i{0}; i < h; ++i) {
        memcpy32_alpha(&taskbar->usr_pix_buf[offset + ((offset + i) * taskbar->width)], &logobtn[i * w], w);
    }
    free(logobtn);

    logobtn = load_png("A:/SYSTEM/SETTIN.PNG", &w, &h);
    offset = (48 - w) / 2;

    klog("Image dimensions are %ix%i", w, h);
    for (auto i{0}; i < h; ++i) {
        memcpy32_alpha(&taskbar->usr_pix_buf[offset + 45 + ((offset + i) * taskbar->width)], &logobtn[i * w], w);
    }
    free(logobtn);

    r0ui_call(R0UICall::PinWindow, (uint64_t)"Taskbar");
    r0ui_call(R0UICall::Redraw, 0);

    for (;;) {
        uint32_t h_idx = taskbar->events.head;
        uint32_t t_idx = taskbar->events.tail;

        while (t_idx != h_idx) {
            Event ev = taskbar->events.ring[t_idx];
            asm volatile("" ::: "memory");

            switch (ev.type) {
                case EventType::MouseMove:
                    klog("mouse move -> %d,%d", taskbar->mouse_pos.x, taskbar->mouse_pos.y);
                    break;
                case EventType::MouseDown:
                    klog("mouse down (buttons=%x)", ev.data.mouse.buttons);
                    break;
                case EventType::MouseUp:
                    klog("mouse up (buttons=%x)", ev.data.mouse.buttons);
                    break;
                case EventType::KeyDown:
                    klog("key down: %x", ev.data.key.keycode);
                    break;
                case EventType::KeyUp:
                    klog("key up: %x", ev.data.key.keycode);
                    break;
                default:
                    break;
            }

            t_idx = (t_idx + 1) % R0UI::EVENT_QUEUE_CAPACITY;
        }
        taskbar->events.tail = t_idx;

        yield();
    }

    return 0;
}