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
    uint32_t scrnw, scrnh;
    uint8_t reserved[4056];
} __attribute__((packed));

extern "C" int main() {
    klog("Starting window program");

    volatile WinControl *ptr = (WinControl *)ioctl("R0UI/", 1); // Open window

    klog("Screen dims are %ix%i", ptr->scrnw, ptr->scrnh);

    int w, h;
    uint32_t *ptrx = load_png("A:/WALLPA~1.PNG", &w, &h);
    klog("Loaded image");
    uint32_t *nbuf = resize_image(ptrx, w, h, 1280, 800);
    klog("Resized image");
    free(ptrx);

    ioctl("R0UI/", 0); // redraw

    #define TSK_BR_WDTH 50
    ptr->x = 0;
    ptr->y = 0;
    ptr->width = ptr->scrnw;
    ptr->height = ptr->scrnh;

    ioctl("R0UI/", 2);

    asm volatile("" ::: "memory");

    memcpy(ptr->usr_pix_buf, nbuf, 1280 * 720 * 4);
    free(nbuf);
    
    for (;;);

    return 0;
}