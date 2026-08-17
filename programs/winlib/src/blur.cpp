#include <klibkrnl.h>
#include <stdint.h>
#include <kalloc.h>

typedef struct {
    uint32_t r;
    uint32_t g;
    uint32_t b;
    uint32_t a;
} accum_t;

static inline void box_blur_h(uint32_t *src, uint32_t *dst, int w, int h, int radius) {
    float inv_window = 1.0f / (radius * 2 + 1);

    for (int y = 0; y < h; y++) {
        accum_t acc = {0, 0, 0, 0};
        uint32_t *src_row = src + y * w;
        uint32_t *dst_row = dst + y * w;

        for (int i = -radius; i <= radius; i++) {
            int x = i < 0 ? 0 : (i >= w ? w - 1 : i);
            uint32_t pixel = src_row[x];
            acc.a += (pixel >> 24) & 0xFF;
            acc.r += (pixel >> 16) & 0xFF;
            acc.g += (pixel >> 8) & 0xFF;
            acc.b += pixel & 0xFF;
        }

        for (int x = 0; x < w; x++) {
            uint8_t a = (uint8_t)(acc.a * inv_window);
            uint8_t r = (uint8_t)(acc.r * inv_window);
            uint8_t g = (uint8_t)(acc.g * inv_window);
            uint8_t b = (uint8_t)(acc.b * inv_window);

            dst_row[x] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;

            int x_out = x - radius < 0 ? 0 : x - radius;
            int x_in  = x + radius + 1 >= w ? w - 1 : x + radius + 1;

            uint32_t p_out = src_row[x_out];
            uint32_t p_in  = src_row[x_in];

            acc.a += ((p_in >> 24) & 0xFF) - ((p_out >> 24) & 0xFF);
            acc.r += ((p_in >> 16) & 0xFF) - ((p_out >> 16) & 0xFF);
            acc.g += ((p_in >> 8)  & 0xFF) - ((p_out >> 8)  & 0xFF);
            acc.b += (p_in & 0xFF)         - (p_out & 0xFF);
        }
    }
}

static inline void box_blur_v(uint32_t *src, uint32_t *dst, int w, int h, int radius) {
    float inv_window = 1.0f / (radius * 2 + 1);

    for (int x = 0; x < w; x++) {
        accum_t acc = {0, 0, 0, 0};

        for (int i = -radius; i <= radius; i++) {
            int y = i < 0 ? 0 : (i >= h ? h - 1 : i);
            uint32_t pixel = src[y * w + x];
            acc.a += (pixel >> 24) & 0xFF;
            acc.r += (pixel >> 16) & 0xFF;
            acc.g += (pixel >> 8) & 0xFF;
            acc.b += pixel & 0xFF;
        }

        for (int y = 0; y < h; y++) {
            uint8_t a = (uint8_t)(acc.a * inv_window);
            uint8_t r = (uint8_t)(acc.r * inv_window);
            uint8_t g = (uint8_t)(acc.g * inv_window);
            uint8_t b = (uint8_t)(acc.b * inv_window);

            dst[y * w + x] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;

            int y_out = y - radius < 0 ? 0 : y - radius;
            int y_in  = y + radius + 1 >= h ? h - 1 : y + radius + 1;

            uint32_t p_out = src[y_out * w + x];
            uint32_t p_in  = src[y_in * w + x];

            acc.a += ((p_in >> 24) & 0xFF) - ((p_out >> 24) & 0xFF);
            acc.r += ((p_in >> 16) & 0xFF) - ((p_out >> 16) & 0xFF);
            acc.g += ((p_in >> 8)  & 0xFF) - ((p_out >> 8)  & 0xFF);
            acc.b += (p_in & 0xFF)         - (p_out & 0xFF);
        }
    }
}

void apply_blur(uint32_t *taskbar_img, int width, int height, int radius, int passes) {
    uint32_t *temp_buf = (uint32_t *)malloc(width * height * sizeof(uint32_t));
    if (!temp_buf) return;

    klog("Applying blur!");
    for (int i = 0; i < passes; i++) {
        box_blur_h(taskbar_img, temp_buf, width, height, radius);
        box_blur_v(temp_buf, taskbar_img, width, height, radius);
    }

    free(temp_buf);
}