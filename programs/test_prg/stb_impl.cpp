#include <string.h>
#include <klibkrnl.h>
#include <kalloc.h>

#define STBI_NO_STDIO
#define STBI_NO_FAILURE_STRINGS
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define _MM_MALLOC_H_INCLUDED

#define STBI_MALLOC(sz)        malloc(sz)
#define STBI_REALLOC(p, newsz) realloc(p, newsz)
#define STBI_FREE(p)           free(p)

#define STBI_ABS(x)            abs(x)

#define STBI_ASSERT(x)         ((void)0)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_img.h"

uint32_t *load_png(const char *path, int *out_width, int *out_height) {
    size_t fd = kopen(path);
    size_t file_size = ksize(fd);
    if (file_size == 0) {
        klog("Invalid file");
        return nullptr;
    }
    
    uint8_t *charbuf = (uint8_t *)malloc(file_size);
    if (!charbuf) {
        kclose(fd);
        return nullptr;
    }

    kread(fd, 0, charbuf, file_size);
    kclose(fd); // Done reading the file

    int width = 0;
    int height = 0;
    int channels_in_file = 0;

    // 4 forces RGBA output (8 bits per channel = 32-bit pixel)
    uint32_t *pixels = (uint32_t *)stbi_load_from_memory(
        charbuf, 
        (int)file_size, 
        &width, 
        &height, 
        &channels_in_file, 
        4
    );

    // Free the raw encoded PNG buffer
    free(charbuf);

    if (!pixels) {
        klog("Failed to decode PNG");
        return nullptr;
    }

    if (out_width) *out_width = width;
    if (out_height) *out_height = height;

    return pixels;
}