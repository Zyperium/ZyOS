#include "stb_impl.hpp"
#include "blur.hpp"
#include "r0ui_protocol.hpp"
#include <kalloc.h>
#include <klibkrnl.h>
#include <string.h>

using R0UI::Event;
using R0UI::EventType;
using R0UI::WinControl;
using R0UI::R0UICall;

static inline uint64_t r0ui_call(R0UICall id) {
    return ioctl("R0UI/", (uint64_t)id);
}

#define _MM_MALLOC_H_INCLUDED
#include <xmmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>
#include <stdint.h>

void apply_acrylic_finish(uint32_t *taskbar_img, int width, int height, int is_dark_mode) {
    if (!taskbar_img || width <= 0 || height <= 0) {
        return;
    }

    uint32_t *top_row = taskbar_img;
    for (int x = 0; x < width; x++) {
        uint32_t pixel = top_row[x];

        uint32_t r = (pixel >> 16) & 0xFF;
        uint32_t g = (pixel >> 8) & 0xFF;
        uint32_t b = pixel & 0xFF;

        r = r + ((255 - r) * 76 >> 8);
        g = g + ((255 - g) * 76 >> 8);
        b = b + ((255 - b) * 76 >> 8);

        top_row[x] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
    }

    if (height > 1) {
        uint32_t *second_row = taskbar_img + width;
        for (int x = 0; x < width; x++) {
            uint32_t pixel = second_row[x];
            uint32_t r = (pixel >> 16) & 0xFF;
            uint32_t g = (pixel >> 8) & 0xFF;
            uint32_t b = pixel & 0xFF;

            r = r + ((255 - r) * 25 >> 8);
            g = g + ((255 - g) * 25 >> 8);
            b = b + ((255 - b) * 25 >> 8);

            second_row[x] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
        }
    }

    int total_pixels = width * height;
    int i = 0;

    if (is_dark_mode) {
        __m128i mult_vec = _mm_set1_epi16(166);
        __m128i add_vec  = _mm_set1_epi16(24);
        __m128i zero     = _mm_setzero_si128();

        for (; i <= total_pixels - 4; i += 4) {
            __m128i pixels = _mm_loadu_si128((__m128i *)(taskbar_img + i));

            __m128i lo = _mm_unpacklo_epi8(pixels, zero);
            __m128i hi = _mm_unpackhi_epi8(pixels, zero);

            lo = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(lo, mult_vec), 8), add_vec);
            hi = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(hi, mult_vec), 8), add_vec);

            __m128i result = _mm_packus_epi16(lo, hi);
            _mm_storeu_si128((__m128i *)(taskbar_img + i), result);
        }
    } else {
        __m128i mult_vec = _mm_set1_epi16(205);
        __m128i add_vec  = _mm_set1_epi16(45);
        __m128i zero     = _mm_setzero_si128();

        for (; i <= total_pixels - 4; i += 4) {
            __m128i pixels = _mm_loadu_si128((__m128i *)(taskbar_img + i));

            __m128i lo = _mm_unpacklo_epi8(pixels, zero);
            __m128i hi = _mm_unpackhi_epi8(pixels, zero);

            lo = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(lo, mult_vec), 8), add_vec);
            hi = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(hi, mult_vec), 8), add_vec);

            __m128i result = _mm_packus_epi16(lo, hi);
            _mm_storeu_si128((__m128i *)(taskbar_img + i), result);
        }
    }

    for (; i < total_pixels; i++) {
        uint32_t pixel = taskbar_img[i];
        uint32_t r = (pixel >> 16) & 0xFF;
        uint32_t g = (pixel >> 8) & 0xFF;
        uint32_t b = pixel & 0xFF;

        if (is_dark_mode) {
            r = ((r * 166) >> 8) + 24;
            g = ((g * 166) >> 8) + 24;
            b = ((b * 166) >> 8) + 24;
        } else {
            r = ((r * 205) >> 8) + 45;
            g = ((g * 205) >> 8) + 45;
            b = ((b * 205) >> 8) + 45;
        }

        taskbar_img[i] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
    }
}

extern "C" int main() {
    klog("Starting window program");

    WinControl *ptr = (WinControl *)r0ui_call(R0UICall::OpenWindow);

    klog("Screen dims are %ix%i", ptr->scrnw, ptr->scrnh);

    int w, h;
    uint32_t *ptrx = load_png("A:/WALLP~1.PNG", &w, &h);
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
    apply_acrylic_finish(taskbar_img, ptr->scrnw, 48, 0);

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