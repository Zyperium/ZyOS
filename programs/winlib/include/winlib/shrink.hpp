#pragma once
#define _MM_MALLOC_H_INCLUDED
#include <immintrin.h>
#include <stdint.h>

namespace Config {
    constexpr uint32_t HoverCapsuleColor = 0x18FFFFFF;
    constexpr uint32_t HoverShadowColorOuter = 0x1A000000;
    constexpr uint32_t HoverShadowColorInner = 0x12000000;
}

float ease_out_cubic(float t);
float ease_in_out_cubic(float t);
void premultiply_words_avx2(__m256i px, __m256i &lo, __m256i &hi);
void scale_and_memcpy32_alpha_avx2(uint32_t *dest, int dest_pitch, int dst_w, int dst_h,
                                    const uint32_t *src, int src_w, int src_h);
void scale_and_memcpy32_alpha_avx2_bilinear(uint32_t *dest, int dest_pitch, int dst_w, int dst_h,
                                             const uint32_t *src, int src_w, int src_h);
void draw_rounded_rect_avx2(uint32_t *buf, int buf_w, int x, int y, int w, int h, int r, uint32_t argb);
void draw_hover_capsule(uint32_t *buf, int buf_w, int x, int y, int w, int h, int r);