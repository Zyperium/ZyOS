#include <string.h>
#include <klibkrnl.h>
#include <kalloc.h>
#define _MM_MALLOC_H_INCLUDED
#define STBI_NO_STDIO
#include <winlib/stb_img.h>

#include <winlib/stb_image_resize2.h>

uint32_t *load_png(const char *path, int *out_width, int *out_height) {
    size_t fd = kopen(path);
    size_t file_size = ksize(fd);
    
    size_t alloc_size = file_size + 32;
    uint8_t *charbuf = (uint8_t *)malloc(alloc_size);

    volatile uint8_t *ptr = (volatile uint8_t *)charbuf;
    for (size_t i = 0; i < file_size; i += 4096) {
        uint8_t dummy = ptr[i]; 
        (void)dummy;
    }
    klog("All pages intact!");

    if (!charbuf) {
        kclose(fd);
        return nullptr;
    }

    memset(charbuf, 0, alloc_size);

    ptr = (volatile uint8_t *)charbuf;
    for (size_t i = 0; i < file_size; i += 4096) {
        uint8_t dummy = ptr[i]; 
        (void)dummy;
    }
    size_t bytes_read = kread(fd, 0, charbuf, file_size);
    
    kclose(fd);

    if (bytes_read < file_size) {
        free(charbuf);
        return nullptr;
    }

    int width = 0;
    int height = 0;
    int channels_in_file = 0;

    ptr = (volatile uint8_t *)charbuf;
    for (size_t i = 0; i < file_size; i += 4096) {
        uint8_t dummy = ptr[i]; 
        (void)dummy;
    }

    uint32_t *pixels = (uint32_t *)stbi_load_from_memory(
        charbuf, 
        (int)file_size, 
        &width, 
        &height, 
        &channels_in_file, 
        4
    );

    free(charbuf);

    if (!pixels) {
        klog("Failed to decode PNG");
        return nullptr;
    }

    for (int y = 0; y < height; y++) {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + (y * width * 4));
        
        for (int x = 0; x < width; x++) {
            uint32_t p = row[x];

            uint32_t r = (p >> 0)  & 0xFF;
            uint32_t g = (p >> 8)  & 0xFF;
            uint32_t b = (p >> 16) & 0xFF;
            uint32_t a = (p >> 24) & 0xFF;

            row[x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    if (out_width) *out_width = width;
    if (out_height) *out_height = height;

    return pixels;
}

uint32_t *resize_image(const uint32_t *src_pixels, int src_w, int src_h, int target_w, int target_h) {
    if (!src_pixels || target_w <= 0 || target_h <= 0) {
        return nullptr;
    }

    uint32_t *resized_pixels = (uint32_t *)malloc((size_t)target_w * (size_t)target_h * sizeof(uint32_t));
    if (!resized_pixels) {
        return nullptr;
    }

    unsigned char *result = stbir_resize_uint8_srgb(
        (const unsigned char *)src_pixels, src_w, src_h, 0,
        (unsigned char *)resized_pixels, target_w, target_h, 0,
        STBIR_RGBA
    );

    if (!result) {
        free(resized_pixels);
        return nullptr;
    }

    return resized_pixels;
}