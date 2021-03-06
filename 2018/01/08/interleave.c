// gcc -O3 -o interleave interleave.c -march=native  && ./interleave
#include "benchmark.h"
#include <assert.h>
#include <immintrin.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stddef.h>
#include <stdint.h>

static inline uint64_t interleave_uint32_with_zeros(uint32_t input)  {
    uint64_t word = input;
    word = (word ^ (word << 16)) & 0x0000ffff0000ffff;
    word = (word ^ (word << 8 )) & 0x00ff00ff00ff00ff;
    word = (word ^ (word << 4 )) & 0x0f0f0f0f0f0f0f0f;
    word = (word ^ (word << 2 )) & 0x3333333333333333;
    word = (word ^ (word << 1 )) & 0x5555555555555555;
    return word;
}



static inline uint32_t deinterleave_lowuint32(uint64_t word) {
    word &= 0x5555555555555555;
    word = (word ^ (word >> 1 )) & 0x3333333333333333;
    word = (word ^ (word >> 2 )) & 0x0f0f0f0f0f0f0f0f;
    word = (word ^ (word >> 4 )) & 0x00ff00ff00ff00ff;
    word = (word ^ (word >> 8 )) & 0x0000ffff0000ffff;
    word = (word ^ (word >> 16)) & 0x00000000ffffffff;
    return (uint32_t)word;
}

typedef struct  {
  uint32_t x;
  uint32_t y;
} uint32_2;


uint64_t interleave(uint32_2 input)  {
    return interleave_uint32_with_zeros(input.x) | (interleave_uint32_with_zeros(input.y) << 1);
}

void interleave_array(uint32_2 *input, size_t length, uint64_t * out)  {
  for(size_t i = 0; i < length; i++) {
    out[i] = interleave(input[i]);
  }
}

_Static_assert (sizeof(uint32_2) == sizeof(uint64_t), "assert1");

uint64_t interleave_clmul(uint32_2 input) {
  __m128i b = _mm_loadl_epi64((__m128i*)&input);
  __m128i expanded = _mm_clmulepi64_si128(b,b,0);
  uint64_t buffer[2];
   _mm_storeu_si128 ((__m128i*) buffer, expanded);
  return buffer[0] | (buffer[1]<<1);
}

void interleave_clmul_array(uint32_2 *input, size_t length, uint64_t * out)  {
  for(size_t i = 0; i < length; i++) {
    out[i] = interleave_clmul(input[i]);
  }
}
uint32_2 deinterleave(uint64_t input)  {
  uint32_2 answer;
  answer.x = deinterleave_lowuint32(input);
  answer.y = deinterleave_lowuint32(input>>1);
  return answer;
}

void deinterleave_array(uint64_t *input, size_t length, uint32_2 * out)  {
  for(size_t i = 0; i < length; i++) {
    out[i] = deinterleave(input[i]);
  }
}



// from https://github.com/yinqiwen/geohash-int/
static inline uint64_t interleave64(uint32_t xlo, uint32_t ylo) {
    static const uint64_t B[] =
        { 0x5555555555555555, 0x3333333333333333, 0x0F0F0F0F0F0F0F0F, 0x00FF00FF00FF00FF, 0x0000FFFF0000FFFF };
    static const unsigned int S[] =
        { 1, 2, 4, 8, 16 };

    uint64_t x = xlo; // Interleave lower  bits of x and y, so the bits of x
    uint64_t y = ylo; // are in the even positions and bits from y in the odd;
    // x and y must initially be less than 2**32.

    x = (x | (x << S[4])) & B[4];
    y = (y | (y << S[4])) & B[4];

    x = (x | (x << S[3])) & B[3];
    y = (y | (y << S[3])) & B[3];

    x = (x | (x << S[2])) & B[2];
    y = (y | (y << S[2])) & B[2];

    x = (x | (x << S[1])) & B[1];
    y = (y | (y << S[1])) & B[1];

    x = (x | (x << S[0])) & B[0];
    y = (y | (y << S[0])) & B[0];

    return x | (y << 1);
}

// from https://github.com/yinqiwen/geohash-int/
static inline uint64_t deinterleave64(uint64_t interleaved) {
    static const uint64_t B[] =
        { 0x5555555555555555, 0x3333333333333333, 0x0F0F0F0F0F0F0F0F, 0x00FF00FF00FF00FF, 0x0000FFFF0000FFFF,
                0x00000000FFFFFFFF };
    static const unsigned int S[] =
        { 0, 1, 2, 4, 8, 16 };

    uint64_t x = interleaved; ///reverse the interleave process
    uint64_t y = interleaved >> 1;

    x = (x | (x >> S[0])) & B[0];
    y = (y | (y >> S[0])) & B[0];

    x = (x | (x >> S[1])) & B[1];
    y = (y | (y >> S[1])) & B[1];

    x = (x | (x >> S[2])) & B[2];
    y = (y | (y >> S[2])) & B[2];

    x = (x | (x >> S[3])) & B[3];
    y = (y | (y >> S[3])) & B[3];

    x = (x | (x >> S[4])) & B[4];
    y = (y | (y >> S[4])) & B[4];

    x = (x | (x >> S[5])) & B[5];
    y = (y | (y >> S[5])) & B[5];

    return x | (y << 32);
}


void interleave64_array(uint32_2 *input, size_t length, uint64_t * out)  {
  for(size_t i = 0; i < length; i++) {
    out[i] = interleave64(input[i].x,input[i].y);
  }
}

void deinterleave64_array(uint64_t *input, size_t length, uint32_2 * out)  {
  for(size_t i = 0; i < length; i++) {
    uint64_t o = deinterleave64(input[i]);
    memcpy(&(out[i]),&o,sizeof(uint64_t));
  }
}

static inline uint64_t interleave_uint32_with_zeros_pdep(uint32_t input)  {
    return _pdep_u64(input, 0x5555555555555555);
}

static inline uint32_t deinterleave_lowuint32_pext(uint64_t word) {
    return (uint32_t)_pext_u64(word, 0x5555555555555555);
}

uint64_t interleave_pdep(uint32_2 input)  {
    return _pdep_u64(input.x, 0x5555555555555555) | _pdep_u64(input.y,0xaaaaaaaaaaaaaaaa);
}


void interleave_array_pdep(uint32_2 *input, size_t length, uint64_t * out)  {
  for(size_t i = 0; i < length; i++) {
    out[i] = interleave_pdep(input[i]);
  }
}

uint32_2 deinterleave_pext(uint64_t input)  {
  uint32_2 answer;
  answer.x = _pext_u64(input, 0x5555555555555555);
  answer.y = _pext_u64(input, 0xaaaaaaaaaaaaaaaa);
  return answer;
}

void deinterleave_array_pext(uint64_t *input, size_t length, uint32_2 * out)  {
  for(size_t i = 0; i < length; i++) {
    out[i] = deinterleave_pext(input[i]);
  }
}

void demo(size_t N) {
  printf("N = %zu \n", N);
  uint32_2* array =  (uint32_2 *)malloc(N * sizeof(uint32_2));
  uint64_t* storedarray =  (uint64_t *)malloc(N * sizeof(uint64_t));
  for(uint32_t k = 0; k < N; k++) {
    array[k].x = k;
    array[k].y = k+1;
  }
  int repeat = 500;
  BEST_TIME_NOCHECK(interleave_array(array,N,storedarray),, repeat, N, true);
  BEST_TIME_NOCHECK(deinterleave_array(storedarray,N,array),, repeat, N, true);
  for(uint32_t k = 0; k < N; k++) {
    assert(array[k].x == k);
    assert(array[k].y == k+1);
  }
  /*BEST_TIME_NOCHECK(interleave64_array(array,N,storedarray),, repeat, N, true);
  BEST_TIME_NOCHECK(deinterleave64_array(storedarray,N,array),, repeat, N, true);
  for(uint32_t k = 0; k < N; k++) {
    assert(array[k].x == k);
    assert(array[k].y == k+1);
  }
*/
/*  BEST_TIME_NOCHECK(interleave_array_pdep(array,N,storedarray),, repeat, N, true);
  BEST_TIME_NOCHECK(deinterleave_array_pext(storedarray,N,array),, repeat, N, true);
  for(uint32_t k = 0; k < N; k++) {
    assert(array[k].x == k);
    assert(array[k].y == k+1);
  }
  BEST_TIME_NOCHECK(interleave_clmul_array(array,N,storedarray),, repeat, N, true);

*/
  free(array);
  free(storedarray);

}
int main() {
  demo(1000);
  return 0;
}
