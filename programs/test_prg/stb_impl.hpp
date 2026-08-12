#pragma once
#include <stdint.h>

uint32_t *load_png(const char *path, int *out_width, int *out_height);
uint32_t *resize_image(const uint32_t *src_pixels, int src_w, int src_h, int target_w, int target_h);