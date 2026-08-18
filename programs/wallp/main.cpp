#include <winlib/stb_impl.hpp>
#include <winlib/blur.hpp>
#include <winlib/shrink.hpp>
#include <winlib/r0ui_protocol.hpp>
#include <winlib/acryl.hpp>
#include <kalloc.h>
#include <klibkrnl.h>
#include <string.h>
#include <ksyscall.h>
#define _MM_MALLOC_H_INCLUDED
#include <immintrin.h>

namespace Config {
    constexpr int TaskbarHeight = 48;
    constexpr int StartMenuWidth = 300;
    constexpr int StartMenuHeight = 300;
    constexpr int ItemCount = 6;
    constexpr int ItemWidth = 40;
    constexpr int ItemHeight = 40;
    constexpr int ItemSpacing = 45;
    constexpr int CornerRadius = 5;

    constexpr int BlurRadius = 16;
    constexpr int BlurPasses = 3;
    constexpr int AcrylicIntensity = 1;

    constexpr int AnimationSteps = 10;
    constexpr int FrameDelayUs = 80;

    constexpr float HoverScaleMin = 0.75f;
    constexpr float HoverScaleMax = 1.00f;
    constexpr float PressScaleMin = 0.82f;

    constexpr float IconScaleNormal = 1.00f;
    constexpr float IconScalePressed = 0.85f;
}

struct TaskbarItem {
    int x;
    int y;
    int w;
    int h;
    uint32_t *icon;
    const char *path;
    int icon_w;
    int icon_h;
};

void usleep(size_t time) { // bad sleep implement until I add sleeping.
    return;
    for (auto i{0ull}; i < time; ++i) {
        yield();
    }
}

using R0UI::Event;
using R0UI::EventType;
using R0UI::WinControl;
using R0UI::R0UICall;

void render_frame(WinControl *taskbar, uint32_t *clean_bg, TaskbarItem *items, int item_count, int active_idx, float capsule_scale, float active_icon_scale) {
    memcpy(taskbar->usr_pix_buf, clean_bg, taskbar->scrnw * Config::TaskbarHeight * 4);

    if (active_idx >= 0 && active_idx < item_count && capsule_scale > 0.0f) {
        TaskbarItem *active = &items[active_idx];
        int cur_w = (int)((float)active->w * capsule_scale);
        int cur_h = (int)((float)active->h * capsule_scale);
        int cur_x = active->x + (active->w - cur_w) / 2;
        int cur_y = active->y + (active->h - cur_h) / 2;

        draw_hover_capsule(taskbar->usr_pix_buf, taskbar->scrnw, cur_x, cur_y, cur_w, cur_h, Config::CornerRadius);
    }

    for (int i = 0; i < item_count; ++i) {
        float current_scale = (i == active_idx) ? active_icon_scale : Config::IconScaleNormal;

        int target_icon_w = (int)((float)items[i].icon_w * current_scale);
        int target_icon_h = (int)((float)items[i].icon_h * current_scale);

        int icon_off_x = items[i].x + (items[i].w - target_icon_w) / 2;
        int icon_off_y = items[i].y + (items[i].h - target_icon_h) / 2;

        uint32_t *dest_ptr = &taskbar->usr_pix_buf[icon_off_x + (icon_off_y * taskbar->width)];

        if (target_icon_w == items[i].icon_w && target_icon_h == items[i].icon_h) {
            for (int row = 0; row < items[i].icon_h; ++row) {
                memcpy32_alpha(&taskbar->usr_pix_buf[icon_off_x + ((icon_off_y + row) * taskbar->width)], &items[i].icon[row * items[i].icon_w], items[i].icon_w);
            }
        } else {
            scale_and_memcpy32_alpha_avx2_bilinear(dest_ptr, taskbar->width, target_icon_w, target_icon_h, items[i].icon, items[i].icon_w, items[i].icon_h);
        }
    }

    r0ui_call(R0UICall::RedrawMyWindows, 0);
}

extern "C" int main() {
    klog("Starting wallpaper program");

    WinControl *taskbar = (WinControl *)r0ui_call(R0UICall::OpenWindow, (uint64_t)"Taskbar");

    klog("Screen dims are %ix%i", taskbar->scrnw, taskbar->scrnh);

    taskbar->x = 0;
    taskbar->y = taskbar->scrnh - Config::TaskbarHeight;
    taskbar->width = taskbar->scrnw;
    taskbar->height = Config::TaskbarHeight;
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
    uint32_t *ptrx = load_png("A:/WP.JPG", &w, &h);
    klog("Loaded image");
    uint32_t *nbuf = resize_image(ptrx, w, h, taskbar->scrnw, taskbar->scrnh);
    klog("Resized image");
    free(ptrx);

    asm volatile("" ::: "memory");
    memcpy(wallpaper, nbuf, taskbar->scrnw * taskbar->scrnh * 4);
    free(nbuf);

    r0ui_call(R0UICall::Redraw, 0);

    memcpy(taskbar->usr_pix_buf, &wallpaper[taskbar->scrnw * taskbar->scrnh - (taskbar->scrnw * Config::TaskbarHeight)], taskbar->scrnw * Config::TaskbarHeight * 4);
    apply_blur(taskbar->usr_pix_buf, taskbar->scrnw, Config::TaskbarHeight, Config::BlurRadius, Config::BlurPasses);
    apply_acrylic_finish(taskbar->usr_pix_buf, taskbar->scrnw, Config::TaskbarHeight, Config::AcrylicIntensity);

    uint32_t *clean_bg = (uint32_t *)malloc(taskbar->scrnw * Config::TaskbarHeight * 4);
    memcpy(clean_bg, taskbar->usr_pix_buf, taskbar->scrnw * Config::TaskbarHeight * 4);

    TaskbarItem items[Config::ItemCount];

    items[0].icon = load_png("A:/SYSTEM/MAIN.PNG", &items[0].icon_w, &items[0].icon_h);
    items[0].w = Config::ItemWidth;
    items[0].h = Config::ItemHeight;
    items[0].x = (Config::TaskbarHeight - items[0].w) / 2;
    items[0].y = (Config::TaskbarHeight - items[0].h) / 2;
    items[0].path = "none";

    WinControl *start_menu = (WinControl *)r0ui_call(R0UICall::OpenWindow, (uint64_t)"Start Menu");

    int start_menu_open_y = start_menu->scrnh - (taskbar->height + items[0].x + Config::StartMenuHeight);
    int start_menu_closed_y = start_menu->scrnh;
    bool start_menu_open = false;

    start_menu->x = items[0].x;
    start_menu->y = start_menu_closed_y;
    start_menu->width = Config::StartMenuWidth;
    start_menu->height = Config::StartMenuHeight;

    r0ui_call(R0UICall::PushRef, 0);

    for (auto i{0uz}; i < start_menu->height; ++i) {
        memcpy(&start_menu->usr_pix_buf[i * start_menu->width], &wallpaper[start_menu->x + ((start_menu_open_y + i) * start_menu->scrnw)], start_menu->width * 4);
    }
    apply_blur(start_menu->usr_pix_buf, start_menu->width, Config::StartMenuHeight, Config::BlurRadius, Config::BlurPasses);
    apply_acrylic_finish(start_menu->usr_pix_buf, start_menu->width, Config::StartMenuHeight, Config::AcrylicIntensity);

    items[1].icon = load_png("A:/SYSTEM/MAGNIF.PNG", &items[1].icon_w, &items[1].icon_h);
    items[1].w = Config::ItemWidth;
    items[1].h = Config::ItemHeight;
    items[1].x = items[0].x + Config::ItemSpacing;
    items[1].y = (Config::TaskbarHeight - items[1].h) / 2;
    items[1].path = "none";

    items[2].icon = load_png("A:/SYSTEM/SETTIN.PNG", &items[2].icon_w, &items[2].icon_h);
    items[2].w = Config::ItemWidth;
    items[2].h = Config::ItemHeight;
    items[2].x = items[1].x + Config::ItemSpacing;
    items[2].y = (Config::TaskbarHeight - items[2].h) / 2;
    items[2].path = "A:/USER/TASKW.ZYX";

    items[3].icon = load_png("A:/SYSTEM/TERM.PNG", &items[3].icon_w, &items[3].icon_h);
    items[3].w = Config::ItemWidth;
    items[3].h = Config::ItemHeight;
    items[3].x = items[2].x + Config::ItemSpacing;
    items[3].y = (Config::TaskbarHeight - items[3].h) / 2;
    items[3].path = "A:/USER/TASKW.ZYX";

    items[4].icon = load_png("A:/SYSTEM/WATCHER.PNG", &items[4].icon_w, &items[4].icon_h);
    items[4].w = Config::ItemWidth;
    items[4].h = Config::ItemHeight;
    items[4].x = items[3].x + Config::ItemSpacing;
    items[4].y = (Config::TaskbarHeight - items[4].h) / 2;
    items[4].path = "A:/USER/TASKW.ZYX";

    items[5].icon = load_png("A:/SYSTEM/NOTES.PNG", &items[5].icon_w, &items[5].icon_h);
    items[5].w = Config::ItemWidth;
    items[5].h = Config::ItemHeight;
    items[5].x = items[4].x + Config::ItemSpacing;
    items[5].y = (Config::TaskbarHeight - items[5].h) / 2;
    items[5].path = "A:/USER/TASKW.ZYX";

    r0ui_call(R0UICall::PinWindow, (uint64_t)"Taskbar");
    r0ui_call(R0UICall::PinWindow, (uint64_t)"Start Menu");

    int current_hover_idx = -1;

    render_frame(taskbar, clean_bg, items, Config::ItemCount, -1, 0.0f, Config::IconScaleNormal);

    for (;;) {
        uint32_t h_idx = taskbar->events.head;
        uint32_t t_idx = taskbar->events.tail;

        while (t_idx != h_idx) {
            Event ev = taskbar->events.ring[t_idx];
            asm volatile("" ::: "memory");

            switch (ev.type) {
                case EventType::MouseMove: {
                    int mx = taskbar->mouse_pos.x;
                    int my = taskbar->mouse_pos.y;
                    int new_hover = -1;

                    for (int i = 0; i < Config::ItemCount; ++i) {
                        if (mx >= items[i].x && mx < items[i].x + items[i].w &&
                            my >= items[i].y && my < items[i].y + items[i].h) {
                            new_hover = i;
                            break;
                        }
                    }

                    if (new_hover != current_hover_idx) {
                        if (current_hover_idx != -1) {
                            for (int step = Config::AnimationSteps; step >= 1; --step) {
                                float progress = (float)step / (float)Config::AnimationSteps;
                                float eased_progress = ease_out_cubic(progress);

                                float cap_scale = Config::HoverScaleMin + (Config::HoverScaleMax - Config::HoverScaleMin) * eased_progress;

                                render_frame(taskbar, clean_bg, items, Config::ItemCount, current_hover_idx, cap_scale, Config::IconScaleNormal);
                                usleep(Config::FrameDelayUs);
                            }
                            render_frame(taskbar, clean_bg, items, Config::ItemCount, -1, 0.0f, Config::IconScaleNormal);
                        }

                        current_hover_idx = new_hover;

                        if (current_hover_idx != -1) {
                            for (int step = 1; step <= Config::AnimationSteps; ++step) {
                                float progress = (float)step / (float)Config::AnimationSteps;
                                float eased_progress = ease_out_cubic(progress);

                                float cap_scale = Config::HoverScaleMin + (Config::HoverScaleMax - Config::HoverScaleMin) * eased_progress;

                                render_frame(taskbar, clean_bg, items, Config::ItemCount, current_hover_idx, cap_scale, Config::IconScaleNormal);
                                usleep(Config::FrameDelayUs);
                            }
                        }
                    }
                    break;
                }
                case EventType::MouseDown: {
                    if (current_hover_idx != -1) {
                        for (int step = 1; step <= Config::AnimationSteps; ++step) {
                            float progress = (float)step / (float)Config::AnimationSteps;
                            float eased_progress = ease_in_out_cubic(progress);

                            float cap_scale = Config::HoverScaleMax - (Config::HoverScaleMax - Config::PressScaleMin) * eased_progress;
                            float icon_scale = Config::IconScaleNormal - (Config::IconScaleNormal - Config::IconScalePressed) * eased_progress;

                            render_frame(taskbar, clean_bg, items, Config::ItemCount, current_hover_idx, cap_scale, icon_scale);
                            usleep(Config::FrameDelayUs);
                        }

                        if (current_hover_idx > 1) {
                            syscall(21, (uint64_t)items[current_hover_idx].path);
                            klog("Launched task %s", items[current_hover_idx].path);
                        }
                    }
                    break;
                }
                case EventType::MouseUp:
                    if (current_hover_idx != -1) {
                        for (int step = 1; step <= Config::AnimationSteps; ++step) {
                            float progress = (float)step / (float)Config::AnimationSteps;
                            float eased_progress = ease_out_cubic(progress);

                            float cap_scale = Config::PressScaleMin + (Config::HoverScaleMax - Config::PressScaleMin) * eased_progress;
                            float icon_scale = Config::IconScalePressed + (Config::IconScaleNormal - Config::IconScalePressed) * eased_progress;

                            render_frame(taskbar, clean_bg, items, Config::ItemCount, current_hover_idx, cap_scale, icon_scale);
                            usleep(Config::FrameDelayUs);
                        }

                        if (current_hover_idx == 0) {
                            start_menu_open = !start_menu_open;

                            int start_y = start_menu->y;
                            int target_y = start_menu_open ? start_menu_open_y : start_menu_closed_y;

                            for (int step = 1; step <= Config::AnimationSteps; ++step) {
                                float progress = (float)step / (float)Config::AnimationSteps;
                                float eased_progress = start_menu_open ? ease_out_cubic(progress) : ease_in_out_cubic(progress);

                                start_menu->y = start_y + (int)((float)(target_y - start_y) * eased_progress);

                                render_frame(taskbar, clean_bg, items, Config::ItemCount, current_hover_idx, Config::HoverScaleMax, Config::IconScaleNormal);
                                usleep(Config::FrameDelayUs);
                            }
                            start_menu->y = target_y;
                            r0ui_call(R0UICall::RedrawMyWindows, 0);
                        }
                    }
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