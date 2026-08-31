#ifndef BL_PIECE_ID_FREQ_CALC_H
#define BL_PIECE_ID_FREQ_CALC_H

#include <cstdint>

struct PllTccConfig {
  uint8_t gclkDiv = 0;
  uint8_t ldrFrac = 0;
  uint16_t ldrWhole = 0;
  uint32_t tccCnt = 0;
  int32_t error = 0;

  // Allows functions returning this type to be constexpr
  constexpr PllTccConfig() {}
};

PllTccConfig getPllTccConfigForFreq(uint32_t targetFreq);

#endif // BL_PIECE_ID_FREQ_CALC_H
