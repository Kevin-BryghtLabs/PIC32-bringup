#include "bl_stat.h"

#include <string.h>

void statInit(IntStat * stat) {
  memset(stat, 0, sizeof(stat[0]));
}

void statReset(IntStat * stat) {
  memset(stat, 0, sizeof(stat[0]));
}

// Samples in the stat first have an offset applied to keep the sums small.
// The offset is removed at the end to provide an accurate value:
//
//  - s1 is a running sum: sum(Sn - K)
//  - s2 is a running sum of the squares: sum((Sn - K)^2)
//
void statAdd(IntStat * stat, int32_t sample) {
  if (stat->count == 0) {
    // Use the first sampled value as the offset
    stat->offset = sample;

    stat->min = sample;
    stat->max = sample;
  }
  else if (sample < stat->min) {
    stat->min = sample;
  }
  else if (sample > stat->max) {
    stat->max = sample;
  }

  int32_t delta = sample - stat->offset;
  stat->count++;
  stat->s1 += delta;
  stat->s2 += (uint64_t)((int64_t)delta * delta);
}

// s1 is sum(Sn - K)
// Mean is sum(Sn) / N
//
// Subtracting a K from each term means sum(Sn - K) is N * K smaller than the true sum.
// After dividing by N, just adding K corrects to the true mean:
//
//   sum(Sn)      sum(Sn - K) + N * K     sum(Sn - K)          s1
//   -------  =   -------------------  =  ----------- + K  =  ---- + k
//      N                 N                    N               N
//
int32_t statGetMean(const IntStat * stat) {
  if (stat->count == 0) {
    return 0;
  }

  return stat->s1 / stat->count + stat->offset;
}

// Sample Variance is:
//
//    sum( (sample - mean)^2 )
//    ------------------------
//             n - 1
//
// Defining:
//   - Xn = sample
//   - N  = Sample count
//   - Dn = Xn - K   ("delta")
//   - x^ = mean  (x bar)
//   - D^ = x^ - K (D bar)
//   - s1 = sum(Xn - K) = sum(Dn)
//   - s2 = sum((Xn - K)^2) = sum(Dn^2)
//
// Then:
//   x^ = sum(Xn - K) / N + K = sum(Dn) / N + K = s1 / N + K
//
//   D^ = x^ - K = s1 / N    (1)
//
// variance doesn't change when an offset is applied:
//   Dn - D^ = (Xn - K) - (x^ - K) = Xn - x^ -K + K = Xn - x^
//
// So rewrite the original formula as:
//
//     sum((Dn - D^)^2)
//     ----------------
//           n - 1
//
// Expand the numerator:
//
//   sum(Dn^2) - 2 * sum(Dn * D^) + sum((D^)^2)
//
// Substitute definition of s2 and pull out constant terms from sums:
//   s2 - 2 * D^ * sum(Dn) + N * (D^)^2
//
// Substitute definition of s1:
//   s2 - 2 * D^ * s1 + N * (D^)^2
//
// Substitute D^ from (1):
//   s2 - 2 * (s1 / N) * s1  + N * (s1 / N)^2
//
//   s2 - 2 * (s1^2 / N) + (s1^2 / N)
//
//   s2 - s1^2 / N
//
// Formula for variance in terms of s1 and s2:
//
//  Var = (s2 - s1^2 / N) / (N - 1)
//
// ==================================================
//
// s1 and s2 are 64-bit values.  To avoid 128-bit math without overflow,
// Claude suggested this approach:
//
// Define:
//   - a = abs(s1)
//
// Using the absolute value keeps every term non-negative, so the math stays
// in uint64_t: no signed-overflow UB, and the remainder is 0 <= r < N.
//
//  s1^2 / N  = a^2 / N
//
// Write a / N as a quotient and remainder:
//
//   a = q * N + r    where  q = a / N  (integer math)  and   r = a % N
//
// Expand and substitute for only one `a`:
//
//   a^2 / N
//
//   a * a / N
//
//   a * (q * N + r) / N
//
//   a * q + a * r / N
//
// Substitute a again into the second term:
//
//   a * q + (q * N + r) * r / N
//
//   a * q + q * r + r^2 / N
//
// So:
//
//   s1^2/N = a^2 / N = a * q + q * r + r^2 / N
//
// Filling this into our variance formula above, we compute
//
//   Var = (s2 - (a * q + q * r + r^2 / N)) / (N - 1)
//
// ** This is quite a bit more expensive than mean.  Only
//    compute it where the value is truly helpful.
uint32_t statGetVariance(const IntStat * stat) {
  if (stat->count <= 1) {
    return 0;
  }

  // Absolute value of s1
  uint64_t a = (stat->s1 < 0) ? -(uint64_t)stat->s1 : (uint64_t)stat->s1;

  // Quotient and remainder
  uint64_t q = a / stat->count;
  uint64_t r = a % stat->count;

  // Numerator of derived variance formula
  uint64_t numerator = stat->s2 - q * a - q * r - (r * r) / stat->count;

  // Full variance result
  return numerator / (stat->count - 1);
}

// Used to verify math on a list of values against Excel
#ifdef PC_TEST
#include <stdio.h>

int main() {
  IntStat stat;
  statInit(&stat);

  for (;;) {
    int val;
    int res = fscanf(stdin, "%d\n", &val);
    if (res < 1) {
      break;
    }
    statAdd(&stat, val);
  }

  printf("Count: %u\n", stat.count);
  printf("Min:   %d\n", stat.min);
  printf("Max:   %d\n", stat.max);
  printf("Mean:  %d\n", statGetMean(&stat));
  printf("Var:   %u\n", statGetVariance(&stat));

  return 0;
}
#endif
