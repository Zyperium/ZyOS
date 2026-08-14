#include <klibkrnl.h>
#include <stdint.h>
#include <winlib/shrink.hpp>
#include <stddef.h>
#define _MM_MALLOC_H_INCLUDED
#include <immintrin.h>

static inline int32_t imin(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t imax(int32_t a, int32_t b) { return a > b ? a : b; }

float ease_out_cubic(float t) {
    float f = t - 1.0f;
    return f * f * f + 1.0f;
}

float ease_in_out_cubic(float t) {
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    }
    float f = (2.0f * t) - 2.0f;
    return 0.5f * f * f * f + 1.0f;
}

void premultiply_words_avx2(__m256i px, __m256i &lo, __m256i &hi) {
    const __m256i zero = _mm256_setzero_si256();
    const __m256i alpha_bcast_shuf = _mm256_setr_epi8(
        3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15,
        3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15);

    __m256i a_bytes = _mm256_shuffle_epi8(px, alpha_bcast_shuf);
    __m256i a_lo = _mm256_unpacklo_epi8(a_bytes, zero);
    __m256i a_hi = _mm256_unpackhi_epi8(a_bytes, zero);
    __m256i px_lo = _mm256_unpacklo_epi8(px, zero);
    __m256i px_hi = _mm256_unpackhi_epi8(px, zero);

    lo = _mm256_srli_epi16(_mm256_mullo_epi16(px_lo, a_lo), 8);
    hi = _mm256_srli_epi16(_mm256_mullo_epi16(px_hi, a_hi), 8);

    lo = _mm256_blend_epi16(lo, a_lo, 0x88);
    hi = _mm256_blend_epi16(hi, a_hi, 0x88);
}


static inline __m256i premultiply_avx2(__m256i px) {
    __m256i lo, hi;
    premultiply_words_avx2(px, lo, hi);
    return _mm256_packus_epi16(lo, hi);
}


static inline __m256i composite_premul_avx2(__m256i bg, __m256i pm) {
    const __m256i zero = _mm256_setzero_si256();
    const __m256i alpha_bcast_shuf = _mm256_setr_epi8(
        3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15,
        3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15);
    const __m256i k255_16 = _mm256_set1_epi16(255);

    __m256i alpha32 = _mm256_srli_epi32(pm, 24);
    __m256i is_zero_mask = _mm256_cmpeq_epi32(alpha32, zero);

    __m256i a_bytes = _mm256_shuffle_epi8(pm, alpha_bcast_shuf);
    __m256i a_lo = _mm256_unpacklo_epi8(a_bytes, zero);
    __m256i a_hi = _mm256_unpackhi_epi8(a_bytes, zero);

    __m256i pm_lo = _mm256_unpacklo_epi8(pm, zero);
    __m256i pm_hi = _mm256_unpackhi_epi8(pm, zero);
    __m256i bg_lo = _mm256_unpacklo_epi8(bg, zero);
    __m256i bg_hi = _mm256_unpackhi_epi8(bg, zero);

    __m256i inv_a_lo = _mm256_sub_epi16(k255_16, a_lo);
    __m256i inv_a_hi = _mm256_sub_epi16(k255_16, a_hi);

    __m256i blend_lo = _mm256_add_epi16(
        _mm256_srli_epi16(_mm256_mullo_epi16(bg_lo, inv_a_lo), 8), pm_lo);
    __m256i blend_hi = _mm256_add_epi16(
        _mm256_srli_epi16(_mm256_mullo_epi16(bg_hi, inv_a_hi), 8), pm_hi);

    __m256i result = _mm256_packus_epi16(blend_lo, blend_hi);
    result = _mm256_blendv_epi8(result, bg, is_zero_mask);
    return result;
}

void scale_and_memcpy32_alpha_avx2(uint32_t *dest, int dest_pitch, int dst_w, int dst_h,
                                    const uint32_t *src, int src_w, int src_h) {
    if (dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0) return;

    uint32_t x_ratio = (uint32_t)(((uint64_t)src_w << 16) / dst_w);
    uint32_t y_ratio = (uint32_t)(((uint64_t)src_h << 16) / dst_h);

    alignas(32) int32_t x_indices[8 + dst_w];
    for (int x = 0; x < dst_w; ++x)
        x_indices[x] = (int32_t)((x * x_ratio) >> 16);

    for (int dy = 0; dy < dst_h; ++dy) {
        uint32_t sy = (dy * y_ratio) >> 16;
        const uint32_t *src_row = &src[(size_t)sy * src_w];
        uint32_t *dst_row = &dest[(size_t)dy * dest_pitch];

        int dx = 0;
        for (; dx <= dst_w - 8; dx += 8) {
            __m256i idx = _mm256_loadu_si256((const __m256i *)&x_indices[dx]);
            __m256i fg  = _mm256_i32gather_epi32((const int *)src_row, idx, 4);

            __m256i alpha32 = _mm256_srli_epi32(fg, 24);
            if (_mm256_testc_si256(_mm256_cmpeq_epi32(alpha32, _mm256_setzero_si256()),
                                    _mm256_set1_epi32(-1)))
                continue;

            __m256i bg = _mm256_loadu_si256((const __m256i *)&dst_row[dx]);
            __m256i pm = premultiply_avx2(fg);
            _mm256_storeu_si256((__m256i *)&dst_row[dx], composite_premul_avx2(bg, pm));
        }

        for (; dx < dst_w; ++dx) {
            uint32_t pixel = src_row[x_indices[dx]];
            uint8_t a = (pixel >> 24) & 0xFF;
            if (a == 0) continue;
            if (a == 255) { dst_row[dx] = pixel; continue; }
            uint32_t bg = dst_row[dx];
            uint8_t inv_a = 255 - a;
            uint8_t fg_r = (pixel >> 16) & 0xFF, fg_g = (pixel >> 8) & 0xFF, fg_b = pixel & 0xFF;
            uint8_t pr = (uint8_t)(((uint32_t)fg_r * a) / 255);
            uint8_t pg = (uint8_t)(((uint32_t)fg_g * a) / 255);
            uint8_t pb = (uint8_t)(((uint32_t)fg_b * a) / 255);
            uint8_t b = (uint8_t)((((bg & 0xFF) * inv_a) >> 8) + pb);
            uint8_t g = (uint8_t)(((((bg >> 8) & 0xFF) * inv_a) >> 8) + pg);
            uint8_t r = (uint8_t)(((((bg >> 16) & 0xFF) * inv_a) >> 8) + pr);
            uint8_t out_a = (uint8_t)(((((bg >> 24) & 0xFF) * inv_a) >> 8) + a);
            dst_row[dx] = ((uint32_t)out_a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }
}

void scale_and_memcpy32_alpha_avx2_bilinear(uint32_t *dest, int dest_pitch, int dst_w, int dst_h,
                                             const uint32_t *src, int src_w, int src_h) {
    if (dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0) return;

    uint32_t x_ratio = (uint32_t)(((uint64_t)src_w << 16) / dst_w);
    uint32_t y_ratio = (uint32_t)(((uint64_t)src_h << 16) / dst_h);

    alignas(32) int32_t  x0[8 + dst_w];
    alignas(32) int32_t  x1[8 + dst_w];
    alignas(32) uint32_t fxb[8 + dst_w];
    alignas(32) uint32_t invfxb[8 + dst_w];

    for (int x = 0; x < dst_w; ++x) {
        uint32_t sx_fixed = x * x_ratio;
        int32_t sx0 = (int32_t)(sx_fixed >> 16);
        uint8_t fx = (uint8_t)((sx_fixed >> 8) & 0xFF);
        x0[x] = imax(0, imin(sx0, src_w - 1));
        x1[x] = imin(x0[x] + 1, src_w - 1);
        fxb[x]    = (uint32_t)fx * 0x01010101u;
        invfxb[x] = (uint32_t)(255 - fx) * 0x01010101u;
    }

    for (int dy = 0; dy < dst_h; ++dy) {
        uint32_t sy_fixed = dy * y_ratio;
        int32_t sy0 = (int32_t)(sy_fixed >> 16);
        uint8_t fy = (uint8_t)((sy_fixed >> 8) & 0xFF);
        sy0 = imax(0, imin(sy0, src_h - 1));
        int32_t sy1 = imin(sy0 + 1, src_h - 1);
        uint8_t inv_fy = 255 - fy;

        const uint32_t *row0 = &src[(size_t)sy0 * src_w];
        const uint32_t *row1 = &src[(size_t)sy1 * src_w];
        uint32_t *dst_row = &dest[(size_t)dy * dest_pitch];

        __m256i fy_w    = _mm256_set1_epi16((int16_t)fy);
        __m256i inv_fy_w = _mm256_set1_epi16((int16_t)inv_fy);
        const __m256i zero = _mm256_setzero_si256();

        int dx = 0;
        for (; dx <= dst_w - 8; dx += 8) {
            __m256i idx0 = _mm256_loadu_si256((const __m256i *)&x0[dx]);
            __m256i idx1 = _mm256_loadu_si256((const __m256i *)&x1[dx]);

            __m256i TL = _mm256_i32gather_epi32((const int *)row0, idx0, 4);
            __m256i TR = _mm256_i32gather_epi32((const int *)row0, idx1, 4);
            __m256i BL = _mm256_i32gather_epi32((const int *)row1, idx0, 4);
            __m256i BR = _mm256_i32gather_epi32((const int *)row1, idx1, 4);

            __m256i TL_lo, TL_hi, TR_lo, TR_hi, BL_lo, BL_hi, BR_lo, BR_hi;
            premultiply_words_avx2(TL, TL_lo, TL_hi);
            premultiply_words_avx2(TR, TR_lo, TR_hi);
            premultiply_words_avx2(BL, BL_lo, BL_hi);
            premultiply_words_avx2(BR, BR_lo, BR_hi);

            __m256i fxv    = _mm256_loadu_si256((const __m256i *)&fxb[dx]);
            __m256i invfxv = _mm256_loadu_si256((const __m256i *)&invfxb[dx]);
            __m256i fx_lo    = _mm256_unpacklo_epi8(fxv, zero);
            __m256i fx_hi    = _mm256_unpackhi_epi8(fxv, zero);
            __m256i invfx_lo = _mm256_unpacklo_epi8(invfxv, zero);
            __m256i invfx_hi = _mm256_unpackhi_epi8(invfxv, zero);

            __m256i top_lo = _mm256_add_epi16(
                _mm256_srli_epi16(_mm256_mullo_epi16(TL_lo, invfx_lo), 8),
                _mm256_srli_epi16(_mm256_mullo_epi16(TR_lo, fx_lo), 8));
            __m256i top_hi = _mm256_add_epi16(
                _mm256_srli_epi16(_mm256_mullo_epi16(TL_hi, invfx_hi), 8),
                _mm256_srli_epi16(_mm256_mullo_epi16(TR_hi, fx_hi), 8));
            __m256i bot_lo = _mm256_add_epi16(
                _mm256_srli_epi16(_mm256_mullo_epi16(BL_lo, invfx_lo), 8),
                _mm256_srli_epi16(_mm256_mullo_epi16(BR_lo, fx_lo), 8));
            __m256i bot_hi = _mm256_add_epi16(
                _mm256_srli_epi16(_mm256_mullo_epi16(BL_hi, invfx_hi), 8),
                _mm256_srli_epi16(_mm256_mullo_epi16(BR_hi, fx_hi), 8));

            __m256i res_lo = _mm256_add_epi16(
                _mm256_srli_epi16(_mm256_mullo_epi16(top_lo, inv_fy_w), 8),
                _mm256_srli_epi16(_mm256_mullo_epi16(bot_lo, fy_w), 8));
            __m256i res_hi = _mm256_add_epi16(
                _mm256_srli_epi16(_mm256_mullo_epi16(top_hi, inv_fy_w), 8),
                _mm256_srli_epi16(_mm256_mullo_epi16(bot_hi, fy_w), 8));

            __m256i pm = _mm256_packus_epi16(res_lo, res_hi);
            __m256i bg = _mm256_loadu_si256((const __m256i *)&dst_row[dx]);
            _mm256_storeu_si256((__m256i *)&dst_row[dx], composite_premul_avx2(bg, pm));
        }

        for (; dx < dst_w; ++dx) {
            auto premul = [](uint32_t px, uint8_t &ao) -> uint32_t {
                uint8_t a = (px >> 24) & 0xFF;
                ao = a;
                uint8_t r = (uint8_t)(((px >> 16 & 0xFF) * a) >> 8);
                uint8_t g = (uint8_t)(((px >> 8 & 0xFF) * a) >> 8);
                uint8_t b = (uint8_t)(((px & 0xFF) * a) >> 8);
                return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            };
            uint8_t aTL, aTR, aBL, aBR;
            uint32_t TL = premul(row0[x0[dx]], aTL), TR = premul(row0[x1[dx]], aTR);
            uint32_t BL = premul(row1[x0[dx]], aBL), BR = premul(row1[x1[dx]], aBR);
            uint8_t fx = (uint8_t)fxb[dx], invfx = (uint8_t)invfxb[dx];

            auto chan = [&](int shift, uint32_t A, uint32_t B, uint32_t C, uint32_t D) -> uint8_t {
                uint8_t a_ = (A >> shift) & 0xFF, b_ = (B >> shift) & 0xFF;
                uint8_t c_ = (C >> shift) & 0xFF, d_ = (D >> shift) & 0xFF;
                uint16_t top = (uint16_t)(((uint16_t)a_ * invfx) >> 8) + (uint16_t)(((uint16_t)b_ * fx) >> 8);
                uint16_t bot = (uint16_t)(((uint16_t)c_ * invfx) >> 8) + (uint16_t)(((uint16_t)d_ * fx) >> 8);
                return (uint8_t)((((uint16_t)top * inv_fy) >> 8) + (((uint16_t)bot * fy) >> 8));
            };
            uint8_t pr = chan(16, TL, TR, BL, BR);
            uint8_t pg = chan(8, TL, TR, BL, BR);
            uint8_t pb = chan(0, TL, TR, BL, BR);
            uint8_t pa = (uint8_t)((((uint16_t)((((uint16_t)aTL*invfx)>>8)+(((uint16_t)aTR*fx)>>8)) * inv_fy) >> 8)
                                  + (((uint16_t)((((uint16_t)aBL*invfx)>>8)+(((uint16_t)aBR*fx)>>8)) * fy) >> 8));

            if (pa == 0) continue;
            uint32_t bg = dst_row[dx];
            uint8_t inv_a = 255 - pa;
            uint8_t b = (uint8_t)((((bg & 0xFF) * inv_a) >> 8) + pb);
            uint8_t g = (uint8_t)(((((bg >> 8) & 0xFF) * inv_a) >> 8) + pg);
            uint8_t r = (uint8_t)(((((bg >> 16) & 0xFF) * inv_a) >> 8) + pr);
            uint8_t out_a = (uint8_t)(((((bg >> 24) & 0xFF) * inv_a) >> 8) + pa);
            dst_row[dx] = ((uint32_t)out_a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    }
}

void blend_scanline_avx2(uint32_t *dest, int len, uint32_t pm_color, uint8_t inv_a) {
    int i = 0;
    
    __m256i zero = _mm256_setzero_si256();
    __m256i inv_a_256 = _mm256_set1_epi16(inv_a);
    __m256i color_vec = _mm256_set1_epi32(pm_color);
    
    __m256i pm_color_lo = _mm256_unpacklo_epi8(color_vec, zero);
    __m256i pm_color_hi = _mm256_unpackhi_epi8(color_vec, zero);

    for (; i <= len - 8; i += 8) {
        __m256i bg = _mm256_loadu_si256((__m256i *)&dest[i]);

        __m256i bg_lo = _mm256_unpacklo_epi8(bg, zero);
        __m256i bg_hi = _mm256_unpackhi_epi8(bg, zero);

        bg_lo = _mm256_mullo_epi16(bg_lo, inv_a_256);
        bg_hi = _mm256_mullo_epi16(bg_hi, inv_a_256);

        bg_lo = _mm256_srli_epi16(bg_lo, 8);
        bg_hi = _mm256_srli_epi16(bg_hi, 8);

        bg_lo = _mm256_add_epi16(bg_lo, pm_color_lo);
        bg_hi = _mm256_add_epi16(bg_hi, pm_color_hi);

        __m256i out = _mm256_packus_epi16(bg_lo, bg_hi);
        _mm256_storeu_si256((__m256i *)&dest[i], out);
    }

    for (; i < len; ++i) {
        uint32_t bg = dest[i];
        uint8_t b = (((bg & 0xFF) * inv_a) >> 8) + (pm_color & 0xFF);
        uint8_t g = ((((bg >> 8) & 0xFF) * inv_a) >> 8) + ((pm_color >> 8) & 0xFF);
        uint8_t r = ((((bg >> 16) & 0xFF) * inv_a) >> 8) + ((pm_color >> 16) & 0xFF);
        uint8_t a = ((((bg >> 24) & 0xFF) * inv_a) >> 8) + ((pm_color >> 24) & 0xFF);
        dest[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

void draw_rounded_rect_avx2(uint32_t *buf, int buf_w, int x, int y, int w, int h, int r, uint32_t argb) {
    uint8_t alpha = (argb >> 24) & 0xFF;
    if (alpha == 0) {
        return;
    }

    uint8_t raw_r = (argb >> 16) & 0xFF;
    uint8_t raw_g = (argb >> 8) & 0xFF;
    uint8_t raw_b = argb & 0xFF;

    uint8_t pr = (uint8_t)(((uint32_t)raw_r * alpha) / 255);
    uint8_t pg = (uint8_t)(((uint32_t)raw_g * alpha) / 255);
    uint8_t pb = (uint8_t)(((uint32_t)raw_b * alpha) / 255);
    uint32_t pm_color = ((uint32_t)alpha << 24) | ((uint32_t)pr << 16) | ((uint32_t)pg << 8) | pb;
    uint8_t inv_a = 255 - alpha;

    float r_f = (float)r;

    for (int cy = 0; cy < h; ++cy) {
        int py = y + cy;
        if (py < 0) {
            continue;
        }

        for (int cx = 0; cx < w; ++cx) {
            float dist = 0.0f;

            if (cy < r && cx < r) {
                float dx = (r_f - 0.5f) - (float)cx;
                float dy = (r_f - 0.5f) - (float)cy;
                dist = r_f - __builtin_sqrtf(dx * dx + dy * dy);
            } else if (cy < r && cx >= w - r) {
                float dx = (float)cx - ((float)w - r_f - 0.5f);
                float dy = (r_f - 0.5f) - (float)cy;
                dist = r_f - __builtin_sqrtf(dx * dx + dy * dy);
            } else if (cy >= h - r && cx < r) {
                float dx = (r_f - 0.5f) - (float)cx;
                float dy = (float)cy - ((float)h - r_f - 0.5f);
                dist = r_f - __builtin_sqrtf(dx * dx + dy * dy);
            } else if (cy >= h - r && cx >= w - r) {
                float dx = (float)cx - ((float)w - r_f - 0.5f);
                float dy = (float)cy - ((float)h - r_f - 0.5f);
                dist = r_f - __builtin_sqrtf(dx * dx + dy * dy);
            } else {
                dist = 2.0f;
            }

            if (dist < -0.5f) {
                continue;
            }

            if (dist > 0.5f && (cx + 8) < w) {
                int interior_len = 0;
                if (cy < r || cy >= h - r) {
                    interior_len = (w - r) - cx;
                } else {
                    interior_len = (w - 1) - cx;
                }

                if (interior_len >= 8) {
                    blend_scanline_avx2(&buf[py * buf_w + x + cx], interior_len, pm_color, inv_a);
                    cx += interior_len - 1;
                    continue;
                }
            }

            float alpha_cov = dist < 0.5f ? (dist + 0.5f) : 1.0f;
            uint8_t cur_a = (uint8_t)((float)alpha * alpha_cov);
            if (cur_a == 0) {
                continue;
            }

            uint8_t cur_inv_a = 255 - cur_a;
            uint8_t cur_pr = (uint8_t)(((uint32_t)raw_r * cur_a) / 255);
            uint8_t cur_pg = (uint8_t)(((uint32_t)raw_g * cur_a) / 255);
            uint8_t cur_pb = (uint8_t)(((uint32_t)raw_b * cur_a) / 255);

            int px = x + cx;
            uint32_t bg = buf[py * buf_w + px];

            uint8_t nb = (uint8_t)((((bg & 0xFF) * cur_inv_a) >> 8) + cur_pb);
            uint8_t ng = (uint8_t)(((((bg >> 8) & 0xFF) * cur_inv_a) >> 8) + cur_pg);
            uint8_t nr = (uint8_t)(((((bg >> 16) & 0xFF) * cur_inv_a) >> 8) + cur_pr);
            uint8_t na = (uint8_t)(((((bg >> 24) & 0xFF) * cur_inv_a) >> 8) + cur_a);

            buf[py * buf_w + px] = ((uint32_t)na << 24) | ((uint32_t)nr << 16) | ((uint32_t)ng << 8) | nb;
        }
    }
}

void draw_hover_capsule(uint32_t *buf, int buf_w, int x, int y, int w, int h, int r) {
    if (w <= 0 || h <= 0) {
        return;
    }

    draw_rounded_rect_avx2(buf, buf_w, x - 1, y, w + 2, h + 2, r + 1, Config::HoverShadowColorOuter);
    draw_rounded_rect_avx2(buf, buf_w, x, y + 1, w, h + 1, r + 1, Config::HoverShadowColorInner);

    draw_rounded_rect_avx2(buf, buf_w, x, y, w, h, r, Config::HoverCapsuleColor);
}