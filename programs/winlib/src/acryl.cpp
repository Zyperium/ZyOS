#define _MM_MALLOC_H_INCLUDED
#include <xmmintrin.h>
#include <emmintrin.h>
#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

void apply_acrylic_finish(uint32_t *taskbar_img, int width, int height, int is_dark_mode) {
    if (!taskbar_img || width <= 0 || height <= 0) {
        return;
    }

    uint32_t *top_row = taskbar_img;
    for (int x = 0; x < width; x++) {
        uint32_t pixel = top_row[x];

        uint32_t r = (pixel >> 16) & 0xFF;
        uint32_t g = (pixel >> 8) & 0xFF;
        uint32_t b = pixel & 0xFF;

        r = r + ((255 - r) * 76 >> 8);
        g = g + ((255 - g) * 76 >> 8);
        b = b + ((255 - b) * 76 >> 8);

        top_row[x] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
    }

    if (height > 1) {
        uint32_t *second_row = taskbar_img + width;
        for (int x = 0; x < width; x++) {
            uint32_t pixel = second_row[x];
            uint32_t r = (pixel >> 16) & 0xFF;
            uint32_t g = (pixel >> 8) & 0xFF;
            uint32_t b = pixel & 0xFF;

            r = r + ((255 - r) * 25 >> 8);
            g = g + ((255 - g) * 25 >> 8);
            b = b + ((255 - b) * 25 >> 8);

            second_row[x] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
        }
    }

    int total_pixels = width * height;
    int i = 0;

    if (is_dark_mode) {
        __m128i mult_vec = _mm_set1_epi16(166);
        __m128i add_vec  = _mm_set1_epi16(24);
        __m128i zero     = _mm_setzero_si128();

        for (; i <= total_pixels - 4; i += 4) {
            __m128i pixels = _mm_loadu_si128((__m128i *)(taskbar_img + i));

            __m128i lo = _mm_unpacklo_epi8(pixels, zero);
            __m128i hi = _mm_unpackhi_epi8(pixels, zero);

            lo = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(lo, mult_vec), 8), add_vec);
            hi = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(hi, mult_vec), 8), add_vec);

            __m128i result = _mm_packus_epi16(lo, hi);
            _mm_storeu_si128((__m128i *)(taskbar_img + i), result);
        }
    } else {
        __m128i mult_vec = _mm_set1_epi16(205);
        __m128i add_vec  = _mm_set1_epi16(45);
        __m128i zero     = _mm_setzero_si128();

        for (; i <= total_pixels - 4; i += 4) {
            __m128i pixels = _mm_loadu_si128((__m128i *)(taskbar_img + i));

            __m128i lo = _mm_unpacklo_epi8(pixels, zero);
            __m128i hi = _mm_unpackhi_epi8(pixels, zero);

            lo = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(lo, mult_vec), 8), add_vec);
            hi = _mm_add_epi16(_mm_srli_epi16(_mm_mullo_epi16(hi, mult_vec), 8), add_vec);

            __m128i result = _mm_packus_epi16(lo, hi);
            _mm_storeu_si128((__m128i *)(taskbar_img + i), result);
        }
    }

    for (; i < total_pixels; i++) {
        uint32_t pixel = taskbar_img[i];
        uint32_t r = (pixel >> 16) & 0xFF;
        uint32_t g = (pixel >> 8) & 0xFF;
        uint32_t b = pixel & 0xFF;

        if (is_dark_mode) {
            r = ((r * 166) >> 8) + 24;
            g = ((g * 166) >> 8) + 24;
            b = ((b * 166) >> 8) + 24;
        } else {
            r = ((r * 205) >> 8) + 45;
            g = ((g * 205) >> 8) + 45;
            b = ((b * 205) >> 8) + 45;
        }

        taskbar_img[i] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
    }
}

void memcpy32_alpha(uint32_t *dest, const uint32_t *src, size_t count) {
    size_t i = 0;

    const __m256i v_zero = _mm256_setzero_si256();
    const __m256i v_255  = _mm256_set1_epi16(255);

    for (; i + 8 <= count; i += 8) {
        __m256i s = _mm256_loadu_si256((const __m256i *)(src + i));
        __m256i d = _mm256_loadu_si256((const __m256i *)(dest + i));
        __m256i s_lo = _mm256_unpacklo_epi8(s, v_zero);
        __m256i s_hi = _mm256_unpackhi_epi8(s, v_zero);

        __m256i d_lo = _mm256_unpacklo_epi8(d, v_zero);
        __m256i d_hi = _mm256_unpackhi_epi8(d, v_zero);

        __m256i alpha_shuffle = _mm256_setr_epi8(
            6,7, 6,7, 6,7, 6,7,   14,15, 14,15, 14,15, 14,15,
            6,7, 6,7, 6,7, 6,7,   14,15, 14,15, 14,15, 14,15
        );

        __m256i a_lo = _mm256_shuffle_epi8(s_lo, alpha_shuffle);
        __m256i a_hi = _mm256_shuffle_epi8(s_hi, alpha_shuffle);

        __m256i inv_a_lo = _mm256_sub_epi16(v_255, a_lo);
        __m256i inv_a_hi = _mm256_sub_epi16(v_255, a_hi);

        __m256i blend_lo = _mm256_srli_epi16(
            _mm256_add_epi16(
                _mm256_mullo_epi16(s_lo, a_lo),
                _mm256_mullo_epi16(d_lo, inv_a_lo)
            ), 8
        );

        __m256i blend_hi = _mm256_srli_epi16(
            _mm256_add_epi16(
                _mm256_mullo_epi16(s_hi, a_hi),
                _mm256_mullo_epi16(d_hi, inv_a_hi)
            ), 8
        );

        __m256i result = _mm256_packus_epi16(blend_lo, blend_hi);

        _mm256_storeu_si256((__m256i *)(dest + i), result);
    }

    for (; i < count; ++i) {
        uint32_t s = src[i];
        uint32_t d = dest[i];

        uint32_t a = (s >> 24) & 0xFF;
        uint32_t inv_a = 255 - a;

        uint32_t r = (((s >> 16 & 0xFF) * a) + ((d >> 16 & 0xFF) * inv_a)) >> 8;
        uint32_t g = (((s >> 8  & 0xFF) * a) + ((d >> 8  & 0xFF) * inv_a)) >> 8;
        uint32_t b = (((s       & 0xFF) * a) + ((d       & 0xFF) * inv_a)) >> 8;

        dest[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}