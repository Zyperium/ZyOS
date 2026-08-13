#include <winlib/stb_impl.hpp>
#include <winlib/blur.hpp>
#include <winlib/r0ui_protocol.hpp>
#include <winlib/acryl.hpp>
#include <kalloc.h>
#include <klibkrnl.h>
#include <string.h>

/**
This is a mess of magic numbers. Should be cleaned up later I guess.
*/

using R0UI::Event;
using R0UI::EventType;
using R0UI::WinControl;
using R0UI::R0UICall;

static inline uint64_t r0ui_call(R0UICall id) {
    return ioctl("R0UI/", (uint64_t)id);
}

extern "C" int main() {
    klog("Starting window program");

    WinControl *ptr = (WinControl *)r0ui_call(R0UICall::OpenWindow);

    klog("Screen dims are %ix%i", ptr->scrnw, ptr->scrnh);

    int w, h;
    uint32_t *ptrx = load_png("A:/WALLPA~1.PNG", &w, &h);
    klog("Loaded image");
    uint32_t *nbuf = resize_image(ptrx, w, h, ptr->scrnw, ptr->scrnh);
    klog("Resized image");
    free(ptrx);

    ptr->x = 0;
    ptr->y = 0;
    ptr->width = ptr->scrnw;
    ptr->height = ptr->scrnh;

    uint32_t *taskbar_img = (uint32_t *)malloc(ptr->scrnw * 48 * 4); // taskbar is 48 pixels
    memcpy(taskbar_img, &nbuf[ptr->scrnw * ptr->scrnh - (ptr->scrnw * 48)], ptr->scrnw * 48 * 4);
    // memset32(taskbar_img, 0x00, ptr->scrnw * 48);
    apply_blur(taskbar_img, ptr->scrnw, 48, 8, 2);
    apply_acrylic_finish(taskbar_img, ptr->scrnw, 48, 1);

    r0ui_call(R0UICall::PushRef);

    asm volatile("" ::: "memory");
    memcpy(ptr->usr_pix_buf, nbuf, ptr->scrnw * ptr->scrnh * 4);
    memcpy(&ptr->usr_pix_buf[ptr->scrnw * ptr->scrnh - (ptr->scrnw * 48)], taskbar_img, ptr->scrnw * 48 * 4);
    
    klog("Curr. values are: %ux%u", ptr->width, ptr->height);
    // free(nbuf);

    r0ui_call(R0UICall::Redraw);

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