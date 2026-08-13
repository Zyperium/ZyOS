#pragma once
#include <stdint.h>
#include <stddef.h>

void apply_acrylic_finish(uint32_t *img, int width, int height, int is_dark_mode); // is_dark_mode will probably be driven by a system state later.
void memcpy32_alpha(uint32_t *dest, const uint32_t *src, size_t count);