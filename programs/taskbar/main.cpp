#include <winlib/r0ui_protocol.hpp>

// This is a reference.
//     uint32_t *taskbar_img = (uint32_t *)malloc(ptr->scrnw * 48 * 4); // taskbar is 48 pixels
//     memcpy(taskbar_img, &nbuf[ptr->scrnw * ptr->scrnh - (ptr->scrnw * 48)], ptr->scrnw * 48 * 4);
//     // memset32(taskbar_img, 0x00, ptr->scrnw * 48);
//     apply_blur(taskbar_img, ptr->scrnw, 48, 8, 2);
//     apply_acrylic_finish(taskbar_img, ptr->scrnw, 48, 1);

// using R0UI::Event;
// using R0UI::EventType;
using R0UI::WinControl;
using R0UI::R0UICall;

extern "C" int main() {
    klog("Starting taskbar program");

    WinControl *ptr = (WinControl *)r0ui_call(R0UICall::OpenWindow, (uint64_t)"Taskbar");
    (void)ptr;
    for (;;);

    return 0;
}