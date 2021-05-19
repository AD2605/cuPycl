#include <cstdlib>
#include <iostream>
#include <cstdlib>
#include <immintrin.h>

#include "Imaging.h"


#define PRECISION_BITS 7
#define UINT8 unsigned char
#define UINT32 unsigned INT32

#define

typedef struct{
    UINT8 r;
    UINT8 g;
    UINT8 b;
    UINT8 a;
}rgba8 ;

void ImagingAlphaComposite(Imaging* imDst, Imaging* imSrc, Imaging* imOut){

    char* num_threads = std::getenv("PIL_NUM_THREADS");
    if (num_threads == nullptr){
        throw std::invalid_argument("Environment Variable PIL_NUM_THREADS not defined");
    }

    int num_threads_value = std::atoi(num_threads);

    int x, y;
    int xsize = (*imDst)->xsize();

    __m128i mm_max_alpha = _mm_set1_epi32(255);
    __m128i mm_max_alpha2 = _mm_set1_epi32(255 * 255);
    
    __m128i mm_zero = _mm_setzero_si128();
    __m128i mm_half = _mm_set1_epi16(128);
    __m128i mm_get_lo = _mm_set_epi8(-1,-1, 5,4, 5,4, 5,4, -1,-1, 1,0, 1,0, 1,0);
    __m128i mm_get_hi = _mm_set_epi8(-1,-1, 13,12, 13,12, 13,12, -1,-1, 9,8, 9,8, 9,8);


#if defined(__AVX2__)
    __m256i vmm_max_alpha = _mm256_set1_epi32(255);
    __m256i vmm_max_alpha2 = _mm256_set1_epi32(255 * 255);
    __m256i vmm_zero = _mm256_setzero_si256();
    __m256i vmm_half = _mm256_set1_epi16(128);
    __m256i vmm_get_lo = _mm256_set_epi8(
        -1,-1, 5,4, 5,4, 5,4, -1,-1, 1,0, 1,0, 1,0,
        -1,-1, 5,4, 5,4, 5,4, -1,-1, 1,0, 1,0, 1,0);
    __m256i vmm_get_hi = _mm256_set_epi8(
        -1,-1, 13,12, 13,12, 13,12, -1,-1, 9,8, 9,8, 9,8,
        -1,-1, 13,12, 13,12, 13,12, -1,-1, 9,8, 9,8, 9,8);
#endif



#pragma omp parallel for num_threads(num_threads_value)
    for (y = 0; y < imDst->ysize; y++) {
    rgba8* dst = (rgba8*) (*imDst)->image[y];
    rgba8* src = (rgba8*) (*imSrc)->image[y];
    rgba8* out = (rgba8*) (*imOut)->image[y];

    x = 0;

#if defined(__AVX2__)

        #define MM_SHIFTDIV255_epi16(src)\
            _mm256_srli_epi16(_mm256_add_epi16(src, _mm256_srli_epi16(src, 8)), 8)

        for (; x < xsize - 7; x += 8) {
            __m256i mm_dst, mm_dst_lo, mm_dst_hi;
            __m256i mm_src, mm_src_lo, mm_src_hi;
            __m256i mm_dst_a, mm_src_a, mm_out_a, mm_blend;
            __m256i mm_coef1, mm_coef2, mm_out_lo, mm_out_hi;

            mm_dst = _mm256_loadu_si256((__m256i *) &dst[x]);
            mm_dst_lo = _mm256_unpacklo_epi8(mm_dst, vmm_zero);
            mm_dst_hi = _mm256_unpackhi_epi8(mm_dst, vmm_zero);
            mm_src = _mm256_loadu_si256((__m256i *) &src[x]);
            mm_src_lo = _mm256_unpacklo_epi8(mm_src, vmm_zero);
            mm_src_hi = _mm256_unpackhi_epi8(mm_src, vmm_zero);

            mm_dst_a = _mm256_srli_epi32(mm_dst, 24);
            mm_src_a = _mm256_srli_epi32(mm_src, 24);

            mm_blend = _mm256_mullo_epi16(mm_dst_a, _mm256_sub_epi32(vmm_max_alpha, mm_src_a));
            
            mm_out_a = _mm256_add_epi32(_mm256_mullo_epi16(mm_src_a, vmm_max_alpha), mm_blend);
            mm_coef1 = _mm256_mullo_epi32(mm_src_a, vmm_max_alpha2);
            // 8 bits split
            mm_coef1 = _mm256_cvtps_epi32(
                _mm256_mul_ps(_mm256_cvtepi32_ps(mm_coef1),
                              _mm256_rcp_ps(_mm256_cvtepi32_ps(mm_out_a)))
            );
            // 8 bits other spllit
            mm_coef2 = _mm256_sub_epi32(vmm_max_alpha, mm_coef1);

            mm_out_lo = _mm256_add_epi16(
                _mm256_mullo_epi16(mm_src_lo, _mm256_shuffle_epi8(mm_coef1, vmm_get_lo)),
                _mm256_mullo_epi16(mm_dst_lo, _mm256_shuffle_epi8(mm_coef2, vmm_get_lo)));
            mm_out_lo = _mm256_or_si256(mm_out_lo, _mm256_slli_epi64(
                _mm256_unpacklo_epi32(mm_out_a, vmm_zero), 48));
            mm_out_lo = _mm256_add_epi16(mm_out_lo, vmm_half);
            mm_out_lo = MM_SHIFTDIV255_epi16(mm_out_lo);

            mm_out_hi = _mm256_add_epi16(
                _mm256_mullo_epi16(mm_src_hi, _mm256_shuffle_epi8(mm_coef1, vmm_get_hi)),
                _mm256_mullo_epi16(mm_dst_hi, _mm256_shuffle_epi8(mm_coef2, vmm_get_hi)));
            mm_out_hi = _mm256_or_si256(mm_out_hi, _mm256_slli_epi64(
                _mm256_unpackhi_epi32(mm_out_a, vmm_zero), 48));
            mm_out_hi = _mm256_add_epi16(mm_out_hi, vmm_half);
            mm_out_hi = MM_SHIFTDIV255_epi16(mm_out_hi);

            _mm256_storeu_si256((__m256i *) &out[x],
                _mm256_packus_epi16(mm_out_lo, mm_out_hi));
        }

        #undef MM_SHIFTDIV255_epi16

#endif

        #define MM_SHIFTDIV255_epi16(src)\
            _mm_srli_epi16(_mm_add_epi16(src, _mm_srli_epi16(src, 8)), 8)
        
        for (; x < xsize - 3; x += 4) {
            __m128i mm_dst, mm_dst_lo, mm_dst_hi;
            __m128i mm_src, mm_src_hi, mm_src_lo;
            __m128i mm_dst_a, mm_src_a, mm_out_a, mm_blend;
            __m128i mm_coef1, mm_coef2, mm_out_lo, mm_out_hi;

            
            mm_dst = _mm_loadu_si128((__m128i *) &dst[x]);
            
            mm_dst_lo = _mm_unpacklo_epi8(mm_dst, mm_zero);
            
            mm_dst_hi = _mm_unpackhi_epi8(mm_dst, mm_zero);
            
            mm_src = _mm_loadu_si128((__m128i *) &src[x]);
            mm_src_lo = _mm_unpacklo_epi8(mm_src, mm_zero);
            mm_src_hi = _mm_unpackhi_epi8(mm_src, mm_zero);

            
            mm_dst_a = _mm_srli_epi32(mm_dst, 24);
            mm_src_a = _mm_srli_epi32(mm_src, 24);

            mm_blend = _mm_mullo_epi16(mm_dst_a, _mm_sub_epi32(mm_max_alpha, mm_src_a));
            mm_out_a = _mm_add_epi32(_mm_mullo_epi16(mm_src_a, mm_max_alpha), mm_blend);
            
            mm_coef1 = _mm_mullo_epi32(mm_src_a, mm_max_alpha2);
            
            mm_coef1 = _mm_cvtps_epi32(
                _mm_mul_ps(_mm_cvtepi32_ps(mm_coef1),
                           _mm_rcp_ps(_mm_cvtepi32_ps(mm_out_a)))
            );
            mm_coef2 = _mm_sub_epi32(mm_max_alpha, mm_coef1);

            mm_out_lo = _mm_add_epi16(
                _mm_mullo_epi16(mm_src_lo, _mm_shuffle_epi8(mm_coef1, mm_get_lo)),
                _mm_mullo_epi16(mm_dst_lo, _mm_shuffle_epi8(mm_coef2, mm_get_lo)));
            mm_out_lo = _mm_or_si128(mm_out_lo, _mm_slli_epi64(
                _mm_unpacklo_epi32(mm_out_a, mm_zero), 48));
            mm_out_lo = _mm_add_epi16(mm_out_lo, mm_half);
            mm_out_lo = MM_SHIFTDIV255_epi16(mm_out_lo);

            mm_out_hi = _mm_add_epi16(
                _mm_mullo_epi16(mm_src_hi, _mm_shuffle_epi8(mm_coef1, mm_get_hi)),
                _mm_mullo_epi16(mm_dst_hi, _mm_shuffle_epi8(mm_coef2, mm_get_hi)));
            mm_out_hi = _mm_or_si128(mm_out_hi, _mm_slli_epi64(
                _mm_unpackhi_epi32(mm_out_a, mm_zero), 48));
            mm_out_hi = _mm_add_epi16(mm_out_hi, mm_half);
            mm_out_hi = MM_SHIFTDIV255_epi16(mm_out_hi);

            _mm_storeu_si128((__m128i *) &out[x],
                _mm_packus_epi16(mm_out_lo, mm_out_hi));
        }

        #undef MM_SHIFTDIV255_epi16

        for (; x < xsize; x += 1) {
            if (src[x].a == 0) {
                // Copy 4 bytes at once.
                out[x] = dst[x];
            } else {
                // Integer implementation with increased precision.
                // Each variable has extra meaningful bits.
                // Divisions are rounded.

                UINT32 tmpr, tmpg, tmpb;
                UINT32 blend = dst[x].a * (255 - src[x].a);
                UINT32 outa255 = src[x].a * 255 + blend;
                // There we use 7 bits for precision.
                // We could use more, but we go beyond 32 bits.
                UINT32 coef1 = src[x].a * 255 * 255 * (1<<PRECISION_BITS) / outa255;
                UINT32 coef2 = 255 * (1<<PRECISION_BITS) - coef1;

                tmpr = src[x].r * coef1 + dst[x].r * coef2;
                tmpg = src[x].g * coef1 + dst[x].g * coef2;
                tmpb = src[x].b * coef1 + dst[x].b * coef2;
                out[x].r = SHIFTFORDIV255(tmpr + (0x80<<PRECISION_BITS)) >> PRECISION_BITS;
                out[x].g = SHIFTFORDIV255(tmpg + (0x80<<PRECISION_BITS)) >> PRECISION_BITS;
                out[x].b = SHIFTFORDIV255(tmpb + (0x80<<PRECISION_BITS)) >> PRECISION_BITS;
                out[x].a = SHIFTFORDIV255(outa255 + 0x80);
            }
        }

    }
}