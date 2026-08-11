#include <string.h>
#include <kalloc.h>
#include <klibkrnl.h>
#include "stb_impl.hpp"

struct WinControl {
    int32_t x, y;
    uint32_t width, height;
    uint32_t z_index;
    uint32_t flags;
    uint32_t *usr_pix_buf;
    uint8_t reserved[4064];
} __attribute__((packed));

extern "C" int main() {
    klog("Starting window program");

    WinControl *ptr = (WinControl *)ioctl("R0UI/", 1); // Open window

    klog("Window is x: %d, y: %d, w: %d, h: %d", ptr->x, ptr->y, ptr->width, ptr->height);

    int w, h;
    uint32_t *ptrx = load_png("A:/square.png", &w, &h);

    memcpy(ptr->usr_pix_buf, ptrx, ptr->width * ptr->height * 4);

    ioctl("R0UI/", 0); // redraw

    ptr->width = 500;
    ptr->height = 500;

    for (;;);

    return 0;
}