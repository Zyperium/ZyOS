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

    WinControl *ptr = (WinControl *)r0ui_call(R0UICall::OpenWindow, (uint64_t)"WallpaperRenderer");

    klog("WinControl @ %x", ptr);

    klog("Screen dims are %ix%i", ptr->scrnw, ptr->scrnh);

    ptr->x = 0;
    ptr->y = 0;
    ptr->width = ptr->scrnw;
    ptr->height = ptr->scrnh;

    r0ui_call(R0UICall::PushRef, 0);
    
    klog("Curr. values are: %ux%u", ptr->width, ptr->height);
    // free(nbuf);

    r0ui_call(R0UICall::Redraw, 0);

    int w, h;
    uint32_t *ptrx = load_png("A:/WALLPA~1.PNG", &w, &h);
    klog("Loaded image");
    uint32_t *nbuf = resize_image(ptrx, w, h, ptr->scrnw, ptr->scrnh);
    klog("Resized image");
    free(ptrx);

    asm volatile("" ::: "memory");
    memcpy(ptr->usr_pix_buf, nbuf, ptr->scrnw * ptr->scrnh * 4);
    
    klog("Curr. values are: %ux%u", ptr->width, ptr->height);
    // free(nbuf);

    r0ui_call(R0UICall::Redraw, 0);

    syscall(21, "A:/TASKBAR.ZYX");

    for (;;) {
        uint32_t h_idx = ptr->events.head;
        uint32_t t_idx = ptr->events.tail;

        while (t_idx != h_idx) {
            Event ev = ptr->events.ring[t_idx];
            asm volatile("" ::: "memory");

            switch (ev.type) {
                case EventType::MouseMove:
                    klog("mouse move -> %d,%d", ptr->mouse_pos.x, ptr->mouse_pos.y);
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
        ptr->events.tail = t_idx;
        
        yield();
    }

    return 0;
}