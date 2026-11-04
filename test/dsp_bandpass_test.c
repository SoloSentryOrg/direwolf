/* SPDX-License-Identifier: GPL-2.0-or-later */
#if defined(__FAST_MATH__) || defined(_M_FP_FAST) || \
    (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0)
#error Bandpass validation tests require strict floating-point semantics.
#endif
#include "direwolf.h"
#include "audio.h"
#include "fsk_demod_state.h"
#include "dsp.h"
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
  float coefficients[MAX_FILTER_SIZE];
  const int taps[] = {3, 31, 80, 101, MAX_FILTER_SIZE};
  const float bands[][2] = {{0.05f, 0.2f}, {0.1f, 0.4f}, {-0.02f, 0.16f}};
  unsigned int t, b;
  int w, j;
  for (w = BP_WINDOW_TRUNCATED; w <= BP_WINDOW_FLATTOP; ++w) {
    for (t = 0; t < sizeof(taps)/sizeof(taps[0]); ++t) {
      for (b = 0; b < sizeof(bands)/sizeof(bands[0]); ++b) {
        double response = 0;
        double frequency = M_PI * ((double)bands[b][0] + bands[b][1]);
        assert(gen_bandpass(bands[b][0], bands[b][1], coefficients, taps[t], (bp_window_t)w) == 0);
        for (j = 0; j < taps[t]; ++j) {
          assert(isfinite(coefficients[j]));
          assert(fabs((double)coefficients[j] - coefficients[taps[t]-1-j]) < 1e-6);
          response += coefficients[j] * cos((j - 0.5*(taps[t]-1))*frequency);
        }
        /* Preserve the existing normalization convention, including its 2x
         * divisor. Changing receiver gain is outside this repair. */
        assert(fabs(response - 0.5) < 1e-5);
      }
    }
  }
  assert(gen_bandpass(NAN, 0.2f, coefficients, 31, BP_WINDOW_HAMMING) != 0);
  assert(gen_bandpass(0.1f, INFINITY, coefficients, 31, BP_WINDOW_HAMMING) != 0);
  assert(gen_bandpass(0.1f, 0.1f, coefficients, 31, BP_WINDOW_HAMMING) != 0);
  assert(gen_bandpass(0.2f, 0.1f, coefficients, 31, BP_WINDOW_HAMMING) != 0);
  assert(gen_bandpass(-FLT_MAX, FLT_MAX, coefficients, 31, BP_WINDOW_HAMMING) != 0);
  coefficients[0] = 123;
  assert(gen_bandpass(0.1f, 0.2f, coefficients, 2, BP_WINDOW_HAMMING) != 0);
  assert(gen_bandpass(0.1f, 0.2f, coefficients, MAX_FILTER_SIZE+1, BP_WINDOW_HAMMING) != 0);
  assert(gen_bandpass(0.1f, 0.2f, coefficients, INT_MIN, BP_WINDOW_HAMMING) != 0);
  assert(gen_bandpass(0.1f, 0.2f, coefficients, INT_MAX, BP_WINDOW_HAMMING) != 0);
  assert(gen_bandpass(0.1f, 0.2f, NULL, 31, BP_WINDOW_HAMMING) != 0);
  assert(coefficients[0] == 123);
  puts("bandpass finite response and invalid-construction checks passed");
  return 0;
}
